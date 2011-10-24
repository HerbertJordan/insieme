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

#include <stack>

#include "insieme/analysis/polyhedral/backends/isl_backend.h"

#include "insieme/core/ast_builder.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/transform/node_replacer.h"

#include "insieme/utils/logging.h"
#include "insieme/utils/map_utils.h"

#define CLOOG_INT_GMP
#include "cloog/cloog.h"
#include "cloog/isl/cloog.h"

using namespace insieme;
using namespace insieme::analysis::poly;

#ifdef CLOOG_INT_GMP 
#define PRINT_CLOOG_INT(out, val) \
	do { 										\
	char* str;										\
	cloog_int_print_gmp_free_t gmp_free;			\
	str = mpz_get_str(0, 10, val);					\
	out << str;										\
	mp_get_memory_functions(NULL, NULL, &gmp_free);	\
	(*gmp_free)(str, strlen(str)+1); }while(0)
#endif 

namespace {

core::ExpressionPtr buildMin(core::NodeManager& mgr, const core::ExpressionList& args) { 
	
	assert(args.size() == 2);
	core::ASTBuilder builder(mgr);
	return builder.callExpr(
			mgr.basic.getIfThenElse(), 
			builder.callExpr( mgr.basic.getSignedIntLt(), args[0], args[1] ),
			builder.createCallExprFromBody( builder.returnStmt(args[0]), mgr.basic.getInt4(), true ),
			builder.createCallExprFromBody( builder.returnStmt(args[1]), mgr.basic.getInt4(), true )
		);
}

template <class RetTy=void>
struct ClastVisitor {

	// Visit of Clast Stmts 
	virtual RetTy visitClastRoot(const clast_root* rootStmt) = 0;

	virtual RetTy visitClastAssignment(const clast_assignment* assignmentStmt) = 0;

	virtual RetTy visitClastBlock(const clast_block* blockStmt) = 0;

	virtual RetTy visitClastUser(const clast_user_stmt* userStmt) = 0;

	virtual RetTy visitClastFor(const clast_for* forStmt) = 0;

	virtual RetTy visitClastGuard(const clast_guard* guardStmt) = 0;

	virtual RetTy visitCloogStmt(const CloogStatement* cloogStmt) = 0;

	// Visit of Expr Stmts
	virtual RetTy visitClastName(const clast_name* nameExpr) = 0;

	virtual RetTy visitClastTerm(const clast_term* termExpr) = 0;

	virtual RetTy visitClastBinary(const clast_binary* binExpr) = 0;

	virtual RetTy visitClastReduction(const clast_reduction* redExpr) = 0;

	virtual RetTy visitClastEquation(const clast_equation* equation) = 0;

	virtual RetTy visit(const clast_expr* expr) {
		assert(expr && "Expression is not valid!");
		switch(expr->type) {
		case clast_expr_name:
			return visitClastName( reinterpret_cast<const clast_name*>(expr) );
		case clast_expr_term:
			return visitClastTerm( reinterpret_cast<const clast_term*>(expr) );
		case clast_expr_bin:
			return visitClastBinary( reinterpret_cast<const clast_binary*>(expr) );
		case clast_expr_red:
			return visitClastReduction( reinterpret_cast<const clast_reduction*>(expr) );
		default:
			assert(false && "Clast Expression not valid!");
		}

	}

	virtual RetTy visit(const clast_stmt* clast_node) {
		assert(clast_node && "Clast node is not valid");

		if( CLAST_STMT_IS_A(clast_node, stmt_root) )
			return visitClastRoot( reinterpret_cast<const clast_root*>(clast_node) );
		
		if( CLAST_STMT_IS_A(clast_node, stmt_ass) )
			return visitClastAssignment( reinterpret_cast<const clast_assignment*>(clast_node) );

		if( CLAST_STMT_IS_A(clast_node, stmt_user) )
			return visitClastUser( reinterpret_cast<const clast_user_stmt*>(clast_node) );

		if( CLAST_STMT_IS_A(clast_node, stmt_block) )
			return visitClastBlock( reinterpret_cast<const clast_block*>(clast_node) );

		if( CLAST_STMT_IS_A(clast_node, stmt_for) )
			return visitClastFor( reinterpret_cast<const clast_for*>(clast_node) );
	
		if( CLAST_STMT_IS_A(clast_node, stmt_guard) )
			return visitClastGuard( reinterpret_cast<const clast_guard*>(clast_node) );

		assert(false && "Clast node not supported");
	}

