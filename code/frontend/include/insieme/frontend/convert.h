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

#pragma once

#include "insieme/core/program.h"
#include "insieme/core/ast_builder.h"

#include "insieme/frontend/pragma_handler.h"
#include "insieme/utils/map_utils.h"

#include <functional>

// Forward declarations
namespace clang {
class ASTContext;
class DeclGroupRef;
class FunctionDecl;
class InitListExpr;
namespace idx {
class Indexer;
class Program;
} // End idx namespace
} // End clang namespace


namespace {
typedef vector<insieme::core::StatementPtr>  StatementList;
typedef vector<insieme::core::ExpressionPtr> ExpressionList;

#define GET_TYPE_PTR(type) (type)->getType().getTypePtr()
}

namespace insieme {
namespace frontend {
namespace conversion {

class ASTConverter;

// ------------------------------------ ConversionFactory ---------------------------
/**
 * A factory used to convert clang AST nodes (i.e. statements, expressions and types) to Insieme IR nodes.
 */
class ConversionFactory : public boost::noncopyable {

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							ConversionContext
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Keeps all the information gathered during the conversion process.
	// Maps for variable names, cached resolved function definitions and so on...
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	struct ConversionContext :  public boost::noncopyable {

		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// 						Recursive Function resolution
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// Maps Clang variable declarations (VarDecls and ParmVarDecls) to IR variables.
		typedef std::map<const clang::ValueDecl*, core::VariablePtr> VarDeclMap;
		VarDeclMap varDeclMap;

		// Stores the generated IR for function declarations
		typedef std::map<const clang::FunctionDecl*, insieme::core::ExpressionPtr> LambdaExprMap;
		LambdaExprMap lambdaExprCache;

		/*
		 * Maps a function with the variable which has been introduced to represent
		 * the function in the recursive definition
		 */
		typedef std::map<const clang::FunctionDecl*, insieme::core::VariablePtr> RecVarExprMap;
		RecVarExprMap recVarExprMap;

		/*
		 * When set this variable tells the frontend to resolve eventual recursive function call
		 * using the mu variables which has been previously placed in the recVarExprMap
		 */
		bool isRecSubFunc;

		// It tells the frontend the body of a recursive function is being resolved and
		// eventual functions which are already been resolved should be not converted again
		// but read from the map
		bool isResolvingRecFuncBody;

		// This variable points to the current mu variable representing the start of the recursion
		core::VariablePtr currVar;

		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// 						Recursive Type resolution
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		typedef std::map<const clang::Type*, insieme::core::TypeVariablePtr> TypeRecVarMap;
		TypeRecVarMap recVarMap;
		bool isRecSubType;

		typedef std::map<const clang::Type*, insieme::core::TypePtr> RecTypeMap;
		RecTypeMap recTypeCache;

		bool isResolvingFunctionType;

		typedef std::map<const clang::Type*, insieme::core::TypePtr> TypeCache;
		TypeCache typeCache;
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// 						Global variables utility
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		typedef std::pair<core::StructTypePtr, core::StructExprPtr> GlobalStructPair;
		// Keeps the type and initialization of the global variables within the entry point
		GlobalStructPair globalStruct;

		// Global and static variables
		core::VariablePtr globalVar;

		/*
		 * Set of the function which need access to global variables, every time such a
		 * function is converted the data structure containing global variables has to
		 * be correctly forwarded by using the capture list
		 */
		typedef std::set<const clang::FunctionDecl*> UseGlobalFuncMap;
		UseGlobalFuncMap globalFuncMap;

		/*
		 * Every time an input parameter of a function of type 'a is improperly used as a ref<'a>
		 * a new variable is created in function body and the value of the input parameter assigned to it
		 */
		typedef utils::map::PointerMap<insieme::core::VariablePtr, insieme::core::VariablePtr> WrapRefMap;
		WrapRefMap wrapRefMap;

		ConversionContext() :
			isRecSubFunc(false), isResolvingRecFuncBody(false), isRecSubType(false), isResolvingFunctionType(false) { }
	};

