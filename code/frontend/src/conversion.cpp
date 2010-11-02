/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please 
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details 
 * regarding third party software licenses.
 */

#include "conversion.h"

#include <sstream>
#include <memory>
#include <functional>

#include "utils/types_lenght.h"
#include "utils/source_locations.h"
#include "utils/dep_graph.h"

#include "analysis/loop_analyzer.h"
#include "analysis/expr_analysis.h"

#include "program.h"
#include "ast_node.h"
#include "types.h"
#include "statements.h"
#include "container_utils.h"
#include "lang_basic.h"
#include "numeric_cast.h"
#include "naming.h"
#include "ast_visitor.h"
#include "container_utils.h"
#include "logging.h"
#include "location.h"

#include "omp/omp_pragma.h"
#include "ocl/ocl_annotations.h"

#include "transform/node_replacer.h"

#include "clang/Basic/FileManager.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/TypeVisitor.h"

#include "clang/Index/Entity.h"
#include "clang/Index/Indexer.h"

#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <boost/algorithm/string.hpp>

using namespace boost;

using namespace clang;
using namespace insieme;
namespace fe = insieme::frontend;

#define GET_TYPE_PTR(type) (type)->getType().getTypePtr()

namespace {

//TODO put it into a class
void printErrorMsg(std::ostringstream& errMsg, const frontend::ClangCompiler& clangComp, const clang::Decl* decl) {

	SourceManager& manager = clangComp.getSourceManager();
    clang::SourceLocation errLoc = decl->getLocStart();
    errMsg << " at location (" << frontend::utils::Line(errLoc, manager) << ":" <<
            frontend::utils::Column(errLoc, manager) << ").\n";


    /*Crashes
    DiagnosticInfo di(&diag);
    tdc->HandleDiagnostic(Diagnostic::Level::Warning, di);*/

    clang::Preprocessor& pp =  clangComp.getPreprocessor();
    pp.Diag(errLoc, pp.getDiagnostics().getCustomDiagID(Diagnostic::Warning, errMsg.str()));
}

//------------------- StmtWrapper -------------------------------------------------------------
// Utility class used as a return type for the StmtVisitor. It can store a list of statement
// as conversion of a single C stmt can result in multiple IR statements.

struct StmtWrapper: public std::vector<core::StatementPtr>{
	StmtWrapper(): std::vector<core::StatementPtr>() { }
	StmtWrapper(const core::StatementPtr& stmt):  std::vector<core::StatementPtr>() { push_back(stmt); }

	core::StatementPtr getSingleStmt() const {
		assert(size() == 1 && "More than 1 statement present");
		return front();
	}

	bool isSingleStmt() const { return size() == 1; }
};

// Returns a string of the text within the source range of the input stream
std::string GetStringFromStream(const SourceManager& srcMgr, const SourceLocation& start) {
	// we use the getDecomposedSpellingLoc() method because in case we read macros values
	// we have to read the expanded value
	std::pair<FileID, unsigned> startLocInfo = srcMgr.getDecomposedSpellingLoc(start);
	llvm::StringRef startBuffer = srcMgr.getBufferData(startLocInfo.first);
	const char *strDataStart = startBuffer.begin() + startLocInfo.second;

	return string(strDataStart, clang::Lexer::MeasureTokenLength(srcMgr.getSpellingLoc(start), srcMgr, clang::LangOptions()));
}

// Tried to aggregate statements into a compound statement (if more than 1 statement is present)
core::StatementPtr tryAggregateStmts(const core::ASTBuilder& builder, const vector<core::StatementPtr>& stmtVect) {
	if( stmtVect.size() == 1 )
		return stmtVect.front();
	return builder.compoundStmt(stmtVect);
}

// in case the the last argument of the function is a var_arg, we try pack the exceeding arguments with the pack
// operation provided by the IR.
vector<core::ExpressionPtr> tryPack(const core::ASTBuilder& builder, core::FunctionTypePtr funcTy, const vector<core::ExpressionPtr>& args) {

	// check if the function type ends with a VAR_LIST type
	core::TupleTypePtr&& argTy = core::dynamic_pointer_cast<const core::TupleType>(funcTy->getArgumentType());
	assert(argTy && "Function argument is of not type TupleType");

	const core::TupleType::ElementTypeList& elements = argTy->getElementTypes();
	// if the tuple type is empty it means we cannot pack any of the arguments
	if( elements.empty() )
		return args;

	if(*elements.back() == *core::lang::TYPE_VAR_LIST_PTR) {
		vector<core::ExpressionPtr> ret;
		assert(args.size() >= elements.size()-1 && "Function called with fewer arguments than necessary");
		// last type is a var_list, we have to do the packing of arguments

		// we copy the first N-1 arguments, the remaining will be unpacked
		std::copy(args.begin(), args.begin()+elements.size()-1, std::back_inserter(ret));

		vector<core::ExpressionPtr> toPack;
		std::copy(args.begin()+elements.size()-1, args.end(), std::back_inserter(toPack));

		ret.push_back( builder.callExpr(core::lang::TYPE_VAR_LIST_PTR, core::lang::OP_VAR_LIST_PACK_PTR, toPack) ); //fixme
		return ret;
	}
	return args;
}

std::string getOperationType(const core::TypePtr& type) {
	using namespace core::lang;
	DVLOG(2) << type;
	if(isUIntType(*type))	return "uint";
	if(isIntType(*type)) 	return "int";
	if(isBoolType(*type))	return "bool";
	if(isRealType(*type))	return "real";
    if(isVectorType(*type)) {
        const core::VectorType* vt = dynamic_cast<const core::VectorType*>(&*type);

        const core::TypePtr ref = vt->getElementType();
        std::ostringstream ss;

        if(const core::RefType* subtype = dynamic_cast<const core::RefType*>(&*ref))
            ss << "vector<" << getOperationType(subtype->getElementType()) << ">";
        else
            ss << "vector<" << getOperationType(ref) << ">";

//        ss << "vector<" << getOperationType(vt->getElementType()) << ">";
        return ss.str();
    }
    // FIXME
    return "unit";
	// assert(false && "Type not supported");
}

core::ExpressionPtr tryDeref(const core::ASTBuilder& builder, const core::ExpressionPtr& expr) {
	if(core::RefTypePtr&& refTy = core::dynamic_pointer_cast<const core::RefType>(expr->getType())) {
		return builder.callExpr( refTy->getElementType(), core::lang::OP_REF_DEREF_PTR, toVector<core::ExpressionPtr>(expr) );
	}
	return expr;
}

// creates a function call from a list of expressions,
// usefull for implementing the semantics of ++ or -- or comma separated expressions in the IR
core::CallExprPtr createCallExpr(const core::ASTBuilder& builder, const std::vector<core::StatementPtr>& body, core::TypePtr retTy) {

	core::CompoundStmtPtr&& bodyStmt = builder.compoundStmt( body );
	// keeps the list variables used in the body
	insieme::frontend::analysis::VarRefFinder args(bodyStmt);

	core::TupleType::ElementTypeList elemTy;
	core::LambdaExpr::ParamList params;
	std::for_each(args.begin(), args.end(),
		[ &params, &elemTy, &builder, &bodyStmt] (const core::ExpressionPtr& curr) {
			const core::VariablePtr& bodyVar = core::dynamic_pointer_cast<const core::Variable>(curr);
			core::VariablePtr parmVar = builder.variable( bodyVar->getType() );
			params.push_back( parmVar );
			elemTy.push_back( parmVar->getType() );

			// we have to replace the variable of the body with the newly created parmVar
			bodyStmt = core::dynamic_pointer_cast<const core::CompoundStmt>( core::transform::replaceNode(builder, bodyStmt, bodyVar, parmVar, true) );
			assert(bodyStmt);
		}
	);

	// build the type of the function
	core::FunctionTypePtr funcTy = builder.functionType( builder.tupleType(elemTy), retTy);

	// build the expression body
	core::LambdaExprPtr retExpr = builder.lambdaExpr( funcTy, params, bodyStmt );
	return builder.callExpr( retTy, retExpr, std::vector<core::ExpressionPtr>(args.begin(), args.end()) );
}

} // End empty namespace