	virtual RetTy visit(const CloogStatement* cloogStmt) {
		return visitCloogStmt( cloogStmt );
	}
};

template <class RetTy=void>
struct RecClastVisitor: public ClastVisitor<RetTy> {

	// Statements 
	RetTy visitClastRoot(const clast_root* rootStmt) { 
		return RetTy();
	}

	virtual RetTy visitClastAssignment(const clast_assignment* assignmentStmt) {
		ClastVisitor<RetTy>::visit( assignmentStmt->RHS );

		return RetTy();
	}

	virtual RetTy visitClastBlock(const clast_block* blockStmt) {
		return ClastVisitor<RetTy>::visit( blockStmt->body );
	}

	virtual RetTy visitClastUser(const clast_user_stmt* userStmt) { 
		ClastVisitor<RetTy>::visit( userStmt->statement );
		if (userStmt->substitutions ) {
			ClastVisitor<RetTy>::visit( userStmt->substitutions );
		}
		return RetTy();
	}

	virtual RetTy visitClastFor(const clast_for* forStmt) {
		ClastVisitor<RetTy>::visit( forStmt->LB );
		ClastVisitor<RetTy>::visit( forStmt->UB );
		ClastVisitor<RetTy>::visit( forStmt->body );
		return RetTy();
	}

	virtual RetTy visitClastGuard(const clast_guard* guardStmt) {
		ClastVisitor<RetTy>::visit( guardStmt->then );
		for(size_t i=0, e=guardStmt->n; i!=e; ++i) {
			visitClastEquation(&guardStmt->eq[i]);
		}
		return RetTy();
	}

	virtual RetTy visitClastEquation(const clast_equation* equation) {
		ClastVisitor<RetTy>::visit(equation->LHS);
		ClastVisitor<RetTy>::visit(equation->RHS);
		return RetTy();
	}

	virtual RetTy visitCloogStmt(const CloogStatement* cloogStmt) {
		if (cloogStmt->next)
			ClastVisitor<RetTy>::visit(cloogStmt->next);
		return RetTy();
	}

	// Expressions 
	virtual RetTy visitClastName(const clast_name* nameExpr) {
		return RetTy();
	}

	virtual RetTy visitClastTerm(const clast_term* termExpr) {
		if (termExpr->var)
			return ClastVisitor<RetTy>::visit(termExpr->var);

		return RetTy();
	}

	virtual RetTy visitClastBinary(const clast_binary* binExpr) {
		ClastVisitor<RetTy>::visit(binExpr->LHS);
		return RetTy();
	}

	virtual RetTy visitClastReduction(const clast_reduction* redExpr) {
		for(size_t i=0, e=redExpr->n; i!=e; ++i) {
			ClastVisitor<RetTy>::visit( redExpr->elts[i] );
		}
		return RetTy();
	}

	virtual RetTy visit(const clast_expr* expr) {
		return ClastVisitor<RetTy>::visit(expr);
	}

	virtual RetTy visit(const clast_stmt* clast_node) {
		clast_stmt* ptr = clast_node->next;

		if (!ptr)
			return ClastVisitor<RetTy>::visit(clast_node);
		
		ClastVisitor<RetTy>::visit(clast_node);
		visit(ptr);

		return RetTy();
	}

	virtual RetTy visit(const CloogStatement* cloogStmt) {
		const CloogStatement* ptr = cloogStmt->next;

		if( !ptr )
			return ClastVisitor<RetTy>::visit( cloogStmt );

		ClastVisitor<RetTy>::visit( cloogStmt );
		visit( ptr );

		return RetTy();
	}

};

/**************************************************************************************************
 * ClastDump: dump the cloog ast into output stream, this is useful to understand the structure of 
 * the cloog ast and test the visitor 
 *************************************************************************************************/
class ClastDump: public RecClastVisitor<void> {

	std::ostream& out;

	// Utility object used to manage indentation of the pretty printer for the cloog ast (clast)
	class Indent : public utils::Printable {
		size_t spaces;
		const char indent_symbol;