	ConversionContext 		ctx;

	/**
	 * Converts a Clang statements into an IR statements.
	 */
	class ClangStmtConverter;
	// Instantiates the statement converter
	static ClangStmtConverter* makeStmtConvert(ConversionFactory& fact);
	// clean the memory
	static void cleanStmtConvert(ClangStmtConverter* stmtConv);
	ClangStmtConverter* stmtConv; // PIMPL pattern

	/**
	 * Converts a Clang types into an IR types.
	 */
	class ClangTypeConverter;
	// Instantiates the type converter
	static ClangTypeConverter* makeTypeConvert(ConversionFactory& fact, Program& program);
	// clean the memory
	static void cleanTypeConvert(ClangTypeConverter* typeConv);
	ClangTypeConverter* typeConv; // PIMPL pattern

	/**
	 * Converts a Clang expression into an IR expression.
	 */
	class ClangExprConverter;
	// Instantiates the expression converter
	static ClangExprConverter* makeExprConvert(ConversionFactory& fact, Program& program);
	// clean the memory
	static void cleanExprConvert(ClangExprConverter* exprConv);
	ClangExprConverter* exprConv; // PIMPL pattern

	core::NodeManager& 			mgr;
	const core::ASTBuilder  	builder;
    Program& 					program;

    /**
     * Maps of statements to pragmas.
     */
    PragmaStmtMap 	 		pragmaMap;

    /**
     * A pointer to the translation unit which is currently used to resolve symbols, i.e. literals
     * Every time a function belonging to a different translation unit is called this pointer
     * is set to translation unit containing the function definition.
     */
    const TranslationUnit*	currTU;

    /**
     * Returns a reference to the IR data structure used to represent a variable of the input C program.
     *
     * The function guarantees that the same variable in the input program is always represented in the
     * IR with the same generated Variable and in the case of access to global variables, a reference
     * to a member of the global data structure is returned.
     */
	core::ExpressionPtr lookUpVariable(const clang::ValueDecl* valDecl);
	core::ExpressionPtr convertInitializerList(const clang::InitListExpr* initList, const core::TypePtr& type) const;

	/**
	 * Attach annotations to a C function of the input program.
	 *
	 * returns the a MarkerExprPtr if a marker node has to be added and the passed node else
	 */
	core::ExpressionPtr attachFuncAnnotations(const core::ExpressionPtr& node, const clang::FunctionDecl* funcDecl);

	friend class ASTConverter;
public:

	typedef std::pair<clang::FunctionDecl*, clang::idx::TranslationUnit*> TranslationUnitPair;

	ConversionFactory(core::NodeManager& mgr, Program& program);
	~ConversionFactory();

	// Getters & Setters
	const core::ASTBuilder& getASTBuilder() const { return builder; }
	core::NodeManager& 	getNodeManager() const { return mgr; }
	const Program& getProgram() const { return program; }

	/**
	 * Force the current translation.
	 * @param tu new translation unit
	 */
	void setTranslationUnit(const TranslationUnit& tu) { currTU = &tu; }

	/**
	 * Because when literals are read from a function declaration we need to
	 * set manually the translation unit which contains the definition of the
	 * function, this method helps in setting the translation unit correctly.
	 *
	 * Returns the previous translation unit in the case it has to be set back. 
	 */
	const clang::idx::TranslationUnit* getTranslationUnitForDefinition(clang::FunctionDecl*& fd); 

	/**
	 * Returns a map which associates a statement of the clang AST to a pragma (if any)
	 * @return The statement to pragma multimap
	 */
	const PragmaStmtMap& getPragmaMap() const { return pragmaMap; }

	/**
	 * Entry point for converting clang types into an IR types
	 * @param type is a clang type
	 * @return the corresponding IR type
	 */
	core::TypePtr convertType(const clang::Type* type);