#define START_LOG_EXPR_CONVERSION(expr) \
	DVLOG(1) << "\n****************************************************************************************\n" \
			 << "Converting expression [class: '" << expr->getStmtClassName() << "']\n" \
			 << "-> at location: (" << utils::location(expr->getLocStart(), convFact.clangComp.getSourceManager()) << "): "; \
	if( VLOG_IS_ON(2) ) { \
		DVLOG(2) << "Dump of clang expression: \n" \
				 << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"; \
		expr->dump(); \
	}

#define END_LOG_EXPR_CONVERSION(expr) \
	DVLOG(1) << "Converted into IR expression: "; \
	DVLOG(1) << "\t" << *expr;

namespace insieme {
namespace frontend {
namespace conversion {

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//							ConversionContext
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Keeps all the information gathered during the conversion process.
// Maps for variable names, cached resolved function definitions and so on...
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct ConversionFactory::ConversionContext {

	// Maps Clang variable declarations (VarDecls and ParmVarDecls) to an
	// IR variable.
	typedef std::map<const clang::VarDecl*, core::VariablePtr> VarDeclMap;
	VarDeclMap varDeclMap;

	// Map for resolved lambda functions
	typedef std::map<const FunctionDecl*, core::ExpressionPtr> LambdaExprMap;
	LambdaExprMap lambdaExprCache;

	// this map keeps naming map between the functiondecl and the variable
	// introduced to represent it in the recursive definition
	// typedef std::map<const FunctionDecl*, core::VariablePtr> VarMap;
	// VarMap  varMap;

	// CallGraph for functions, used to resolved eventual recursive functions
	utils::DependencyGraph<const FunctionDecl*> funcDepGraph;

	// Maps a function with the variable which has been introduced to represent
	// the function in the recursive definition
	typedef std::map<const FunctionDecl*, core::VariablePtr> RecVarExprMap;
	RecVarExprMap recVarExprMap;

	bool isRecSubFunc;
	bool isResolvingRecFuncBody;
	core::VariablePtr currVar;

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// 						Recursive Type resolution
	utils::DependencyGraph<const Type*> typeGraph;

	typedef std::map<const Type*, core::TypeVariablePtr> TypeRecVarMap;
	TypeRecVarMap recVarMap;
	bool isRecSubType;

	typedef std::map<const Type*, core::TypePtr> RecTypeMap;
	RecTypeMap recTypeCache;

	bool isResolvingFunctionType;

	ConversionContext(): isRecSubFunc(false), isResolvingRecFuncBody(false), isRecSubType(false), isResolvingFunctionType(false) { }
};

//#############################################################################
//
//							CLANG EXPRESSION CONVERTER
//
//############################################################################
class ConversionFactory::ClangExprConverter: public StmtVisitor<ClangExprConverter, core::ExpressionPtr> {
	ConversionFactory& convFact;
	ConversionContext& ctx;
public:
	ClangExprConverter(ConversionFactory& convFact): convFact(convFact), ctx(*convFact.ctx) { }

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								INTEGER LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitIntegerLiteral(clang::IntegerLiteral* intLit) {
		START_LOG_EXPR_CONVERSION(intLit);
		core::ExpressionPtr&& retExpr =
			convFact.builder.literal(
				// retrieve the string representation from the source code
				GetStringFromStream( convFact.clangComp.getSourceManager(), intLit->getExprLoc()),
				convFact.convertType( GET_TYPE_PTR(intLit) )
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								FLOATING LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitFloatingLiteral(clang::FloatingLiteral* floatLit) {
		START_LOG_EXPR_CONVERSION(floatLit);
		core::ExpressionPtr&& retExpr =
			// retrieve the string representation from the source code
			convFact.builder.literal(
				GetStringFromStream( convFact.clangComp.getSourceManager(), floatLit->getExprLoc()),
				convFact.convertType( GET_TYPE_PTR(floatLit) )
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								CHARACTER LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCharacterLiteral(CharacterLiteral* charLit) {
		START_LOG_EXPR_CONVERSION(charLit);
		core::ExpressionPtr&& retExpr =
			convFact.builder.literal(
				// retrieve the string representation from the source code
				GetStringFromStream(convFact.clangComp.getSourceManager(), charLit->getExprLoc()),
					(charLit->isWide() ? convFact.builder.genericType("wchar") : convFact.builder.genericType("char"))
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								STRING LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitStringLiteral(clang::StringLiteral* stringLit) {
		START_LOG_EXPR_CONVERSION(stringLit);
		core::ExpressionPtr&& retExpr =
			convFact.builder.literal(
				GetStringFromStream( convFact.clangComp.getSourceManager(), stringLit->getExprLoc()),
				convFact.builder.genericType(core::Identifier("string"))
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								CXX BOOLEAN LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCXXBoolLiteralExpr(CXXBoolLiteralExpr* boolLit) {
		START_LOG_EXPR_CONVERSION(boolLit);
		core::ExpressionPtr&& retExpr =
			// retrieve the string representation from the source code
			convFact.builder.literal(
				GetStringFromStream(convFact.clangComp.getSourceManager(), boolLit->getExprLoc()), core::lang::TYPE_BOOL_PTR
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							PARENTESIS EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitParenExpr(clang::ParenExpr* parExpr) {
		return Visit( parExpr->getSubExpr() );
	}

//	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	//						   IMPLICIT CAST EXPRESSION
//	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	core::ExpressionPtr VisitImplicitCastExpr(clang::ImplicitCastExpr* implCastExpr) {
//		START_LOG_EXPR_CONVERSION(implCastExpr);
////		const core::TypePtr& type = convFact.convertType( GET_TYPE_PTR(castExpr) );
//		core::ExpressionPtr&& subExpr = Visit(implCastExpr->getSubExpr());
////		core::ExpressionPtr&& retExpr = convFact.builder.castExpr( type, subExpr );
//		END_LOG_EXPR_CONVERSION(subExpr);
//		return subExpr;
//	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								CAST EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCastExpr(clang::CastExpr* castExpr) {
		START_LOG_EXPR_CONVERSION(castExpr);
		const core::TypePtr& type = convFact.convertType( GET_TYPE_PTR(castExpr) );
		core::ExpressionPtr&& subExpr = Visit(castExpr->getSubExpr());
		core::ExpressionPtr&& retExpr = convFact.builder.castExpr( type, subExpr );
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							FUNCTION CALL EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCallExpr(clang::CallExpr* callExpr) {
		START_LOG_EXPR_CONVERSION(callExpr);
		if( FunctionDecl* funcDecl = dyn_cast<FunctionDecl>(callExpr->getDirectCallee()) ) {
			const core::ASTBuilder& builder = convFact.builder;

			// collects the type of each argument of the expression
			vector<core::ExpressionPtr> args;
			std::for_each(callExpr->arg_begin(), callExpr->arg_end(),
				[ &args, this ] (Expr* currArg) { args.push_back( this->Visit(currArg) ); }
			);

			core::FunctionTypePtr&& funcTy = core::dynamic_pointer_cast<const core::FunctionType>( convFact.convertType( GET_TYPE_PTR(funcDecl) ) );
			vector<core::ExpressionPtr>&& packedArgs = tryPack(convFact.builder, funcTy, args);

			clang::idx::Entity&& funcEntity = clang::idx::Entity::get(funcDecl, const_cast<clang::idx::Program&>(convFact.clangProg));
			const FunctionDecl* definition = convFact.indexer.getDefinitionFor(funcEntity).first;

			if(!definition) {
				// in the case the function is extern, a literal is build
				core::ExpressionPtr irNode =
						convFact.builder.callExpr(	funcTy->getReturnType(), builder.literal(funcDecl->getNameAsString(), funcTy), packedArgs );
				// handle eventual pragmas attached to the Clang node
				frontend::omp::attachOmpAnnotation(irNode, callExpr, convFact);
				return irNode;
			}

			// If we are resolving the body of a recursive function we have to return the associated
			// variable every time a function in the strongly connected graph of function calls
			// is encountred.
			if(ctx.isResolvingRecFuncBody) {
				// check if this type has a typevar already associated, in such case return it
				ConversionContext::RecVarExprMap::const_iterator fit = ctx.recVarExprMap.find(definition);
				if( fit != ctx.recVarExprMap.end() ) {
					// we are resolving a parent recursive type, so we shouldn't
					return builder.callExpr(funcTy->getReturnType(), fit->second, packedArgs);
				}
			}

			if(!ctx.isResolvingRecFuncBody) {
				ConversionContext::LambdaExprMap::const_iterator fit = ctx.lambdaExprCache.find(definition);
				if(fit != ctx.lambdaExprCache.end()) {
					core::ExpressionPtr irNode = builder.callExpr(funcTy->getReturnType(), fit->second, packedArgs);
					// handle eventual pragmas attached to the Clang node
					frontend::omp::attachOmpAnnotation(irNode, callExpr, convFact);
					return irNode;
				}
			}

			assert(definition && "No definition found for function");
			core::ExpressionPtr&& lambdaExpr = convFact.convertFunctionDecl(definition);

			core::ExpressionPtr irNode = builder.callExpr(funcTy->getReturnType(), lambdaExpr, packedArgs);
			// handle eventual pragmas attached to the Clang node
			frontend::omp::attachOmpAnnotation(irNode, callExpr, convFact);
			return irNode;
		}
		assert(false && "Call expression not referring a function");
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						CXX MEMBER CALL EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCXXMemberCallExpr(clang::CXXMemberCallExpr* callExpr) {
		//todo: CXX extensions
		assert(false && "CXXMemberCallExpr not yet handled");
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						CXX OPERATOR CALL EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCXXOperatorCallExprr(clang::CXXOperatorCallExpr* callExpr) {
		//todo: CXX extensions
		assert(false && "CXXOperatorCallExpr not yet handled");
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							BINARY OPERATOR
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitBinaryOperator(clang::BinaryOperator* binOp)  {
		START_LOG_EXPR_CONVERSION(binOp);
		const core::ASTBuilder& builder = convFact.builder;

 		core::ExpressionPtr&& rhs = Visit(binOp->getRHS());
		core::ExpressionPtr&& lhs = Visit(binOp->getLHS());

		// if the binary operator is a comma separated expression, we convert it into a function call
		// which returns the value of the last expression
		if( binOp->getOpcode() == BO_Comma) {
			return createCallExpr(builder, toVector<core::StatementPtr>(lhs, builder.returnStmt(rhs)), rhs->getType());
		}

		core::TypePtr exprTy = convFact.convertType( GET_TYPE_PTR(binOp) );

		// create Pair type
		core::TupleTypePtr tupleTy = builder.tupleType(toVector( exprTy, exprTy ) );
		std::string opType = getOperationType(exprTy);

		// we take care of compound operators first,
		// we rewrite the RHS expression in a normal form, i.e.:
		// a op= b  ---->  a = a op b
		std::string op;
		clang::BinaryOperatorKind baseOp;
		switch( binOp->getOpcode() ) {
		// a *= b
		case BO_MulAssign: 	op = "mul"; baseOp = BO_Mul; break;
		// a /= b
		case BO_DivAssign: 	op = "div"; baseOp = BO_Div; break;
		// a %= b
		case BO_RemAssign:	op = "mod"; baseOp = BO_Rem; break;
		// a += b
		case BO_AddAssign: 	op = "add"; baseOp = BO_Add; break;
		// a -= b
		case BO_SubAssign:	op = "sub"; baseOp = BO_Sub; break;
		// a <<= b
		case BO_ShlAssign: 	op = "shl"; baseOp = BO_Shl; break;
		// a >>= b
		case BO_ShrAssign: 	op = "shr"; baseOp = BO_Shr; break;
		// a &= b
		case BO_AndAssign: 	op = "and"; baseOp = BO_And; break;
		// a |= b
		case BO_OrAssign: 	op = "or"; 	baseOp = BO_Or; break;
		// a ^= b
		case BO_XorAssign: 	op = "xor"; baseOp = BO_Xor; break;
		default:
			break;
		}

		if( !op.empty() ) {
			// The operator is a compound operator, we substitute the RHS expression with the expanded one
			core::RefTypePtr&& lhsTy = core::dynamic_pointer_cast<const core::RefType>(lhs->getType());
			assert( lhsTy && "LHS operand must of type ref<a'>." );
			core::lang::OperatorPtr&& opFunc = builder.literal( opType + "." + op, builder.functionType(tupleTy, exprTy));

			// we check if the RHS is a ref, in that case we use the deref operator
			rhs = tryDeref(builder, rhs);

			const core::TypePtr& lhsSubTy = lhsTy->getElementType();
			rhs = builder.callExpr(lhsSubTy, opFunc,
				toVector<core::ExpressionPtr>(builder.callExpr( lhsSubTy, core::lang::OP_REF_DEREF_PTR, toVector(lhs) ), rhs) );

			// add an annotation to the subexpression
			opFunc->addAnnotation( std::make_shared<c_info::COpAnnotation>( BinaryOperator::getOpcodeStr(baseOp)) );
		}

		bool isAssignment = false;
		bool isLogical = false;
		baseOp = binOp->getOpcode();

		core::lang::OperatorPtr opFunc;

		switch( binOp->getOpcode() ) {
		case BO_PtrMemD:
		case BO_PtrMemI:
			assert(false && "Operator not yet supported!");

		// a * b
		case BO_Mul: 	op = "mul";  break;
		// a / b
		case BO_Div: 	op = "div";  break;
		// a % b
		case BO_Rem: 	op = "mod";  break;
		// a + b
		case BO_Add: 	op = "add";  break;
		// a - b
		case BO_Sub: 	op = "sub";  break;
		// a << b
		case BO_Shl: 	op = "shl";  break;
		// a >> b
		case BO_Shr: 	op = "shr";  break;
		// a & b
		case BO_And: 	op = "and";  break;
		// a ^ b
		case BO_Xor: 	op = "xor";  break;
		// a | b
		case BO_Or:  	op = "or"; 	 break;

		// Logic operators

		// a && b
		case BO_LAnd: 	op = "land"; isLogical=true; break;
		// a || b
		case BO_LOr:  	op = "lor";  isLogical=true; break;
		// a < b
		case BO_LT:	 	op = "lt";   isLogical=true; break;
		// a > b
		case BO_GT:  	op = "gt";   isLogical=true; break;
		// a <= b
		case BO_LE:  	op = "le";   isLogical=true; break;
		// a >= b
		case BO_GE:  	op = "ge";   isLogical=true; break;
		// a == b
		case BO_EQ:  	op = "eq";   isLogical=true; break;
		// a != b
		case BO_NE:	 	op = "ne";   isLogical=true; break;

		case BO_MulAssign: case BO_DivAssign: case BO_RemAssign: case BO_AddAssign: case BO_SubAssign:
		case BO_ShlAssign: case BO_ShrAssign: case BO_AndAssign: case BO_XorAssign: case BO_OrAssign:
		case BO_Assign:
			baseOp = BO_Assign;
			// This is an assignment, we have to make sure the LHS operation is of type ref<a'>
			assert( core::dynamic_pointer_cast<const core::RefType>(lhs->getType()) && "LHS operand must of type ref<a'>." );
			isAssignment = true;
			opFunc = convFact.mgr->get(core::lang::OP_REF_ASSIGN_PTR);
			exprTy = core::lang::TYPE_UNIT_PTR;
			break;
		default:
			assert(false && "Operator not supported");
		}

		// build a callExpr with the 2 arguments
		rhs = tryDeref(builder, rhs);

		if( !isAssignment )
			lhs = tryDeref(builder, lhs);

		// check the types
//		const core::TupleTypePtr& opTy = core::dynamic_pointer_cast<const core::FunctionType>(opFunc->getType())->getArgumentType();
//		if(lhs->getType() != opTy->getElementTypes()[0]) {
//			// add a castepxr
//			lhs = convFact.builder.castExpr(opTy->getElementTypes()[0], lhs);
//		}
//		if(rhs->getType() != opTy->getElementTypes()[1]) {
//			// add a castepxr
//			rhs = convFact.builder.castExpr(opTy->getElementTypes()[1], rhs);
//		}

		if(isLogical) {
			exprTy = core::lang::TYPE_BOOL_PTR;
			tupleTy = builder.tupleType(toVector(lhs->getType(), rhs->getType())); // FIXME
		}

		if(!isAssignment)
			opFunc = builder.literal( opType + "." + op, builder.functionType(tupleTy, exprTy));

		core::ExpressionPtr retExpr = convFact.builder.callExpr( exprTy, opFunc, toVector(lhs, rhs) );

		// add the operator name in order to help the convertion process in the backend
		opFunc->addAnnotation( std::make_shared<c_info::COpAnnotation>( BinaryOperator::getOpcodeStr(baseOp) ) );

		// handle eventual pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(retExpr, binOp, convFact);

		END_LOG_EXPR_CONVERSION( retExpr );
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							UNARY OPERATOR
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitUnaryOperator(clang::UnaryOperator *unOp) {
		START_LOG_EXPR_CONVERSION(unOp);
		const core::ASTBuilder& builder = convFact.builder;
		core::ExpressionPtr&& subExpr = Visit(unOp->getSubExpr());

		// build lambda expression for post/pre increment/decrement unary operators
		auto encloseIncrementOperator = [ &builder ](core::ExpressionPtr subExpr, bool post, bool additive) {
			core::RefTypePtr expTy = core::dynamic_pointer_cast<const core::RefType>(subExpr->getType());
			assert( expTy && "LHS operand must of type ref<a'>." );
			const core::TypePtr& subTy = expTy->getElementType();

			core::VariablePtr tmpVar;
			std::vector<core::StatementPtr> stmts;
			if(post) {
				tmpVar = builder.variable(subTy);
				// ref<a'> __tmp = subexpr
				stmts.push_back(builder.declarationStmt(tmpVar,
						builder.callExpr( subTy, core::lang::OP_REF_DEREF_PTR, toVector<core::ExpressionPtr>(subExpr) ) ));
			}
			// subexpr op= 1
			stmts.push_back(
				builder.callExpr(
					core::lang::TYPE_UNIT_PTR,
					core::lang::OP_REF_ASSIGN_PTR,
					toVector<core::ExpressionPtr>(
						subExpr, // ref<a'> a
						builder.callExpr(
							subTy,
							( additive ? core::lang::OP_INT_ADD_PTR : core::lang::OP_INT_SUB_PTR ),
								toVector<core::ExpressionPtr>(
									builder.callExpr( subTy, core::lang::OP_REF_DEREF_PTR, toVector<core::ExpressionPtr>(subExpr) ),
									builder.castExpr( subTy, builder.literal("1", core::lang::TYPE_INT_4_PTR))
								)
							) // a - 1
					)
				)
			);
			if(post) {
				assert(tmpVar);
				// return __tmp
				stmts.push_back( builder.returnStmt( tmpVar ) );
			} else {
				// return the variable
				stmts.push_back( builder.callExpr( subTy, core::lang::OP_REF_DEREF_PTR, toVector<core::ExpressionPtr>(subExpr) ) );
			}
			return createCallExpr(builder, std::vector<core::StatementPtr>(stmts), subTy);
		};

		bool post = true;
		switch(unOp->getOpcode()) {
		// conversion of post increment/decrement operation is done by creating a tuple expression i.e.:
		// a++ ==> (__tmp = a, a=a+1, __tmp)
		// ++a ==> ( a=a+1, a)
		// --a
		case UO_PreDec:
			post = false;
		// a--
		case UO_PostDec:
			subExpr = encloseIncrementOperator(subExpr, post, false);
			break;
		// a++
		case UO_PreInc:
			post = false;
		// ++a
		case UO_PostInc:
			subExpr = encloseIncrementOperator(subExpr, post, true);
			break;
		// &a
		case UO_AddrOf:
			// assert(false && "Conversion of AddressOf operator '&' not supported");
			break;
		// *a
		case UO_Deref: {
			subExpr = tryDeref(builder, subExpr);

			if(core::dynamic_pointer_cast<const core::VectorType>(subExpr->getType()) ||
				core::dynamic_pointer_cast<const core::ArrayType>(subExpr->getType())) {

				core::SingleElementTypePtr subTy = core::dynamic_pointer_cast<const core::SingleElementType>(subExpr->getType());
				assert(subTy);
				subExpr = builder.callExpr( subTy->getElementType(), core::lang::OP_SUBSCRIPT_SINGLE_PTR,
						toVector<core::ExpressionPtr>(subExpr, builder.literal("0", core::lang::TYPE_INT_4_PTR)) );
			}
			break;
		}
		// +a
		case UO_Plus:
			// just return the subexpression
			break;
		// -a
		case UO_Minus:
			// TODO:
			// assert(false && "Conversion of unary operator '-' not supported");
		// ~a
		case UO_Not:
			// TODO:
			// assert(false && "Conversion of unary operator '~' not supported");
		// !a
		case UO_LNot:
			// TODO:
			// assert(false && "Conversion of unary operator '!' not supported");

		case UO_Real:
		case UO_Imag:
		case UO_Extension: //TODO:
		default:
			break;
			// assert(false && "Unary operator not supported");
		}

		// handle eventual pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(subExpr, unOp, convFact);

		// add the operator name in order to help the convertion process in the backend
		subExpr->addAnnotation( std::make_shared<c_info::COpAnnotation>( UnaryOperator::getOpcodeStr(unOp->getOpcode()) ) );

		return subExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							CONDITIONAL OPERATOR
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitConditionalOperator(clang::ConditionalOperator* condOp) {
		START_LOG_EXPR_CONVERSION(condOp);

		assert(condOp->getSaveExpr() == NULL && "Conditional operation with 'gcc save' expession not supperted.");
		core::TypePtr&& retTy = convFact.convertType( GET_TYPE_PTR(condOp) );

		core::ExpressionPtr&& trueExpr = Visit(condOp->getTrueExpr());
		core::ExpressionPtr&& falseExpr = Visit(condOp->getFalseExpr());
		core::ExpressionPtr&& condExpr = Visit( condOp->getCond() );

		// add ref.deref if needed
		condExpr = tryDeref(convFact.builder, condExpr);

		if(*condExpr->getType() != *core::lang::TYPE_BOOL_PTR) {
			// the return type of the condition is not a boolean, we add a cast expression
			condExpr = convFact.builder.castExpr(core::lang::TYPE_BOOL_PTR, condExpr);
		}
		core::StatementPtr&& ifStmt = convFact.builder.ifStmt(condExpr, trueExpr, falseExpr);
		core::ExpressionPtr&& retExpr = createCallExpr( convFact.builder, toVector( ifStmt ),  retTy);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						ARRAY SUBSCRIPT EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitArraySubscriptExpr(clang::ArraySubscriptExpr* arraySubExpr) {
		START_LOG_EXPR_CONVERSION(arraySubExpr);

		// CLANG introduces implicit cast for the base expression of array subscripts
		// which cast the array type into a simple pointer. As insieme supports subscripts
		// only for array or vector types, we skip eventual implicit cast operations
		Expr* baseExpr = arraySubExpr->getBase();
		while(ImplicitCastExpr *castExpr = dyn_cast<ImplicitCastExpr>(baseExpr))
			baseExpr = castExpr->getSubExpr();

		// IDX
		core::ExpressionPtr&& idx = tryDeref(convFact.builder, Visit( arraySubExpr->getIdx() ));

		// BASE
		core::ExpressionPtr&& base = tryDeref(convFact.builder, Visit( baseExpr ) );

		// TODO: we need better checking for vector type
		assert( (core::dynamic_pointer_cast<const core::VectorType>( base->getType() ) ||
				core::dynamic_pointer_cast<const core::ArrayType>( base->getType() )) &&
				"Base expression of array subscript is not a vector/array type.");

		// We are sure the type of base is either a vector or an array
		const core::TypePtr& subTy = core::dynamic_pointer_cast<const core::SingleElementType>(base->getType())->getElementType();

		core::ExpressionPtr&& retExpr =
			convFact.builder.callExpr( subTy, core::lang::OP_SUBSCRIPT_SINGLE_PTR, toVector<core::ExpressionPtr>(base, idx) );

		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						EXT VECTOR ELEMENT EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitExtVectorElementExpr(ExtVectorElementExpr* vecElemExpr){
        START_LOG_EXPR_CONVERSION(vecElemExpr);
        core::ExpressionPtr&& base = Visit( vecElemExpr->getBase() );

        std::string pos;
        llvm::StringRef&& accessor = vecElemExpr->getAccessor().getName();

        //translate OpenCL accessor string to index
        if(accessor == "x") 		pos = "0";
        else if(accessor == "y")    pos = "1";
        else if(accessor == "z")	pos = "2";
        else if(accessor == "w")	pos = "3";
        else {
        	// the input string is in a form sXXX
        	assert(accessor.front() == 's');
        	// we skip the s and return the value to get the number
        	llvm::StringRef numStr = accessor.substr(1,accessor.size()-1);
        	assert(insieme::utils::numeric_cast<unsigned int>(numStr.data()) >= 0 && "String is not a number");
        	pos = numStr;
        }

        core::TypePtr&& exprTy = convFact.convertType( GET_TYPE_PTR(vecElemExpr) );
        core::ExpressionPtr&& idx = convFact.builder.literal(pos, exprTy); // FIXME! are you sure the type is exprTy? and not ref<rexprTy>?
        // if the type of the vector is a refType, we deref it
        base = tryDeref(convFact.builder, base);

        core::ExpressionPtr&& retExpr = convFact.builder.callExpr(convFact.builder.refType(exprTy), core::lang::OP_SUBSCRIPT_PTR, toVector( base, idx ));
        END_LOG_EXPR_CONVERSION(retExpr);
        return retExpr;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							VAR DECLARATION REFERENCE
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitDeclRefExpr(clang::DeclRefExpr* declRef) {
		START_LOG_EXPR_CONVERSION(declRef);
		// check whether this is a reference to a variable
		if(VarDecl* varDecl = dyn_cast<VarDecl>(declRef->getDecl())) {
			return convFact.lookUpVariable(varDecl);
		}
		// todo: C++ check whether this is a reference to a class field, or method (function).
		assert(false && "DeclRefExpr not supported!");
	}

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //                       VECTOR INITALIZATION EXPRESSION
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitInitListExpr(clang::InitListExpr* initList) {
        START_LOG_EXPR_CONVERSION(initList);
        std::vector<core::ExpressionPtr> elements;

        // get all values of the init expression
        for(size_t i = 0, end = initList->getNumInits(); i < end; ++i) {
             elements.push_back( convFact.convertExpr( initList->getInit(i)) );
        }

        // create vector initializator
        core::ExpressionPtr retExpr = convFact.builder.vectorExpr(elements);

        END_LOG_EXPR_CONVERSION(retExpr);
        return retExpr;
    }
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// 							Printing macros for statements
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define FORWARD_VISITOR_CALL(StmtTy) \
	StmtWrapper Visit##StmtTy( StmtTy* stmt ) { return StmtWrapper( convFact.convertExpr(stmt) ); }


#define START_LOG_STMT_CONVERSION(stmt) \
	DVLOG(1) << "\n****************************************************************************************\n" \
			 << "Converting statement [class: '" << stmt->getStmtClassName() << "'] \n" \
			 << "-> at location: (" << utils::location(stmt->getLocStart(), convFact.clangComp.getSourceManager()) << "): "; \
	if( VLOG_IS_ON(2) ) { \
		DVLOG(2) << "Dump of clang statement:\n" \
				 << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"; \
		stmt->dump(convFact.clangComp.getSourceManager()); \
	}

#define END_LOG_STMT_CONVERSION(stmt) \
	DVLOG(1) << "Converted 'statement' into IR stmt: "; \
	DVLOG(1) << "\t" << *stmt;


//#############################################################################
//
//							CLANG STMT CONVERTER
//
//############################################################################
class ConversionFactory::ClangStmtConverter: public StmtVisitor<ClangStmtConverter, StmtWrapper> {
	ConversionFactory& convFact;

public:
	ClangStmtConverter(ConversionFactory& convFact): convFact(convFact) { }

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							DECLARATION STATEMENT
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// In clang a declstmt is represented as a list of VarDecl
	StmtWrapper VisitDeclStmt(clang::DeclStmt* declStmt) {
		// if there is only one declaration in the DeclStmt we return it
		if( declStmt->isSingleDecl() && isa<clang::VarDecl>(declStmt->getSingleDecl()) ) {
			core::DeclarationStmtPtr&& retStmt = convFact.convertVarDecl( dyn_cast<clang::VarDecl>(declStmt->getSingleDecl()) );

			// handle eventual OpenMP pragmas attached to the Clang node
			frontend::omp::attachOmpAnnotation(retStmt, declStmt, convFact);
			return StmtWrapper(retStmt );
		}

		// otherwise we create an an expression list which contains the multiple declaration inside the statement
		StmtWrapper retList;
		for(clang::DeclStmt::decl_iterator it = declStmt->decl_begin(), e = declStmt->decl_end(); it != e; ++it)
			if( clang::VarDecl* varDecl = dyn_cast<clang::VarDecl>(*it) ) {
				retList.push_back( convFact.convertVarDecl(varDecl) );
				// handle eventual OpenMP pragmas attached to the Clang node
				frontend::omp::attachOmpAnnotation(retList.back(), declStmt, convFact);
			}
		return retList;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							RETURN STATEMENT
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	StmtWrapper VisitReturnStmt(ReturnStmt* retStmt) {
		START_LOG_STMT_CONVERSION(retStmt);
		assert(retStmt->getRetValue() && "ReturnStmt has an empty expression");

		core::StatementPtr ret = convFact.builder.returnStmt( convFact.convertExpr( retStmt->getRetValue() ) );
		// handle eventual OpenMP pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(ret, retStmt, convFact);

		END_LOG_STMT_CONVERSION( ret );
		return StmtWrapper( ret );
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								FOR STATEMENT
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	StmtWrapper VisitForStmt(ForStmt* forStmt) {
		START_LOG_STMT_CONVERSION(forStmt);
		const core::ASTBuilder& builder = convFact.builder;
		VLOG(2) << "{ Visit ForStmt }";

		StmtWrapper retStmt;
		StmtWrapper&& body = Visit(forStmt->getBody());

		try {
			// Analyze loop for induction variable
			analysis::LoopAnalyzer loopAnalysis(forStmt, convFact);

			core::ExpressionPtr&& incExpr = loopAnalysis.getIncrExpr();
			core::ExpressionPtr&& condExpr = loopAnalysis.getCondExpr();

			Stmt* initStmt = forStmt->getInit();
			// if there is no initialization stmt, we transform the ForStmt into a WhileStmt
			if( !initStmt ) {
				//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
				// we are analyzing a loop where the init expression is empty, e.g.:
				//
				// 		for(; cond; inc) { body }
				//
				// As the IR doesn't support loop stmt with no initialization we represent
				// the for loop as while stmt, i.e.
				//
				// 		while( cond ) {
				//			{ body }
				//  		inc;
				// 		}
				//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

				vector<core::StatementPtr> whileBody(body);
				// adding the incExpr at after the loop body
				whileBody.push_back( convFact.convertExpr( forStmt->getInc() ) );

				core::StatementPtr&& whileStmt = builder.whileStmt( convFact.convertExpr( forStmt->getCond() ), builder.compoundStmt(whileBody) );

				// handle eventual pragmas refering to the Clang node
				frontend::omp::attachOmpAnnotation(whileStmt, forStmt, convFact);

				END_LOG_STMT_CONVERSION( whileStmt );
				return StmtWrapper( whileStmt );
			}

			StmtWrapper&& initExpr = Visit( initStmt );
			// induction variable for this loop
			core::VariablePtr&& inductionVar = convFact.lookUpVariable(loopAnalysis.getInductionVar());

			if( !initExpr.isSingleStmt() ) {
				assert(core::dynamic_pointer_cast<const core::DeclarationStmt>(initExpr[0]) && "Not a declaration statement");
				//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
				// we have a multiple declaration in the initialization part of the stmt, e.g.
				//
				//		for(int a,b=0; ...)
				//
				// to handle this situation we have to create an outer block in order to declare
				// the variables which are not used as induction variable:
				//
				//		{
				//			int a=0;
				//			for(int b=0;...) { }
				//		}
				// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
				typedef std::function<bool (const core::StatementPtr&)> InductionVarFilterFunc;
				InductionVarFilterFunc inductionVarFilter =
					[ this, inductionVar ](const core::StatementPtr& curr) -> bool {
						core::DeclarationStmtPtr&& declStmt = core::dynamic_pointer_cast<const core::DeclarationStmt>(curr);
						assert(declStmt && "Not a declaration statement");
						return declStmt->getVariable() == inductionVar;
					};

				std::function<bool (const InductionVarFilterFunc& functor, const core::StatementPtr& curr)> negation =
					[](std::function<bool (const core::StatementPtr&)> functor, const core::StatementPtr& curr) -> bool { return !functor(curr); };

				// we insert all the variable declarations (excluded the induction variable)
				// before the body of the for loop
				std::copy_if(initExpr.begin(), initExpr.end(), std::back_inserter(retStmt),
						std::bind(negation, inductionVarFilter, std::placeholders::_1) );

				// we now look for the declaration statement which contains the induction variable
				std::vector<core::StatementPtr>::const_iterator fit =
						std::find_if(initExpr.begin(), initExpr.end(), std::bind( inductionVarFilter, std::placeholders::_1));
				assert(fit != initExpr.end() && "Induction variable not declared in the loop initialization expression");
				// replace the initExpr with the declaration statement of the induction variable
				initExpr = *fit;
			}

			assert(initExpr.isSingleStmt() && "Init expression for loop sttatement contains multiple statements");
			// We are in the case where we are sure there is exactly 1 element in the
			// initialization expression
			core::DeclarationStmtPtr&& declStmt = core::dynamic_pointer_cast<const core::DeclarationStmt>( initExpr.getSingleStmt() );
			bool iteratorChanged = false;
			core::VariablePtr newIndVar;
			if( !declStmt ) {
				//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
				// the init expression is not a declaration stmt, it could be a situation where
				//	it is an assignment operation, eg:
				//
				//			for( i=exp; ...) { i... }
				//
				// In this case we have to replace the old induction variable with a new one
				// and replace every occurrence of the old variable with the new one. Furthermore,
				// to mantain the correct semantics of the code, the value of the old induction
				// variable has to be restored when exiting the loop.
				//
				//			{
				//				for(_i = init; _i < cond; _i += step) { _i... }
				//				i = ceil((cond-init)/step) * step + init;
				//			}
				//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
				core::ExpressionPtr&& init = core::dynamic_pointer_cast<const core::Expression>( initExpr.getSingleStmt() );
				assert(init && "Init statement for loop is not an xpression");

				const core::TypePtr& varTy = inductionVar->getType();
				// we create a new induction variable, we don't register it to the variable
				// map as it will be valid only within this for statement
				newIndVar = builder.variable(varTy);

				// we have to define a new induction variable for the loop and replace every
				// instance in the loop with the new variable
				DVLOG(2) << "Substituting loop induction variable: " << loopAnalysis.getInductionVar()->getNameAsString()
						<< " with variable: v" << newIndVar->getId();

				// Initialize the value of the new induction variable with the value of the old one
				core::CallExprPtr&& callExpr = core::dynamic_pointer_cast<const core::CallExpr>(init);
				assert(callExpr && *callExpr->getFunctionExpr() == *core::lang::OP_REF_ASSIGN_PTR &&
						"Expression not handled in a forloop initaliazation statement!");

				// we handle only the situation where the initExpr is an assignment
				init = callExpr->getArguments()[1]; // getting RHS

				declStmt = builder.declarationStmt( newIndVar, builder.callExpr(varTy, core::lang::OP_REF_VAR_PTR, toVector(init)) );
				core::NodePtr&& ret = core::transform::replaceNode(builder, body.getSingleStmt(), inductionVar, newIndVar, true);

				// replace the body with the newly modified one
				body = StmtWrapper( core::dynamic_pointer_cast<const core::Statement>(ret) );

				// we have to remember that the iterator has been changed for this loop
				iteratorChanged = true;
			}

			assert(declStmt && "Falied convertion of loop init expression");

			// We finally create the IR ForStmt
			core::ForStmtPtr&& irFor = builder.forStmt(declStmt, body.getSingleStmt(), condExpr, incExpr);
			assert(irFor && "Created for statement is not valid");

			// handle eventual pragmas attached to the Clang node
			frontend::omp::attachOmpAnnotation(irFor, forStmt, convFact);
			retStmt.push_back( irFor );

			if(iteratorChanged) {
				// in the case we replace the loop iterator with a temporary variable,
				// we have to assign the final value of the iterator to the old variable
				// so we don't change the semantics of the code
				const core::lang::OperatorPtr& refAssign = convFact.mgr->get(core::lang::OP_REF_ASSIGN_PTR);
				refAssign->addAnnotation( std::make_shared<c_info::COpAnnotation>("=") ); // FIXME

				// inductionVar = COND()? ---> FIXME!
				retStmt.push_back( builder.callExpr(
						core::lang::TYPE_UNIT_PTR, refAssign, toVector<core::ExpressionPtr>( inductionVar, loopAnalysis.getCondExpr() )
				));
			}

		} catch(const analysis::LoopNormalizationError& e) {

			if( VarDecl* condVarDecl = forStmt->getConditionVariable() ) {
				assert(forStmt->getCond() == NULL && "ForLoop condition cannot be a variable declaration and an expression");
				//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
				// the for loop has a variable declared in the condition part, e.g.
				//
				// 		for(...; int a = f(); ...)
				//
				// to handle this kind of situation we have to move the declaration
				// outside the loop body inside a new context
				//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
				Expr* expr = condVarDecl->getInit();
				condVarDecl->setInit(NULL); // set the expression to null (temporarely)
				core::DeclarationStmtPtr&& declStmt = convFact.convertVarDecl(condVarDecl);
				condVarDecl->setInit(expr); // restore the init value

				assert(false && "ForStmt with a declaration of a condition variable not supported");
				retStmt.push_back( declStmt );
			}

			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			// analysis of loop structure failed, we have to build a while statement
			//
			//		for(init; cond; step) { body }
			//
			// Will be translated in the following while statement structure:
			//
			//		{
			//			init;
			//			while(cond) {
			//				{ body }
			//				step;
			//			}
			//		}
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			retStmt.push_back( Visit( forStmt->getInit() ).getSingleStmt() ); // init;
			core::StatementPtr&& whileStmt = builder.whileStmt(
				convFact.convertExpr( forStmt->getCond() ), // cond
					builder.compoundStmt(
						toVector<core::StatementPtr>( tryAggregateStmts(builder, body), convFact.convertExpr( forStmt->getInc() ) )
					)
				);

			// handle eventual pragmas attached to the Clang node
			frontend::omp::attachOmpAnnotation(whileStmt, forStmt, convFact);

			retStmt.push_back( whileStmt );
		}
		retStmt = tryAggregateStmts(builder, retStmt);

		END_LOG_STMT_CONVERSION( retStmt.getSingleStmt() );
		return retStmt;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								IF STATEMENT
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	StmtWrapper VisitIfStmt(IfStmt* ifStmt) {
		START_LOG_STMT_CONVERSION(ifStmt);
		const core::ASTBuilder& builder = convFact.builder;
		StmtWrapper retStmt;

		VLOG(2) << "{ Visit IfStmt }";
		core::StatementPtr&& thenBody = tryAggregateStmts( builder, Visit( ifStmt->getThen() ) );
		assert(thenBody && "Couldn't convert 'then' body of the IfStmt");

		core::ExpressionPtr condExpr;
		if( const VarDecl* condVarDecl = ifStmt->getConditionVariable() ) {
			assert(ifStmt->getCond() == NULL && "IfStmt condition cannot contains both a variable declaration and an expression");
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			// we are in the situation where a variable is declared in the if condition, i.e.:
			//		if(int a = exp) { }
			//
			// this will be converted into the following IR representation:
			// 		{
			//			int a = exp;
			//			if(cast<bool>(a)){ }
			//		}
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			core::DeclarationStmtPtr&& declStmt = convFact.convertVarDecl(condVarDecl);
			retStmt.push_back( declStmt );

			// the expression will be a cast to bool of the declared variable
			condExpr = builder.castExpr(core::lang::TYPE_BOOL_PTR, declStmt->getVariable());
		} else {
			const Expr* cond = ifStmt->getCond();
			assert(cond && "If statement with no condition.");

			condExpr = convFact.convertExpr( cond );
			if(*condExpr->getType() != *core::lang::TYPE_BOOL_PTR) {
				// add a cast expression to bool
				condExpr = builder.castExpr(core::lang::TYPE_BOOL_PTR, condExpr);
			}
		}
		assert(condExpr && "Couldn't convert 'condition' expression of the IfStmt");

		core::StatementPtr elseBody;
		// check for else statement
		if(Stmt* elseStmt = ifStmt->getElse()) {
			elseBody = tryAggregateStmts( builder, Visit( elseStmt ) );
		} else {
			// create an empty compound statement in the case there is no else stmt
			elseBody = builder.compoundStmt();
		}
		assert(elseBody && "Couldn't convert 'else' body of the IfStmt");

		core::StatementPtr&& irNode = builder.ifStmt(condExpr, thenBody, elseBody);

		// handle eventual OpenMP pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(irNode, ifStmt, convFact);

		// adding the ifstmt to the list of returned stmts
		retStmt.push_back( irNode );

		// try to aggregate statements into a CompoundStmt if more than 1 statement
		// has been created from this IfStmt
		retStmt = tryAggregateStmts(builder, retStmt);

		END_LOG_STMT_CONVERSION( retStmt.getSingleStmt() );
		// otherwise we introduce an outer CompoundStmt
		return retStmt;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							WHILE STATEMENT
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	StmtWrapper VisitWhileStmt(WhileStmt* whileStmt) {
		START_LOG_STMT_CONVERSION(whileStmt);
		const core::ASTBuilder& builder = convFact.builder;
		StmtWrapper retStmt;

		VLOG(2) << "{ WhileStmt }";
		core::StatementPtr&& body = tryAggregateStmts( builder, Visit( whileStmt->getBody() ) );
		assert(body && "Couldn't convert body of the WhileStmt");

		core::ExpressionPtr condExpr;
		if( VarDecl* condVarDecl = whileStmt->getConditionVariable() ) {
			assert(whileStmt->getCond() == NULL &&
					"WhileStmt condition cannot contains both a variable declaration and an expression");

			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			// we are in the situation where a variable is declared in the if condition, i.e.:
			//
			//		 while(int a = expr) { }
			//
			// this will be converted into the following IR representation:
			//
			// 		{
			//			int a = 0;
			//			while(a = expr){ }
			//		}
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			Expr* expr = condVarDecl->getInit();
			condVarDecl->setInit(NULL); // set the expression to null (temporarily)
			core::DeclarationStmtPtr&& declStmt = convFact.convertVarDecl(condVarDecl);
			condVarDecl->setInit(expr); // set back the value of init value

			retStmt.push_back( declStmt );
			// the expression will be an a = expr
			// core::ExpressionPtr&& condExpr = convFact.convertExpr(expr);
			assert(false && "WhileStmt with a declaration of a condition variable not supported");
		} else {
			const Expr* cond = whileStmt->getCond();
			assert(cond && "WhileStmt with no condition.");
			condExpr = convFact.convertExpr( cond );
		}
		assert(condExpr && "Couldn't convert 'condition' expression of the WhileStmt");
		core::StatementPtr&& irNode = builder.whileStmt(condExpr, body);

		// handle eventual OpenMP pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(irNode, whileStmt, convFact);

		// adding the WhileStmt to the list of returned stmts
		retStmt.push_back( irNode );
		retStmt = tryAggregateStmts(builder, retStmt);

		END_LOG_STMT_CONVERSION( retStmt.getSingleStmt() );
		// otherwise we introduce an outer CompoundStmt
		return retStmt;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							SWITCH STATEMENT
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	StmtWrapper VisitSwitchStmt(SwitchStmt* switchStmt) {
		START_LOG_STMT_CONVERSION(switchStmt);
		const core::ASTBuilder& builder = convFact.builder;
		StmtWrapper retStmt;

		VLOG(2) << "{ Visit SwitchStmt }";
		core::ExpressionPtr condExpr(NULL);
		if( const VarDecl* condVarDecl = switchStmt->getConditionVariable() ) {
			assert(switchStmt->getCond() == NULL && "SwitchStmt condition cannot contains both a variable declaration and an expression");

			core::DeclarationStmtPtr&& declStmt = convFact.convertVarDecl(condVarDecl);
			retStmt.push_back( declStmt );

			// the expression will be a reference to the declared variable
			condExpr = declStmt->getVariable();
		} else {
			const Expr* cond = switchStmt->getCond();
			assert(cond && "SwitchStmt with no condition.");
			condExpr = tryDeref( builder, convFact.convertExpr( cond ) );

			// we create a variable to store the value of the condition for this switch
			core::VariablePtr&& condVar = builder.variable(core::lang::TYPE_INT_GEN_PTR);
			// int condVar = condExpr;
			core::DeclarationStmtPtr&& declVar = builder.declarationStmt(condVar, builder.castExpr(core::lang::TYPE_INT_GEN_PTR, condExpr));
			retStmt.push_back(declVar);

			condExpr = condVar;
		}
		assert(condExpr && "Couldn't convert 'condition' expression of the SwitchStmt");

		// Handle the cases of the SwitchStmt
		if( Stmt* body = switchStmt->getBody() ) {
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			// this Switch stamtement has a body, i.e.:
			//
			// 		switch(e) {
			// 	 		{ body }
			// 	 		case x:...
			// 		}
			//
			// As the IR doens't allow a body to be represented inside the switch stmt
			// we bring this code outside after the declaration of the eventual conditional
			// variable.
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			if(CompoundStmt* compStmt = dyn_cast<CompoundStmt>(body)) {
				std::for_each(compStmt->body_begin(), compStmt->body_end(),
					[ &retStmt, this ] (Stmt* curr) {
						if(!isa<SwitchCase>(curr)) {
							StmtWrapper&& visitedStmt = this->Visit(curr);
							std::copy(visitedStmt.begin(), visitedStmt.end(), std::back_inserter(retStmt));
						}
					}
				);
			}
		}
		vector<core::SwitchStmt::Case> cases;
		// initialize the default case with an empty compoundstmt

		core::StatementPtr&& defStmt = builder.compoundStmt();
		// the cases can be handled now
		const SwitchCase* switchCaseStmt = switchStmt->getSwitchCaseList();
		while(switchCaseStmt) {
			if( const CaseStmt* caseStmt = dyn_cast<const CaseStmt>(switchCaseStmt) ) {
				core::StatementPtr subStmt;
				if( const Expr* rhs = caseStmt->getRHS() ) {
					assert(!caseStmt->getSubStmt() && "Case stmt cannot have both a RHS and and sub statement.");
					subStmt = convFact.convertExpr( rhs );
				} else if( const Stmt* sub = caseStmt->getSubStmt() ) {
					subStmt = tryAggregateStmts( builder, Visit( const_cast<Stmt*>(sub) ) );
				}
				cases.push_back( std::make_pair(convFact.convertExpr( caseStmt->getLHS() ), subStmt) );
			} else {
				// default case
				const DefaultStmt* defCase = dyn_cast<const DefaultStmt>(switchCaseStmt);
				assert(defCase && "Case is not the 'default:'.");
				defStmt = tryAggregateStmts( builder, Visit( const_cast<Stmt*>(defCase->getSubStmt())) );
			}
			// next case
			switchCaseStmt = switchCaseStmt->getNextSwitchCase();
		}

		core::StatementPtr&& irNode = builder.switchStmt(condExpr, cases, defStmt);

		// handle eventual OpenMP pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(irNode, switchStmt, convFact);

		// Appends the switchstmt to the current list of stmt
		retStmt.push_back( irNode );
		retStmt = tryAggregateStmts(builder, retStmt);

		END_LOG_STMT_CONVERSION( retStmt.getSingleStmt() );
		return retStmt;
	}

	// as a CaseStmt or DefaultStmt cannot be converted into any IR statements, we generate an error in the case
	// the visitor visits one of these nodes, the VisitSwitchStmt has to make sure the visitor is not called on his subnodes
	StmtWrapper VisitSwitchCase(SwitchCase* caseStmt) { assert(false && "Visitor is visiting a 'case' stmt (cannot compute)"); }

	StmtWrapper VisitBreakStmt(BreakStmt* breakStmt) { return StmtWrapper( convFact.builder.breakStmt() ); }
	StmtWrapper VisitContinueStmt(ContinueStmt* contStmt) { return StmtWrapper( convFact.builder.continueStmt() ); }

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							COMPOUND STATEMENT
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	StmtWrapper VisitCompoundStmt(CompoundStmt* compStmt) {
		START_LOG_STMT_CONVERSION(compStmt);
		vector<core::StatementPtr> stmtList;
		std::for_each( compStmt->body_begin(), compStmt->body_end(),
			[ &stmtList, this ] (Stmt* stmt) {
				// A compoundstmt can contain declaration statements.This means that a clang DeclStmt can be converted in multiple
				// StatementPtr because an initialization list such as: int a,b=1; is converted into the following sequence of statements:
				// int<a> a = 0; int<4> b = 1;
				StmtWrapper&& convertedStmt = this->Visit(stmt);
				std::copy(convertedStmt.begin(), convertedStmt.end(), std::back_inserter(stmtList));
			}
		);
		core::StatementPtr retStmt = convFact.builder.compoundStmt(stmtList);

		// handle eventual OpenMP pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(retStmt, compStmt, convFact);

		END_LOG_STMT_CONVERSION(retStmt);
		return StmtWrapper( retStmt );
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							NULL STATEMENT
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	StmtWrapper VisitNullStmt(NullStmt* nullStmt) {
		//TODO: Visual Studio 2010 fix: && removed
		core::StatementPtr retStmt = core::lang::STMT_NO_OP_PTR;

		// handle eventual OpenMP pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(retStmt, nullStmt, convFact);

		return StmtWrapper( retStmt );
	}

	FORWARD_VISITOR_CALL(IntegerLiteral)
	FORWARD_VISITOR_CALL(FloatingLiteral)
	FORWARD_VISITOR_CALL(CharacterLiteral)
	FORWARD_VISITOR_CALL(StringLiteral)

	FORWARD_VISITOR_CALL(BinaryOperator)
	FORWARD_VISITOR_CALL(UnaryOperator)
	FORWARD_VISITOR_CALL(ConditionalOperator)

	FORWARD_VISITOR_CALL(CastExpr)
	FORWARD_VISITOR_CALL(ImplicitCastExpr)
	FORWARD_VISITOR_CALL(DeclRefExpr)
	FORWARD_VISITOR_CALL(ArraySubscriptExpr)
	FORWARD_VISITOR_CALL(CallExpr)
	FORWARD_VISITOR_CALL(ParenExpr)

	StmtWrapper VisitStmt(Stmt* stmt) {
		std::for_each( stmt->child_begin(), stmt->child_end(), [ this ] (Stmt* stmt) { this->Visit(stmt); });
		return StmtWrapper();
	}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// 							Printing macros for statements
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define MAKE_SIZE(n)	toVector(core::IntTypeParam::getConcreteIntParam(n))
#define EMPTY_TYPE_LIST	vector<core::TypePtr>()

#define START_LOG_TYPE_CONVERSION(type) \
	DVLOG(1) << "\n****************************************************************************************\n" \
			 << "Converting type [class: '" << (type)->getTypeClassName() << "']"; \
	if( VLOG_IS_ON(2) ) { \
		DVLOG(2) << "Dump of clang type: \n" \
				 << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"; \
		type->dump(); \
	}

#define END_LOG_TYPE_CONVERSION(type) \
	DVLOG(1) << "Converted 'type' into IR type: "; \
	DVLOG(1) << "\t" << *type;


//#############################################################################
//
//							CLANG TYPE CONVERTER
//
//############################################################################
class ConversionFactory::ClangTypeConverter: public TypeVisitor<ClangTypeConverter, core::TypePtr> {
	const ConversionFactory& convFact;
	ConversionFactory::ConversionContext& ctx;

public:
	ClangTypeConverter(const ConversionFactory& fact): convFact( fact ), ctx(*fact.ctx) { }

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								BUILTIN TYPES
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitBuiltinType(BuiltinType* buldInTy) {
		START_LOG_TYPE_CONVERSION( buldInTy );
		const core::ASTBuilder& builder = convFact.builder;

		switch(buldInTy->getKind()) {
		case BuiltinType::Void:
			return builder.getUnitType();
		case BuiltinType::Bool:
			return builder.getBoolType();

		// char types
		case BuiltinType::Char_U:
		case BuiltinType::UChar:
			return builder.genericType("uchar");
		case BuiltinType::Char16:
			return builder.genericType("char", EMPTY_TYPE_LIST, MAKE_SIZE(2));
		case BuiltinType::Char32:
			return builder.genericType("char", EMPTY_TYPE_LIST, MAKE_SIZE(4));
		case BuiltinType::Char_S:
		case BuiltinType::SChar:
			return builder.genericType("char");
		case BuiltinType::WChar:
			return builder.genericType("wchar");

		// integer types
		case BuiltinType::UShort:
			return builder.getUIntType( SHORT_LENGTH );
		case BuiltinType::Short:
			return builder.getIntType( SHORT_LENGTH );
		case BuiltinType::UInt:
			return builder.getUIntType( INT_LENGTH );
		case BuiltinType::Int:
			return builder.getIntType( INT_LENGTH );
		case BuiltinType::UInt128:
			return builder.getUIntType( 16 );
		case BuiltinType::Int128:
			return builder.getIntType( 16 );
		case BuiltinType::ULong:
			return builder.getUIntType( LONG_LENGTH );
		case BuiltinType::ULongLong:
			return builder.getUIntType( LONG_LONG_LENGTH );
		case BuiltinType::Long:
			return builder.getIntType( LONG_LENGTH );
		case BuiltinType::LongLong:
			return builder.getIntType( LONG_LONG_LENGTH );

		// real types
		case BuiltinType::Float:
			return builder.getRealType( FLOAT_LENGTH );
		case BuiltinType::Double:
			return builder.getRealType( DOUBLE_LENGTH );
		case BuiltinType::LongDouble:
			return builder.getRealType( LONG_DOUBLE_LENGTH );

		// not supported types
		case BuiltinType::NullPtr:
		case BuiltinType::Overload:
		case BuiltinType::Dependent:
		case BuiltinType::UndeducedAuto:
		default:
			throw "type not supported"; //todo introduce exception class
		}
		assert(false && "Built-in type conversion not supported!");
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								COMPLEX TYPE
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitComplexType(ComplexType* bulinTy) {
		assert(false && "ComplexType not yet handled!");
	}

	// ------------------------   ARRAYS  -------------------------------------
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// 					CONSTANT ARRAY TYPE
	//
	// This method handles the canonical version of C arrays with a specified
	// constant size. For example, the canonical type for 'int A[4 + 4*100]' is
	// a ConstantArrayType where the element type is 'int' and the size is 404
	//
	// The IR representation for such array will be: vector<ref<int<4>>,404>
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitConstantArrayType(ConstantArrayType* arrTy) {
		START_LOG_TYPE_CONVERSION( arrTy );
		if(arrTy->isSugared())
			// if the type is sugared, we Visit the desugared type
			return Visit( arrTy->desugar().getTypePtr() );

		size_t arrSize = *arrTy->getSize().getRawData();
		core::TypePtr&& elemTy = Visit( arrTy->getElementType().getTypePtr() );
		assert(elemTy && "Conversion of array element type failed.");

		core::TypePtr&& retTy = convFact.builder.vectorType( convFact.builder.refType(elemTy), core::IntTypeParam::getConcreteIntParam(arrSize) );
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						INCOMPLETE ARRAT TYPE
	// This method handles C arrays with an unspecified size. For example
	// 'int A[]' has an IncompleteArrayType where the element type is 'int'
	// and the size is unspecified.
	//
	// The representation for such array will be: ref<array<ref<int<4>>>>
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitIncompleteArrayType(IncompleteArrayType* arrTy) {
		START_LOG_TYPE_CONVERSION( arrTy );
		if(arrTy->isSugared())
			// if the type is sugared, we Visit the desugared type
			return Visit( arrTy->desugar().getTypePtr() );

		const core::ASTBuilder& builder = convFact.builder;
		core::TypePtr&& elemTy = Visit( arrTy->getElementType().getTypePtr() );
		assert(elemTy && "Conversion of array element type failed.");

		core::TypePtr&& retTy = builder.arrayType(builder.refType(elemTy));
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							VARIABLE ARRAT TYPE
	// This class represents C arrays with a specified size which is not an
	// integer-constant-expression. For example, 'int s[x+foo()]'. Since the
	// size expression is an arbitrary expression, we store it as such.
	// Note: VariableArrayType's aren't uniqued (since the expressions aren't)
	// and should not be: two lexically equivalent variable array types could
	// mean different things, for example, these variables do not have the same
	// type dynamically:
	//				void foo(int x) { int Y[x]; ++x; int Z[x]; }
	//
	// he representation for such array will be: array<ref<int<4>>>( expr() )
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitVariableArrayType(VariableArrayType* arrTy) {
		START_LOG_TYPE_CONVERSION( arrTy );
		if(arrTy->isSugared())
			// if the type is sugared, we Visit the desugared type
			return Visit( arrTy->desugar().getTypePtr() );

		const core::ASTBuilder& builder = convFact.builder;
		core::TypePtr&& elemTy = Visit( arrTy->getElementType().getTypePtr() );
		assert(elemTy && "Conversion of array element type failed.");

		core::TypePtr retTy = builder.arrayType( builder.refType(elemTy) );
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// 						DEPENDENT SIZED ARRAY TYPE
	// This type represents an array type in C++ whose size is a value-dependent
	// expression. For example:
	//
	//  template<typename T, int Size>
	//  class array {
	//     T data[Size];
	//  };
	//
	// For these types, we won't actually know what the array bound is until
	// template instantiation occurs, at which point this will become either
	// a ConstantArrayType or a VariableArrayType.
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitDependentSizedArrayType(DependentSizedArrayType* arrTy) {
		assert(false && "DependentSizedArrayType not yet handled!");
	}

	// --------------------  FUNCTIONS  ---------------------------------------
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//				FUNCTION PROTO TYPE
	// Represents a prototype with argument type info, e.g. 'int foo(int)' or
	// 'int foo(void)'. 'void' is represented as having no arguments, not as
	// having a single void argument. Such a type can have an exception
	// specification, but this specification is not part of the canonical type.
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitFunctionProtoType(FunctionProtoType* funcTy) {
		START_LOG_TYPE_CONVERSION(funcTy);

		const core::ASTBuilder& builder = convFact.builder;
		core::TypePtr&& retTy = Visit( funcTy->getResultType().getTypePtr() );
		assert(retTy && "Function has no return type!");

		core::TupleType::ElementTypeList argTypes;
		std::for_each(funcTy->arg_type_begin(), funcTy->arg_type_end(),
			[ &argTypes, this ] (const QualType& currArgType) {
				this->ctx.isResolvingFunctionType = true;
				core::TypePtr&& argTy = this->Visit( currArgType.getTypePtr() );
				argTypes.push_back( argTy );
				this->ctx.isResolvingFunctionType = false;
			}
		);

		if( argTypes.size() == 1 && *argTypes.front() == core::lang::TYPE_UNIT_VAL) {
			// we have only 1 argument, and it is a unit type (void), remove it from the list
			argTypes.clear();
		}

		if( funcTy->isVariadic() )
			argTypes.push_back( core::lang::TYPE_VAR_LIST );

		retTy = builder.functionType( builder.tupleType(argTypes), retTy);
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//					FUNCTION NO PROTO TYPE
	// Represents a K&R-style 'int foo()' function, which has no information
	// available about its arguments.
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitFunctionNoProtoType(FunctionNoProtoType* funcTy) {
		START_LOG_TYPE_CONVERSION( funcTy );
		core::TypePtr&& retTy = Visit( funcTy->getResultType().getTypePtr() );
		assert(retTy && "Function has no return type!");

		retTy = convFact.builder.functionType( convFact.builder.tupleType(), retTy);
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	// TBD
//	TypeWrapper VisitVectorType(VectorType* vecTy) {	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// 							EXTENDEND VECTOR TYPE
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitExtVectorType(ExtVectorType* vecTy) {
       // get vector datatype
        const QualType qt = vecTy->getElementType();
        const BuiltinType* buildInTy = dyn_cast<const BuiltinType>( qt->getUnqualifiedDesugaredType() );
        core::TypePtr&& subType = Visit(const_cast<BuiltinType*>(buildInTy));

        // get the number of elements
        size_t num = vecTy->getNumElements();
        core::IntTypeParam numElem = core::IntTypeParam::getConcreteIntParam(num);

        //note: members of OpenCL vectors are always modifiable
        return convFact.builder.vectorType( convFact.builder.refType(subType), numElem);
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// 								TYPEDEF TYPE
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitTypedefType(TypedefType* typedefType) {
		START_LOG_TYPE_CONVERSION( typedefType );

        core::TypePtr&& subType = Visit( typedefType->getDecl()->getUnderlyingType().getTypePtr() );
        // Adding the name of the typedef as annotation
        subType.addAnnotation(std::make_shared<insieme::c_info::CNameAnnotation>(typedefType->getDecl()->getNameAsString()));

        END_LOG_TYPE_CONVERSION( subType );
        return  subType;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// 								TYPE OF TYPE
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitTypeOfType(TypeOfType* typeOfType) {
		START_LOG_TYPE_CONVERSION(typeOfType);
		core::TypePtr retTy = convFact.builder.getUnitType();
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// 							TYPE OF EXPRESSION TYPE
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitTypeOfExprType(TypeOfExprType* typeOfType) {
		START_LOG_TYPE_CONVERSION( typeOfType );
		core::TypePtr&& retTy = Visit( GET_TYPE_PTR(typeOfType->getUnderlyingExpr()) );
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//					TAG TYPE: STRUCT | UNION | CLASS | ENUM
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitTagType(TagType* tagType) {
		if(!ctx.recVarMap.empty()) {
			// check if this type has a typevar already associated, in such case return it
			ConversionContext::TypeRecVarMap::const_iterator fit = ctx.recVarMap.find(tagType);
			if( fit != ctx.recVarMap.end() ) {
				// we are resolving a parent recursive type, so we shouldn't
				return fit->second;
			}
		}

		// check if the type is in the cache of already solved recursive types
		// this is done only if we are not resolving a recursive sub type
		if(!ctx.isRecSubType) {
			ConversionContext::RecTypeMap::const_iterator rit = ctx.recTypeCache.find(tagType);
			if(rit != ctx.recTypeCache.end())
				return rit->second;
		}

		START_LOG_TYPE_CONVERSION(tagType);

		// will store the converted type
		core::TypePtr retTy;
		DVLOG(2) << "Converting TagType: " << tagType->getDecl()->getName().str();

		const TagDecl* tagDecl = tagType->getDecl()->getCanonicalDecl();
		// iterate through all the re-declarations to see if one of them provides a definition
		TagDecl::redecl_iterator i,e = tagDecl->redecls_end();
		for(i = tagDecl->redecls_begin(); i != e && !i->isDefinition(); ++i) ;
		if(i != e) {
			tagDecl = tagDecl->getDefinition();
			// we found a definition for this declaration, use it
			assert(tagDecl->isDefinition() && "TagType is not a definition");

			if(tagDecl->getTagKind() == clang::TTK_Enum) {
				assert(false && "Enum types not supported yet");
			} else {
				// handle struct/union/class
				const RecordDecl* recDecl = dyn_cast<const RecordDecl>(tagDecl);
				assert(recDecl && "TagType decl is not of a RecordDecl type!");

				if(!ctx.isRecSubType) {
					// add this type to the type graph (if not present)
					ctx.typeGraph.addNode(tagDecl->getTypeForDecl());
				}

				// retrieve the strongly connected componenets for this type
				std::set<const Type*>&& components = ctx.typeGraph.getStronglyConnectedComponents(tagDecl->getTypeForDecl());

				if( !components.empty() ) {
					if(VLOG_IS_ON(2)) {
						// we are dealing with a recursive type
						VLOG(2) << "Analyzing RecordDecl: " << recDecl->getNameAsString() << std::endl
								<< "Number of components in the cycle: " << components.size();
						std::for_each(components.begin(), components.end(),
							[] (std::set<const Type*>::value_type c) {
								assert(isa<const TagType>(c));
								VLOG(2) << "\t" << dyn_cast<const TagType>(c)->getDecl()->getNameAsString();
							}
						);
						ctx.typeGraph.print(std::cerr);
					}

					// we create a TypeVar for each type in the mutual dependence
					ctx.recVarMap.insert( std::make_pair(tagType, convFact.builder.typeVariable(recDecl->getName())) );

					// when a subtype is resolved we aspect to already have these variables in the map
					if(!ctx.isRecSubType) {
						std::for_each(components.begin(), components.end(),
							[ this ] (std::set<const Type*>::value_type ty) {
								const TagType* tagTy = dyn_cast<const TagType>(ty);
								assert(tagTy && "Type is not of TagType type");

								this->ctx.recVarMap.insert( std::make_pair(ty, convFact.builder.typeVariable(tagTy->getDecl()->getName())) );
							}
						);
					}
				}

				// Visit the type of the fields recursively
				// Note: if a field is referring one of the type in the cyclic dependency, a reference
				//       to the TypeVar will be returned.
				core::NamedCompositeType::Entries structElements;
				for(RecordDecl::field_iterator it=recDecl->field_begin(), end=recDecl->field_end(); it != end; ++it) {
					RecordDecl::field_iterator::value_type curr = *it;
					const Type* fieldType = curr->getType().getTypePtr();
					structElements.push_back(
							core::NamedCompositeType::Entry(core::Identifier(curr->getNameAsString()), Visit( const_cast<Type*>(fieldType) ))
					);
				}

				// build a struct or union IR type
				retTy = handleTagType(tagDecl, structElements);

				if( !components.empty() ) {
					// if we are visiting a nested recursive type it means someone else will take care
					// of building the rectype node, we just return an intermediate type
					if(ctx.isRecSubType)
						return retTy;

					// we have to create a recursive type
					ConversionContext::TypeRecVarMap::const_iterator tit = ctx.recVarMap.find(tagType);
					assert(tit != ctx.recVarMap.end() && "Recursive type has not TypeVar associated to himself");
					core::TypeVariablePtr recTypeVar = tit->second;

					core::RecTypeDefinition::RecTypeDefs definitions;
					definitions.insert( std::make_pair(recTypeVar, handleTagType(tagDecl, structElements) ) );

					// We start building the recursive type. In order to avoid loop the visitor
					// we have to change its behaviour and let him returns temporarely types
					// when a sub recursive type is visited.
					ctx.isRecSubType = true;

					std::for_each(components.begin(), components.end(),
						[ this, &definitions ] (std::set<const Type*>::value_type ty) {
							const TagType* tagTy = dyn_cast<const TagType>(ty);
							assert(tagTy && "Type is not of TagType type");

							//Visual Studio 2010 fix: full namespace
							insieme::frontend::conversion::ConversionFactory::ConversionContext::TypeRecVarMap::const_iterator tit =
									this->ctx.recVarMap.find(ty);

							assert(tit != this->ctx.recVarMap.end() && "Recursive type has no TypeVar associated");
							core::TypeVariablePtr var = tit->second;

							// we remove the variable from the list in order to fool the solver,
							// in this way it will create a descriptor for this type (and he will not return the TypeVar
							// associated with this recursive type). This behaviour is enabled only when the isRecSubType
							// flag is true
							this->ctx.recVarMap.erase(ty);

							definitions.insert( std::make_pair(var, this->Visit(const_cast<Type*>(ty))) );
							var.addAnnotation( std::make_shared<insieme::c_info::CNameAnnotation>(tagTy->getDecl()->getNameAsString()) );

							// reinsert the TypeVar in the map in order to solve the other recursive types
							this->ctx.recVarMap.insert( std::make_pair(tagTy, var) );
						}
					);
					// we reset the behavior of the solver
					ctx.isRecSubType = false;
					// the map is also erased so visiting a second type of the mutual cycle will yield a correct result
					ctx.recVarMap.clear();

					core::RecTypeDefinitionPtr&& definition = convFact.builder.recTypeDefinition(definitions);
					retTy = convFact.builder.recType(recTypeVar, definition);

					// Once we solved this recursive type, we add to a cache of recursive types
					// so next time we encounter it, we don't need to compute the graph
					ctx.recTypeCache.insert(std::make_pair(tagType, retTy));
				}

				// Adding the name of the C struct as annotation
				retTy.addAnnotation( std::make_shared<insieme::c_info::CNameAnnotation>(recDecl->getName()) );
			}
		} else {
			// We didn't find any definition for this type, so we use a name and define it as a generic type
			retTy = convFact.builder.genericType( tagDecl->getNameAsString() );
		}
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							ELABORATED TYPE (TODO)
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitElaboratedType(ElaboratedType* elabType) {
		assert(false && "ElaboratedType not yet handled!");
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							POINTER TYPE (FIXME)
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitPointerType(PointerType* pointerTy) {
		START_LOG_TYPE_CONVERSION(pointerTy);

		core::TypePtr&& subTy = Visit(pointerTy->getPointeeType().getTypePtr());
		// ~~~~~ Handling of special cases ~~~~~~~
		// void* -> ref<'a>
		if(*subTy == core::lang::TYPE_UNIT_VAL)
			subTy = core::lang::TYPE_ALPHA_PTR;
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		core::TypePtr&& retTy = convFact.builder.arrayType( convFact.builder.refType(subTy) );
		END_LOG_TYPE_CONVERSION( retTy );
		return retTy;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						REFERENCE TYPE (FIXME)
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::TypePtr VisitReferenceType(ReferenceType* refTy) {
		return convFact.builder.refType( Visit( refTy->getPointeeType().getTypePtr()) );
	}

private:

	core::TypePtr handleTagType(const TagDecl* tagDecl, const core::NamedCompositeType::Entries& structElements) {
		if( tagDecl->getTagKind() == clang::TTK_Struct || tagDecl->getTagKind() ==  clang::TTK_Class ) {
			return convFact.builder.structType( structElements );
		} else if( tagDecl->getTagKind() == clang::TTK_Union ) {
			return convFact.builder.unionType( structElements );
		}
		assert(false && "TagType not supported");
	}
};

// ------------------------------------ ConversionFactory ---------------------------

ConversionFactory::ConversionFactory(core::SharedNodeManager mgr, const ClangCompiler& clang, clang::idx::Indexer& indexer,
	clang::idx::Program& clangProg, const PragmaList& pragmaList):
	// cppcheck-suppress exceptNew
	ctx(new ConversionContext),
	mgr(mgr),  builder(mgr), clangComp(clang), indexer(indexer), clangProg(clangProg), pragmaMap(pragmaList),
	// cppcheck-suppress exceptNew
	typeConv( new ClangTypeConverter(*this) ),
	// cppcheck-suppress exceptNew
	exprConv( new ClangExprConverter(*this) ),
	// cppcheck-suppress exceptNew
	stmtConv( new ClangStmtConverter(*this) ) { }

core::TypePtr ConversionFactory::convertType(const clang::Type* type) const {
	assert(type && "Calling convertType with a NULL pointer");
	return typeConv->Visit( const_cast<Type*>(type) );
}

core::StatementPtr ConversionFactory::convertStmt(const clang::Stmt* stmt) const {
	assert(stmt && "Calling convertStmt with a NULL pointer");
	return stmtConv->Visit( const_cast<Stmt*>(stmt) ).getSingleStmt();
}

core::ExpressionPtr ConversionFactory::convertExpr(const clang::Expr* expr) const {
	assert(expr && "Calling convertExpr with a NULL pointer");
	return exprConv->Visit( const_cast<Expr*>(expr) );
}

/* Function to convert Clang attributes of declarations to IR annotations (local version)
 * currently used for:
 * 	* OpenCL address spaces
 */
core::AnnotationPtr ConversionFactory::convertAttribute(const clang::VarDecl* varDecl) const {
    if(!varDecl->hasAttrs())
    	return core::AnnotationPtr();

	const AttrVec attrVec = varDecl->getAttrs();
	std::ostringstream ss;
	ocl::BaseAnnotation::AnnotationList declAnnotation;
	try {
	for(AttrVec::const_iterator I = attrVec.begin(), E = attrVec.end(); I != E; ++I) {
		if(AnnotateAttr* attr = dyn_cast<AnnotateAttr>(*I)) {
			std::string sr = attr->getAnnotation().str();

			//check if the declaration has attribute __private
			if(sr == "__private") {
				DVLOG(2) << "           OpenCL address space __private";
				declAnnotation.push_back(std::make_shared<ocl::AddressSpaceAnnotation>( ocl::AddressSpaceAnnotation::addressSpace::PRIVATE ));
				continue;
			}

			//check if the declaration has attribute __local
			if(sr == "__local") {
				DVLOG(2) << "           OpenCL address space __local";
				declAnnotation.push_back(std::make_shared<ocl::AddressSpaceAnnotation>( ocl::AddressSpaceAnnotation::addressSpace::LOCAL ));
				continue;
			}

            // TODO global also for global variables

			//check if the declaration has attribute __global
			if(sr == "__global") {
				// keywords global and local are only allowed for parameters
				if(isa<const clang::ParmVarDecl>(varDecl)) {
					DVLOG(2) << "           OpenCL address space __global";
					declAnnotation.push_back(std::make_shared<ocl::AddressSpaceAnnotation>( ocl::AddressSpaceAnnotation::addressSpace::GLOBAL ));
					continue;
				}
				ss << "Address space __global not allowed for local variable";
				throw &ss;
			}

			//check if the declaration has attribute __constant
			if(sr == "__constant") {
				if(isa<const clang::ParmVarDecl>(varDecl)) {
					DVLOG(2) << "           OpenCL address space __constant";
					declAnnotation.push_back(std::make_shared<ocl::AddressSpaceAnnotation>( ocl::AddressSpaceAnnotation::addressSpace::CONSTANT ));					continue;
				}
				ss << "Address space __constant not allowed for local variable";
				throw &ss;
			}
		}

		// Throw an error if an unhandled attribute is found
		ss << "Unexpected attribute";
		throw &ss;
	}}
	catch(std::ostringstream *errMsg) {
        //show errors if unexpected patterns were found
        printErrorMsg(*errMsg, clangComp, varDecl);
	}
	return std::make_shared<ocl::BaseAnnotation>(declAnnotation);
}

core::VariablePtr ConversionFactory::lookUpVariable(const clang::VarDecl* varDecl) {
	ConversionContext::VarDeclMap::const_iterator fit = ctx->varDeclMap.find(varDecl);
	if(fit != ctx->varDeclMap.end())
		// variable found in the map, return it
		return fit->second;

	QualType&& varTy = varDecl->getType();
	core::TypePtr&& type = convertType( varTy.getTypePtr() );
	if(!varTy.isConstQualified() && !isa<const clang::ParmVarDecl>(varDecl)) {
		// add a ref in the case of variable which are not const or declared as function parameters
		type = builder.refType(type);
	}
	// variable is not in the map, create a new var and add it
	core::VariablePtr&& var = builder.variable(type);
	// add the var in the map
	ctx->varDeclMap.insert( std::make_pair(varDecl, var) );

	// Add the C name of this variable as annotation
	var->addAnnotation( std::make_shared<c_info::CNameAnnotation>(varDecl->getNameAsString()) );

	// Add OpenCL attributes
	core::AnnotationPtr&& attr = convertAttribute(varDecl);
	if(attr)
		var->addAnnotation(attr);
	return var;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//						CONVERT VARIABLE DECLARATION
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

core::ExpressionPtr ConversionFactory::defaultInitVal(const clang::Type* ty, const core::TypePtr type ) {
    if ( ty->isIntegerType() || ty->isUnsignedIntegerType() ) {
        // initialize integer value
        return builder.literal("0", type);
    }
    if( ty->isFloatingType() || ty->isRealType() || ty->isRealFloatingType() ) {
        // in case of floating types we initialize with a zero value
        return builder.literal("0.0", type);
    }
    if ( ty->isAnyPointerType() || ty->isRValueReferenceType() || ty->isLValueReferenceType() ) {
        // initialize pointer/reference types with the null value
        return core::lang::CONST_NULL_PTR_PTR;
    }
    if ( ty->isCharType() || ty->isAnyCharacterType() ) {
        return builder.literal("", type);
    }
    if ( ty->isBooleanType() ) {
        // boolean values are initialized to false
        return builder.literal("false", core::lang::TYPE_BOOL_PTR);
    }

    //----------------  INTIALIZE VECTORS ---------------------------------
    const Type* elemTy = NULL;
    size_t arraySize = 0;
    if ( ty->isExtVectorType() ) {
    	const TypedefType* typedefType = dyn_cast<const TypedefType>(ty);
        assert(typedefType && "ExtVectorType has unexpected class");
        const ExtVectorType* vecTy = dyn_cast<const ExtVectorType>( typedefType->getDecl()->getUnderlyingType().getTypePtr() );
        assert(vecTy && "ExtVectorType has unexpected class");

        elemTy = vecTy->getElementType()->getUnqualifiedDesugaredType();
		arraySize = vecTy->getNumElements();
    }
    if ( ty->isConstantArrayType() ) {
    	const ConstantArrayType* arrTy = dyn_cast<const ConstantArrayType>(ty);
		assert(arrTy && "ConstantArrayType has unexpected class");

		elemTy = arrTy->getElementType()->getUnqualifiedDesugaredType();
		arraySize = *arrTy->getSize().getRawData();
    }
    if( ty->isExtVectorType() || ty->isConstantArrayType() ) {
    	assert(elemTy);
    	core::ExpressionPtr&& initVal = defaultInitVal(elemTy, convertType(elemTy) );
    	return builder.vectorExpr( std::vector<core::ExpressionPtr>(arraySize, initVal) );
    }

    assert(false && "Default initialization type not defined");
    return core::lang::CONST_NULL_PTR_PTR;
}

core::DeclarationStmtPtr ConversionFactory::convertVarDecl(const clang::VarDecl* varDecl) {
	// logging
	DVLOG(1) << "\n****************************************************************************************\n"
			 << "Converting VarDecl [class: '" << varDecl->getDeclKindName() << "']\n"
			 << "-> at location: (" << utils::location(varDecl->getLocation(), clangComp.getSourceManager()) << "): ";
	if( VLOG_IS_ON(2) ) { \
		DVLOG(2) << "Dump of clang VarDecl: \n"
				 << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
		varDecl->dump();
	}

	clang::QualType clangType = varDecl->getType();
	if(!clangType.isCanonical())
		clangType = clangType->getCanonicalTypeInternal();

	// we cannot analyze if the variable will be modified or not, so we make it of type ref<a'> if
	// it is not declared as const, successive dataflow analysis could be used to restrict the access
	// to this variable
	core::TypePtr&& type = convertType( clangType.getTypePtr() );
	if(!clangType.isConstQualified() && !isa<clang::ParmVarDecl>(varDecl))
		type = builder.refType( type );

	// initialization value
	core::ExpressionPtr initExpr;
	if( varDecl->getInit() ) {
		initExpr = convertExpr( varDecl->getInit() );
	} else {
		initExpr = defaultInitVal( GET_TYPE_PTR(varDecl), type);
	}

	// REF.VAR
	initExpr = builder.callExpr( type, core::lang::OP_REF_VAR_PTR, toVector(initExpr) );

	// lookup for the variable in the map
	core::VariablePtr var = lookUpVariable(varDecl);
	core::DeclarationStmtPtr&& retStmt = builder.declarationStmt( var, initExpr );

	// logging
	DVLOG(1) << "Converted into IR stmt: "; \
	DVLOG(1) << "\t" << *retStmt;

	return retStmt;
}

void ConversionFactory::attachFuncAnnotations(core::ExpressionPtr& node, const clang::FunctionDecl* funcDecl) {
	// ---------------------- Add annotations to this function ------------------------------
	//check Attributes of the function definition
	ocl::BaseAnnotation::AnnotationList kernelAnnotation;
	if(funcDecl->hasAttrs()) {
		const clang::AttrVec attrVec = funcDecl->getAttrs();

		for(AttrVec::const_iterator I = attrVec.begin(), E = attrVec.end(); I != E; ++I) {
			if(AnnotateAttr* attr = dyn_cast<AnnotateAttr>(*I)) {
				//get annotate string
				llvm::StringRef sr = attr->getAnnotation();

				//check if it is an OpenCL kernel function
				if(sr == "__kernel") {
					DVLOG(1) << "is OpenCL kernel function";
					kernelAnnotation.push_back( std::make_shared<ocl::KernelFctAnnotation>() );
				}
			}
			else if(ReqdWorkGroupSizeAttr* attr = dyn_cast<ReqdWorkGroupSizeAttr>(*I)) {
				kernelAnnotation.push_back(std::make_shared<ocl::WorkGroupSizeAnnotation>(
						attr->getXDim(), attr->getYDim(), attr->getZDim())
				);
			}
		}
	}
	// --------------------------------- OPENCL ---------------------------------------------
	// if OpenCL related annotations have been found, create OclBaseAnnotation and
	// add it to the funciton's attribute
	if(!kernelAnnotation.empty())
		node.addAnnotation( std::make_shared<ocl::BaseAnnotation>(kernelAnnotation) );

	// --------------------------------- C NAME ----------------------------------------------
	// annotate with the C name of the function
	node.addAnnotation( std::make_shared<c_info::CNameAnnotation>( funcDecl->getName() ) );

	// ----------------------- SourceLocation Annotation -------------------------------------
	// for each entry function being converted we register the location where it was originally
	// defined in the C program
	auto convertClangSrcLoc = [ this ](const SourceLocation& loc) -> c_info::SourceLocation {
		SourceManager& sm = this->clangComp.getSourceManager();
		FileID&& fileId = sm.getFileID(loc);
		const clang::FileEntry* fileEntry = sm.getFileEntryForID(fileId);
		return c_info::SourceLocation(fileEntry->getName(), sm.getSpellingLineNumber(loc), sm.getSpellingColumnNumber(loc));
	};

	node.addAnnotation( std::make_shared<c_info::CLocAnnotation>(
			convertClangSrcLoc(funcDecl->getLocStart()), convertClangSrcLoc(funcDecl->getLocEnd()) ) );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//						CONVERT FUNCTION DECLARATION
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
core::ExpressionPtr ConversionFactory::convertFunctionDecl(const clang::FunctionDecl* funcDecl) {
	// the function is not extern, a lambdaExpr has to be created
	assert(funcDecl->hasBody() && "Function has no body!");
	DVLOG(1) << "#----------------------------------------------------------------------------------#";
	DVLOG(1) << "\nVisiting Function Declaration for: " << funcDecl->getNameAsString() << std::endl
			 << "\tIsRecSubType: " << ctx->isRecSubFunc << std::endl
			 << "\tEmpty map: "    << ctx->recVarExprMap.size();

	if(!ctx->isRecSubFunc) {
		// add this type to the type graph (if not present)
		ctx->funcDepGraph.addNode(funcDecl);
		if( VLOG_IS_ON(2) ) {
			ctx->funcDepGraph.print( std::cout );
		}
	}

	// retrieve the strongly connected components for this type
	std::set<const FunctionDecl*>&& components = ctx->funcDepGraph.getStronglyConnectedComponents( funcDecl );

	if( !components.empty() ) {
		// we are dealing with a recursive type
		DVLOG(1) << "Analyzing FuncDecl: " << funcDecl->getNameAsString() << std::endl
				 << "Number of components in the cycle: " << components.size();
		std::for_each(components.begin(), components.end(),
			[ ] (std::set<const FunctionDecl*>::value_type c) {
				DVLOG(2) << "\t" << c->getNameAsString( ) << "(" << c->param_size() << ")";
			}
		);

		if(!ctx->isRecSubFunc) {
			if(ctx->recVarExprMap.find(funcDecl) == ctx->recVarExprMap.end()) {
				// we create a TypeVar for each type in the mutual dependence
				core::VariablePtr&& var = builder.variable( convertType( GET_TYPE_PTR(funcDecl) ) );
				ctx->recVarExprMap.insert( std::make_pair(funcDecl, var) );
				var->addAnnotation( std::make_shared<c_info::CNameAnnotation>( funcDecl->getNameAsString() ) );
			}
		} else {
			// we expect the var name to be in currVar
			ctx->recVarExprMap.insert(std::make_pair(funcDecl, ctx->currVar));
		}

		// when a subtype is resolved we expect to already have these variables in the map
		if(!ctx->isRecSubFunc) {
			std::for_each(components.begin(), components.end(),
				[ this ] (std::set<const FunctionDecl*>::value_type fd) {

					// we count how many variables in the map refers to overloaded versions of the same function
					// this can happen when a function get overloaded and the cycle of recursion can happen between
					// the overloaded version, we need unique variable for each version of the function

					if(this->ctx->recVarExprMap.find(fd) == this->ctx->recVarExprMap.end()) {
						core::VariablePtr&& var = this->builder.variable( this->convertType(GET_TYPE_PTR(fd)) );
						this->ctx->recVarExprMap.insert( std::make_pair(fd, var ) );
						var->addAnnotation( std::make_shared<c_info::CNameAnnotation>( fd->getNameAsString() ) );
					}
				}
			);
		}
		if( VLOG_IS_ON(2) ) {
			DVLOG(2) << "MAP: ";
			std::for_each(ctx->recVarExprMap.begin(), ctx->recVarExprMap.end(),
				[] (ConversionContext::RecVarExprMap::value_type c) {
					DVLOG(2) << "\t" << c.first->getNameAsString() << "[" << c.first << "]";
				}
			);
		}
	}

	core::ExpressionPtr retLambdaExpr;

	vector<core::VariablePtr> params;
	std::for_each(funcDecl->param_begin(), funcDecl->param_end(),
		[ &params, this ] (ParmVarDecl* currParam) {
			params.push_back( this->lookUpVariable(currParam) );
		}
	);

	// this lambda is not yet in the map, we need to create it and add it to the cache
	assert(!ctx->isResolvingRecFuncBody && "~~~ Something odd happened ~~~");
	if(!components.empty())
		ctx->isResolvingRecFuncBody = true;
	core::StatementPtr&& body = convertStmt( funcDecl->getBody() );
	ctx->isResolvingRecFuncBody = false;

	retLambdaExpr = builder.lambdaExpr( convertType( GET_TYPE_PTR(funcDecl) ), params, body);

	if( components.empty() ) {
		attachFuncAnnotations(retLambdaExpr, funcDecl);
		return retLambdaExpr;
	}

	// this is a recurive function call
	if(ctx->isRecSubFunc) {
		// if we are visiting a nested recursive type it means someone else will take care
		// of building the rectype node, we just return an intermediate type
		return retLambdaExpr;
	}

	// we have to create a recursive type
	ConversionContext::RecVarExprMap::const_iterator tit = ctx->recVarExprMap.find(funcDecl);
	assert(tit != ctx->recVarExprMap.end() && "Recursive function has not VarExpr associated to himself");
	core::VariablePtr recVarRef = tit->second;

	core::RecLambdaDefinition::RecFunDefs definitions;
	definitions.insert( std::make_pair(recVarRef, core::dynamic_pointer_cast<const core::LambdaExpr>(retLambdaExpr)) );

	// We start building the recursive type. In order to avoid loop the visitor
	// we have to change its behaviour and let him returns temporarely types
	// when a sub recursive type is visited.
	ctx->isRecSubFunc = true;

	std::for_each(components.begin(), components.end(),
		[ this, &definitions ] (std::set<const FunctionDecl*>::value_type fd) {

			//Visual Studios 2010 fix: full namespace
			insieme::frontend::conversion::ConversionFactory::ConversionContext::RecVarExprMap::const_iterator tit = this->ctx->recVarExprMap.find(fd);
			assert(tit != this->ctx->recVarExprMap.end() && "Recursive function has no TypeVar associated");
			this->ctx->currVar = tit->second;

			// we remove the variable from the list in order to fool the solver,
			// in this way it will create a descriptor for this type (and he will not return the TypeVar
			// associated with this recursive type). This behaviour is enabled only when the isRecSubType
			// flag is true
			this->ctx->recVarExprMap.erase(fd);
			definitions.insert( std::make_pair(this->ctx->currVar, core::dynamic_pointer_cast<const core::LambdaExpr>(this->convertFunctionDecl(fd)) ) );

			// reinsert the TypeVar in the map in order to solve the other recursive types
			this->ctx->recVarExprMap.insert( std::make_pair(fd, this->ctx->currVar) );
			this->ctx->currVar = NULL;
		}
	);
	// we reset the behavior of the solver
	ctx->isRecSubFunc = false;
	// the map is also erased so visiting a second type of the mutual cycle will yield a correct result
	// ctx->recVarExprMap.clear();

	core::RecLambdaDefinitionPtr definition = builder.recLambdaDefinition(definitions);
	retLambdaExpr = builder.recLambdaExpr(recVarRef, definition);

	// Adding the lambda function to the list of converted functions
	ctx->lambdaExprCache.insert( std::make_pair(funcDecl, retLambdaExpr) );
	// we also need to cache all the other recursive definition, so when we will resolve
	// another function in the recursion we will not repeat the process again
	std::for_each(components.begin(), components.end(),
		[ this, &definition ] (std::set<const FunctionDecl*>::value_type fd) {
			auto fit = this->ctx->recVarExprMap.find(fd);
			assert(fit != this->ctx->recVarExprMap.end());
			core::ExpressionPtr&& func = builder.recLambdaExpr(fit->second, definition);
			ctx->lambdaExprCache.insert( std::make_pair(fd, func) );

			this->attachFuncAnnotations(func, fd);
		}
	);
	this->attachFuncAnnotations(retLambdaExpr, funcDecl);
	return retLambdaExpr;
}

// ------------------------------------ ClangTypeConverter ---------------------------

core::ProgramPtr ASTConverter::handleTranslationUnit(const clang::DeclContext* declCtx) {

	for(DeclContext::decl_iterator it = declCtx->decls_begin(), end = declCtx->decls_end(); it != end; ++it) {
		Decl* decl = *it;
		if(FunctionDecl* funcDecl = dyn_cast<FunctionDecl>(decl)) {
			// finds a definition of this function if any
			const FunctionDecl* definition = NULL;
			// if this function is just a declaration, and it has no definition, we just skip it
			if(!funcDecl->hasBody(definition))
				continue;

			core::ExpressionPtr&& lambdaExpr = mFact.convertFunctionDecl(definition);
			if(definition->isMain())
				mProgram = core::Program::addEntryPoint(*mFact.getNodeManager(), mProgram, lambdaExpr);
		}
//		else if(VarDecl* varDecl = dyn_cast<VarDecl>(decl)) {
//			mFact.convertVarDecl( varDecl );
//		}
	}

	return mProgram;
}

core::LambdaExprPtr ASTConverter::handleBody(const clang::Stmt* body) {
	core::StatementPtr&& bodyStmt = mFact.convertStmt( body );
	core::CallExprPtr&& callExpr = createCallExpr(mFact.getASTBuilder(), toVector<core::StatementPtr>(bodyStmt), core::lang::TYPE_UNIT);

	return core::dynamic_pointer_cast<const core::LambdaExpr>(callExpr->getFunctionExpr());
}


} // End conversion namespace
} // End frontend namespace
} // End insieme namespace