	public:
		Indent(size_t spaces = 0, const char indent_symbol = '\t') : 
			spaces(spaces), indent_symbol(indent_symbol) { }

		Indent& operator++() { ++spaces; return *this; }
		Indent& operator--() { --spaces; return *this; }

		std::ostream& printTo(std::ostream& out) const {
			return out << std::string(spaces, indent_symbol);
		}
	};

	Indent indent;

public:
	ClastDump(std::ostream& out) : out(out) { }

	void visitClastAssignment(const clast_assignment* assignmentStmt) {
		if (assignmentStmt->LHS) { out << assignmentStmt->LHS << "="; }
		visit( assignmentStmt->RHS );
		if (assignmentStmt->stmt.next )
			out << ", ";
	}
	
	void visitClastName(const clast_name* nameExpr) {
		out << nameExpr->name;
	}

	void visitClastTerm(const clast_term* termExpr) {
		PRINT_CLOOG_INT(out, termExpr->val);

		if (termExpr->var == NULL) 	{ return; }	
		out << "*";
		visit(termExpr->var);
	}

	void visitClastFor(const clast_for* forStmt) {
		out << indent << "for(" << forStmt->iterator << "=";
		visit(forStmt->LB);
		out << "; " << forStmt->iterator << "<=";
		visit(forStmt->UB);
		out << "; " << forStmt->iterator << "+=";
		PRINT_CLOOG_INT(out, forStmt->stride);
		out << ") {" << std::endl;
		++indent;
		visit(forStmt->body); 
		--indent;
		out << indent << "}" << std::endl;
	}

	void visitClastUser(const clast_user_stmt* userStmt) { 
		out << indent;
		visit( userStmt->statement );
		out << "(";
		visit( userStmt->substitutions );
		out << ")" <<std::endl; 
	}

	void visitClastGuard(const clast_guard* guardStmt) {
		out << indent << "if (";
		assert(guardStmt->n>0);
		visitClastEquation(guardStmt->eq);
		for(size_t i=1, e=guardStmt->n; i!=e; ++i) {
			out << " && ";
			visitClastEquation(&guardStmt->eq[i]);
		}
		out << ") {" << std::endl;
		++indent;
		visit( guardStmt->then );
		--indent;
		out << indent << "}" << std::endl;
	}

	void visitClastBlock(const clast_block* blockStmt) {
		out << indent << "{" << std::endl;
		++indent;
		visit( blockStmt->body );
		--indent;
		out << "}" << std::endl;
	}

	void visitClastBinary(const clast_binary* binExpr) {
		bool isFunc = false;
		switch (binExpr->type) {
		case clast_bin_fdiv:
			out << "floord(";
			break;
		case clast_bin_cdiv:
			out << "ceild(";
			break;
		default:
			isFunc = true;
		}
		visit(binExpr->LHS);
		if (isFunc) { out << ","; }
		else {
			switch(binExpr->type) {
			case clast_bin_div: out << '/'; break;
			case clast_bin_mod:	out << '%'; break;
			default: 
				assert(false);
			}
		}
		PRINT_CLOOG_INT(out, binExpr->RHS);
		if (isFunc) { out << ")"; }
	}

	void visitClastReduction(const clast_reduction* redExpr) {
		if (redExpr->n == 1)
			return visit( redExpr->elts[0] );
		
		switch(redExpr->type) {
		case clast_red_sum: out << "sum";
							break;
		case clast_red_min: out << "min";
							break;
		case clast_red_max: out << "max";
							break;
		default:
			assert(false && "Reduction operation not valid");
		}

		out << "(";
		assert(redExpr->n >= 1);

		visit( redExpr->elts[0] );
		for(size_t i=1, e=redExpr->n; i!=e; ++i) {
			out << ", ";
			visit( redExpr->elts[i] );
		}
		out << ")";
	}

	void visitClastEquation(const clast_equation* equation) {
		visit(equation->LHS);
		switch( equation->sign ) {
			case -1: out << "<=";
					 break;
			case 0:  out << "==";
					 break;
			case 1:  out << ">=";
					 break;
		}
		visit(equation->RHS);
	}