	/**
	 * Entry point for converting clang statements into IR statements
	 * @param stmt is a clang statement of the AST
	 * @return the corresponding IR statement
	 */
	core::StatementPtr convertStmt(const clang::Stmt* stmt) const;

	/**
	 * Entry point for converting clang expressions to IR expressions
	 * @param expr is a clang expression of the AST
	 * @return the corresponding IR expression
	 */
	core::ExpressionPtr convertExpr(const clang::Expr* expr) const;

	/**
	 * Converts a function declaration into an IR lambda.
	 * @param funcDecl is a clang FunctionDecl which represent a definition for the function
	 * @param isEntryPoint determine if this function is an entry point of the generated IR
	 * @return Converted lambda
	 */
	core::NodePtr convertFunctionDecl(const clang::FunctionDecl* funcDecl, bool isEntryPoint=false);

	/**
	 * Converts variable declarations into IR an declaration statement. This method is also responsible
	 * to map the generated IR variable with the translated variable declaration, so that later uses
	 * of the variable can be mapped to the same IR variable (see lookupVariable method).
	 * @param varDecl a clang variable declaration
	 * @return The IR translation of the variable declaration
	 */
	core::DeclarationStmtPtr convertVarDecl(const clang::VarDecl* varDecl);

	/**
	 * Returns the default initialization value of the IR type passed as input.
	 * @param type is the IR type
	 * @return The default initialization value for the IR type
	 */
	core::ExpressionPtr defaultInitVal(const core::TypePtr& type) const;

	core::ExpressionPtr convertInitExpr(const clang::Expr* expr, const core::TypePtr& type, const bool zeroInit) const;

	/**
	 * Looks for eventual attributes attached to the clang variable declarations (used for OpenCL implementation)
	 * and returns corresponding IR annotations to be attached to the IR corresponding declaration node.
	 * @param varDecl clang Variable declaration AST node
	 * @return IR annotation
	 */
	core::NodeAnnotationPtr convertAttribute(const clang::ValueDecl* valDecl) const;

	/**
	 * Utility function which tries to apply the deref operation. If the input expression is not a of ref type
	 * the same expression is returned.
	 * @param expr IR expression which could be of ref or non-ref type
	 * @return a non RefType IR expression
	 */
	core::ExpressionPtr tryDeref(const core::ExpressionPtr& expr) const;

	core::ExpressionPtr castToType(const core::TypePtr& trgTy, const core::ExpressionPtr& expr) const;
	// typedef std::function<core::ExpressionPtr (core::NodeManager&, const clang::CallExpr*)> CustomFunctionHandler;
	/**
	 * Registers a handler for call expressions. When a call expression to the provided function declaration 
	 * is encountered by the frontend, the provided handler is invoked. The handler produces an IR expression
	 * which will be used to replace the call expression in the generated IR program
	*/
	// void registerCallExprHandler(const clang::FunctionDecl* funcDecl, CustomFunctionHandler& handler);

// private:
//	typedef std::map<const clang::FunctionDecl*, CustomFunctionHandler> CallExprHandlerMap;
//	CallExprHandlerMap callExprHanlders;
};

struct GlobalVariableDeclarationException: public std::runtime_error {
	GlobalVariableDeclarationException() : std::runtime_error("") { }
};

// ------------------------------------ ASTConverter ---------------------------
/**
 *
 */
class ASTConverter {
	core::NodeManager&	mgr;
	Program& 			mProg;
	ConversionFactory   mFact;
	core::ProgramPtr    mProgram;

public:
	ASTConverter(core::NodeManager& mgr, Program& prog) :
		mgr(mgr), mProg(prog), mFact(mgr, prog), mProgram(prog.getProgram()) { }

	core::ProgramPtr getProgram() const { return mProgram; }

	core::ProgramPtr 	handleFunctionDecl(const clang::FunctionDecl* funcDecl, bool isMain=false);
	core::LambdaExprPtr	handleBody(const clang::Stmt* body, const TranslationUnit& tu);

};


} // End conversion namespace
} // End frontend namespace
} // End insieme namespace