	void visitCloogStmt(const CloogStatement* cloogStmt) {
		out << cloogStmt->name;
	}
};

#define STACK_SIZE_GUARD \
	auto checkPostCond = [&](size_t stackInitialSize) -> void { 	 \
		assert(stmtStack.size() == stackInitialSize);				 \
	};																 \
	FinalActions __check_stack_size( std::bind(checkPostCond, stmtStack.size()) );

/**************************************************************************************************
 * ClastToIr: converts a clast into an IR which will be used to replace the SCoP region
 *************************************************************************************************/
class ClastToIR : public RecClastVisitor< core::ExpressionPtr > {

public:
	typedef std::vector<core::StatementPtr> StatementList;
	typedef std::stack<StatementList> StatementStack;

	typedef std::map<std::string, core::ExpressionPtr> IRVariableMap;

	ClastToIR(core::NodeManager& mgr, const StmtMap& stmtMap, const IterationVector& iterVec) : 
		mgr(mgr), stmtMap(stmtMap), iterVec(iterVec)
	{
		// Builds a map which associates variables in the cloog AST to IR node. This kind of
		// handling has to be done to be able to remap parameters to correct IR nodes 
		std::for_each(iterVec.begin(), iterVec.end(), 
			[&] (const Element& cur) { 
				if ( cur.getType() == Element::ITER || cur.getType() == Element::PARAM ) {
					std::ostringstream ss;
					ss << cur;
					varMap.insert( std::make_pair(ss.str(), static_cast<const Expr&>(cur).getExpr()) );	
				}
			}
		);

		assert ( varMap.size() == iterVec.size()-1 );

		stmtStack.push( StatementList() );
	}

	core::ExpressionPtr visitClastTerm(const clast_term* termExpr) {
		STACK_SIZE_GUARD;

		core::ASTBuilder builder(mgr);
		
		std::ostringstream ss;
		PRINT_CLOOG_INT(ss, termExpr->val);
	
		core::LiteralPtr&& lit = builder.literal( mgr.basic.getInt4(), ss.str() );
		if (termExpr->var == NULL) 	{ return lit; }
		
		core::ExpressionPtr&& var = visit(termExpr->var);
		// If the coefficient is 1 then omit it 
		if (*lit == *builder.intLit(1) ) { return var; }

		return builder.callExpr( mgr.basic.getSignedIntMul(), lit, var ); 
	}

	core::ExpressionPtr visitClastName(const clast_name* nameExpr) {
		STACK_SIZE_GUARD;

		core::ASTBuilder builder(mgr);

		auto&& fit = varMap.find(nameExpr->name);
		assert(fit != varMap.end() && "Variable not defined!");
		
		core::ExpressionPtr ret = fit->second;
		if(fit->second->getType()->getNodeType() == core::NT_RefType) {
			ret = builder.deref(ret);
		}
		return ret;
	}

	core::ExpressionPtr visitClastReduction(const clast_reduction* redExpr) {
		STACK_SIZE_GUARD;

		core::ASTBuilder builder(mgr);
		if (redExpr->n == 1) { return visit( redExpr->elts[0] ); }

		std::vector<core::ExpressionPtr> args;
		for (size_t i=0, e=redExpr->n; i!=e; ++i) {
			args.push_back( visit( redExpr->elts[i] ) );
			assert( args.back() );
		}

		core::LiteralPtr op;
		core::TypePtr&& intGen = mgr.basic.getIntGen();
		switch(redExpr->type) {
		case clast_red_sum: op = mgr.basic.getSignedIntAdd();
							break;

		case clast_red_min: return buildMin(mgr, args);

		case clast_red_max: op = builder.literal("max", builder.functionType( 
										core::TypeList( { intGen, intGen } ), intGen )
									);
							break;
		default:
			assert(false && "Reduction operation not valid");
		}

		assert(redExpr->n >= 1);
		
		return builder.callExpr( op, args );
	}

	core::ExpressionPtr visitClastFor(const clast_for* forStmt) {
		STACK_SIZE_GUARD;

		core::ASTBuilder builder(mgr);

		auto&& fit = varMap.find(forStmt->iterator);
		assert(fit == varMap.end() && "Induction variable being utilizied!");
		core::VariablePtr&& inductionVar = builder.variable( mgr.basic.getInt4() );

		auto&& indPtr = varMap.insert( std::make_pair(std::string(forStmt->iterator), inductionVar) );
		
		VLOG(2) << "Induction variable for loop: " << *inductionVar;

		core::ExpressionPtr&& lowerBound = visit(forStmt->LB);
		assert( lowerBound && "Failed conversion of lower bound expression for loop!");

		core::ExpressionPtr&& upperBound = visit(forStmt->UB);
		// because cloog assumes the upperbound to be <=, we have to add a 1 to make it consistent
		// with the semantics of the IR 
		upperBound = builder.callExpr( mgr.basic.getSignedIntAdd(), upperBound, builder.intLit(1) );
		assert( upperBound && "Failed conversion of upper bound expression for loop!");

		std::ostringstream ss;
		PRINT_CLOOG_INT(ss, forStmt->stride);
		core::LiteralPtr&& strideExpr = builder.literal( mgr.basic.getInt4(), ss.str() );
		
		stmtStack.push( StatementList() );

		visit(forStmt->body); 
	
		core::ForStmtPtr&& irForStmt = 
			builder.forStmt( 
					builder.declarationStmt(inductionVar, lowerBound), 
					builder.compoundStmt( stmtStack.top() ), 
					upperBound, 
					strideExpr 
				);

		stmtStack.pop();

		// remove the induction variable from the map, this is requred because Cloog uses the same
		// loop iterator for different loops, in the IR this is not allowed, therefore we have to
		// renew the mapping of this induction variable 
		varMap.erase(indPtr.first);

		stmtStack.top().push_back( irForStmt );

		return core::ExpressionPtr();
	}

	core::ExpressionPtr visitClastUser(const clast_user_stmt* userStmt) { 
		STACK_SIZE_GUARD;

		stmtStack.push( StatementList() );

		visit( userStmt->statement );
		assert(stmtStack.top().size() == 1 && "Expected 1 statement!");
	
		utils::map::PointerMap<core::NodePtr, core::NodePtr> replacements;

		size_t pos=0;
		for(const clast_stmt* ptr = userStmt->substitutions; ptr; ptr=ptr->next,pos++) {
			assert(CLAST_STMT_IS_A(ptr, stmt_ass) && "Expected assignment statement");
			const clast_assignment* assignment = reinterpret_cast<const clast_assignment*>( ptr );
			assert(assignment->LHS == NULL);
			const core::ExpressionPtr& targetVar = visit(assignment->RHS);
			const core::VariablePtr& sourceVar = 
				core::static_pointer_cast<const core::Variable>(
						static_cast<const Expr&>(iterVec[pos]).getExpr()
					);

			assert(sourceVar->getType()->getNodeType() != core::NT_RefType);
			replacements.insert( std::make_pair(sourceVar, targetVar) );
		}
		
		core::StatementPtr stmt = stmtStack.top().front();
		
		stmt = core::static_pointer_cast<const core::Statement>( 
					core::transform::replaceAll(mgr, stmt, replacements) 
				);	

		stmtStack.pop();
		stmtStack.top().push_back(stmt);

		return core::ExpressionPtr();
	}

	core::ExpressionPtr visitCloogStmt(const CloogStatement* cloogStmt) {
		STACK_SIZE_GUARD;

		auto&& fit = stmtMap.find( cloogStmt->name );
		assert(fit != stmtMap.end() && "Cloog generated statement not found!");

		stmtStack.top().push_back( fit->second ); //FIXME: index replacement
		
		return core::ExpressionPtr();
	}

	core::ExpressionPtr visitClastEquation(const clast_equation* equation) {
		STACK_SIZE_GUARD;

		core::ASTBuilder builder(mgr);

		core::LiteralPtr op;
		switch( equation->sign ) {
			case -1: op = mgr.basic.getSignedIntLe();
					 break;
			case 0:  op = mgr.basic.getSignedIntEq();
					 break;
			case 1: op = mgr.basic.getSignedIntGe();
					 break;
		}

		return builder.callExpr( op, visit(equation->LHS), visit(equation->RHS) );
	}

	core::ExpressionPtr visitClastBinary(const clast_binary* binExpr) {
		STACK_SIZE_GUARD;
		core::ASTBuilder builder(mgr);

		core::ExpressionPtr op;
		switch (binExpr->type) {
		case clast_bin_fdiv:
			op = builder.literal( 
				builder.functionType(mgr.basic.getReal8(), mgr.basic.getReal8()), "floor" );
			break;
		case clast_bin_cdiv:
			op = builder.literal( 
				builder.functionType(mgr.basic.getReal8(), mgr.basic.getReal8()), "floor" );
			break;
		case clast_bin_div: 
			op = mgr.basic.getSignedIntAdd();
			break;
		case clast_bin_mod:
			op = mgr.basic.getSignedIntMod();
			break;
		default: 
			assert(false && "Binary operator not defined");
		}

		core::ExpressionPtr&& lhs = visit(binExpr->LHS);
		std::ostringstream ss;
		PRINT_CLOOG_INT(ss, binExpr->RHS);
		core::LiteralPtr&& rhs = builder.literal( mgr.basic.getInt4(), ss.str() );

		return builder.callExpr(op, lhs, rhs);
	}

	core::ExpressionPtr visitClastGuard(const clast_guard* guardStmt) {
		STACK_SIZE_GUARD;

		assert(guardStmt->n>0);
		core::ASTBuilder builder(mgr);

		core::ExpressionPtr&& condExpr = visitClastEquation(guardStmt->eq);
		for(size_t i=1, e=guardStmt->n; i!=e; ++i) {
			
			condExpr = builder.callExpr( mgr.basic.getBoolLAnd(), condExpr, 
					builder.createCallExprFromBody( 
						builder.returnStmt(visitClastEquation(&guardStmt->eq[i])), 
						mgr.basic.getBool(), 
						true 
					) 
				);

		}
		
		stmtStack.push( StatementList() );
		visit( guardStmt->then );

		core::StatementPtr&& irIfStmt = 
				builder.ifStmt( condExpr, builder.compoundStmt(stmtStack.top()) );
		stmtStack.pop();

		stmtStack.top().push_back( irIfStmt );

		return core::ExpressionPtr();
	}

	core::StatementPtr getIR() const {
		assert( stmtStack.size() == 1 );
		core::ASTBuilder builder(mgr);

		return builder.compoundStmt( stmtStack.top() );
	}

private:
	core::NodeManager& 		mgr;
	const StmtMap&			stmtMap;
	const IterationVector& 	iterVec;
	IRVariableMap  			varMap;
	StatementStack 			stmtStack;
};

} // end anonymous namespace

namespace insieme {
namespace analysis {
namespace poly {

template <>
core::NodePtr toIR(core::NodeManager& mgr, 
		const StmtMap& stmtMap,		
		const IterationVector& iterVec, 
		IslContext& ctx, 
		const Set<IslContext>& domain, 
		const Map<IslContext>& schedule) 
{

	CloogState *state;
	CloogInput *input;
	CloogOptions *options;

	struct clast_stmt *root;
	state = cloog_state_malloc();
	options = cloog_options_malloc(state);

	MapPtr<IslContext>&& schedDom = map_intersect_domain(ctx, schedule, domain);

	CloogUnionDomain* unionDomain = 
		cloog_union_domain_from_isl_union_map( isl_union_map_copy( schedDom->getAsIslMap() ) );

	isl_dim* dim = isl_union_map_get_dim( schedDom->getAsIslMap() );

	CloogDomain* context = cloog_domain_from_isl_set( isl_set_universe(dim) );

	input = cloog_input_alloc(context, unionDomain);

	options->block = 1;
	root = cloog_clast_create_from_input(input, options);
	assert( root && "Generation of Cloog AST failed" );
	// clast_pprint(stdout, root, 0, options);
	
	if (VLOG_IS_ON(1) ) {
		ClastDump dumper( LOG_STREAM(DEBUG) );
		dumper.visit(root);
	}

	ClastToIR converter(mgr, stmtMap, iterVec);
	converter.visit(root);
	
	core::StatementPtr&& retIR = converter.getIR();
	assert(retIR && "Conversion of Cloog AST to Insieme IR failed");

	VLOG(1) << core::printer::PrettyPrinter(retIR, core::printer::PrettyPrinter::OPTIONS_DETAIL);
	
	cloog_clast_free(root);
	cloog_options_free(options);
	cloog_state_free(state);

	return retIR;
}

} // end poly namespace 
} // end analysis namespace 
} // end insieme namespace 
