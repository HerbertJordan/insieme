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

#include "insieme/analysis/polyhedral/scop.h"

#include "insieme/core/ast_visitor.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/lang/basic.h"

#include "insieme/core/ast_builder.h"

namespace {
using namespace insieme::core;
using namespace insieme::core::lang;
using namespace insieme::analysis;
using namespace insieme::analysis::poly;
using namespace insieme::analysis::scop;

/**
 * Expection which is thrown when a particular tree is defined to be not a static control part. This
 * exception has to be forwarded until the root containing this node which has to be defined as a
 * non ScopRegion
 * 
 * Because this exception is only used within the implementation of the ScopRegion visitor, it is
 * defined in the anonymous namespace and therefore not visible outside this translation unit.
 */
class NotASCoP : public std::exception {
	const NodePtr& root;
public:
	NotASCoP( const NodePtr& root ) : std::exception(), root(root) { }

	virtual const char* what() const throw() { 
		std::ostringstream ss;
		ss << "Node '" << *root << "' is not a Static Control Part";
		return ss.str().c_str();
	}

	virtual ~NotASCoP() throw() { }
};

ConstraintCombinerPtr extractFromCondition(IterationVector& iv, const ExpressionPtr& cond) {

	NodeManager& mgr = cond->getNodeManager();
    if (cond->getNodeType() == NT_CallExpr) {
    	const CallExprPtr& callExpr = static_pointer_cast<const CallExpr>(cond);
        if ( mgr.basic.isIntCompOp(callExpr->getFunctionExpr()) || 
	  		 mgr.basic.isUIntCompOp(callExpr->getFunctionExpr()) ) 
		{
			assert(callExpr->getArguments().size() == 2 && "Malformed expression");

			// First of all we check whether this condition is a composed by multiple conditions
			// connected through || or && statements 
			BasicGenerator::Operator&& op = 
				mgr.basic.getOperator( static_pointer_cast<const Literal>(callExpr->getFunctionExpr()) ); 

			switch (op) {
			case BasicGenerator::LOr:
			case BasicGenerator::LAnd:
				{
					ConstraintCombinerPtr&& lhs = extractFromCondition(iv, callExpr->getArgument(0));
					ConstraintCombinerPtr&& rhs = extractFromCondition(iv, callExpr->getArgument(1));

					if (op == BasicGenerator::LAnd)		return makeConjunction(lhs, rhs);
					else 								return makeDisjunction(lhs, rhs);
				}
			case BasicGenerator::LNot:
				 return std::make_shared<NegatedConstraintCombiner>( 
						 extractFromCondition(iv, callExpr->getArgument(0))
					);
			default:
				break;
			}
			// A constraints is normalized having a 0 on the right side, therefore we build a
			// temporary expression by subtracting the rhs to the lhs, Example: 
			//
			// if (a<b) { }    ->    if( a-b<0 ) { }
			try {
				ASTBuilder builder(mgr);
				ExpressionPtr&& expr = builder.callExpr( 
						mgr.basic.getSignedIntSub(), callExpr->getArgument(0), callExpr->getArgument(1) 
					);
				AffineFunction af(iv, expr);
				// Determine the type of this constraint
				Constraint::Type type;
				switch (op) {
					case BasicGenerator::Eq: type = Constraint::EQ; break;
					case BasicGenerator::Ne: type = Constraint::NE; break;
					case BasicGenerator::Lt: type = Constraint::LT; break;
					case BasicGenerator::Le: type = Constraint::LE; break;
					case BasicGenerator::Gt: type = Constraint::GT; break;
					case BasicGenerator::Ge: type = Constraint::GE; break;
					default:
						assert(false && "Operation not supported!");
				}
				return std::make_shared<RawConstraintCombiner>( Constraint(af, type) );

			} catch (const NotAffineExpr& e) { throw NotASCoP(cond); }
		}
	}
	assert(false);
}

IterationVector markAccessExpression(const ExpressionPtr& expr) {
	try {
		IterationVector it;
		expr->addAnnotation(
			std::make_shared<AccessFunction>( it, Constraint( AffineFunction(it, expr), Constraint::EQ ) ) 
		);
		return it;
	} catch(const NotAffineExpr& e) { 
		throw NotASCoP(expr); 
	}
}

//===== ScopVisitor ===============================================================

struct ScopVisitor : public ASTVisitor<IterationVector> {
	ScopList& scopList;
	ScopRegion::StmtList subScops;

	ScopVisitor(ScopList& scopList) : ASTVisitor<IterationVector>(false), scopList(scopList) { }

	// Visit the body of a SCoP. This requires to collect the iteration vector for the body and
	// eventual ref access statements existing in the body. For each array access we extract the
	// equality constraint used for accessing each dimension of the array and store it in the
	// AccessFunction annotation.
	RefList visitBody(IterationVector& iterVec, const StatementPtr& body) { 
		iterVec = merge(iterVec, visit(body));
		
		RefList&& refs = collectDefUse( body, StatementSet(subScops.begin(), subScops.end()) );
		
		// For now we only consider array access. 
		//
		// FIXME: In the future also scalars should be properly handled using technique like scalar
		// arrays and so forth
		std::for_each(refs.arrays_begin(), refs.arrays_end(),
			[&](const ArrayRefPtr& cur) { 
				const ExpressionList& idxExprs = cur->getIndexExpressions();
				std::for_each(idxExprs.begin(), idxExprs.end(), 
					[&](const ExpressionPtr& cur) { 
						iterVec = merge(iterVec, markAccessExpression(cur));
				});
			});
		return refs;
	}

	/*
	 * Visit of If Statements: 
	 *   Visits the then and else body checking whether they are SCoPs. In the case both the
	 *   branches are SCoPs the condition is evaluated and a constraint created out of it. Two
	 *   annotations will be then created, one with the positive condition and the second one with
	 *   the negated condition which will be attached respectively to the then and the else body of
	 *   the if statement. In the case one of the two branches is not a SCoP, the SCoP branch is
	 *   inserted in the list of root scops (scopList) and the NotAScop exception thrown to the
	 *   parent node. 
	 */
	IterationVector visitIfStmt(const IfStmtPtr& ifStmt) {
		IterationVector ret, saveThen, saveElse;
		RefList thenRefs, elseRefs;
		ScopRegion::StmtList thenScops, elseScops;
		bool isThenSCOP = true, isElseSCOP = true;

		try {
			subScops.clear();
			// check the then body
			thenRefs = visitBody( ret, ifStmt->getThenBody() );
			// save the sub scops of the then body
			thenScops = subScops;
			saveThen = ret;
		} catch (const NotASCoP& e) { isThenSCOP = false; }

		// reset the value of the iteration vector 
		ret = IterationVector();

		try {
			subScops.clear();
			// check the else body
			elseRefs = visitBody( ret, ifStmt->getElseBody() );
			// save the sub scops of the else body
			elseScops = subScops; 
			saveElse = ret;
		} catch (const NotASCoP& e) { isElseSCOP = false; }
	
		if ( isThenSCOP && !isElseSCOP ) {
			// then is a root of a ScopRegion, we add it to the list of scops
			scopList.push_back( std::make_pair(ifStmt->getThenBody(), saveThen) );
		}

		if ( !isThenSCOP && isElseSCOP ) {
			// else is a root of a ScopRegion, we add it to the list of scops
			scopList.push_back( std::make_pair(ifStmt->getElseBody(), saveElse) );
		}

		subScops.clear();
		// if either one of the branches is not a ScopRegion it means the if statement is not a
		// ScopRegion, therefore we can re-throw the exception and invalidate this region
		if (!(isThenSCOP && isElseSCOP)) { throw NotASCoP(ifStmt); }

		// reset the value of the iteration vector 
		ret = IterationVector();
		// check the condition expression
		ConstraintCombinerPtr&& comb = extractFromCondition(ret, ifStmt->getCondition());

		// At this point we are sure that both the then, else body are SCoPs and the condition of
		// this If statement is also an affine linear function. 
		ret = merge(ret, merge(saveThen, saveElse));

		// if no exception has been thrown we are sure the sub else and then tree are ScopRegions,
		// therefore this node can be marked as SCoP as well.
		ifStmt->getThenBody()->addAnnotation( 
			std::make_shared<ScopRegion>(ret, comb, thenScops, 
				ScopRegion::AccessRefList(thenRefs.arrays_begin(), thenRefs.arrays_end()) 
			)
		);
		// Add the then body to the list of subscops to which the parent will point at
		subScops.push_back( ifStmt->getThenBody() );
		
		// the else body is annotated with the negated domain
		ifStmt->getElseBody()->addAnnotation( 
			std::make_shared<ScopRegion>(ret, negate(comb), elseScops, 
				ScopRegion::AccessRefList(elseRefs.arrays_begin(), elseRefs.arrays_end()) 
			)
		);
		// Add the else body to the list of subscops to which the parent will point at
		subScops.push_back( ifStmt->getElseBody() );
		return ret;
	}

	IterationVector visitForStmt(const ForStmtPtr& forStmt) {
		subScops.clear();

		IterationVector bodyIV;
		RefList&& refs = visitBody( bodyIV, forStmt->getBody() );
		
		IterationVector ret;
		const DeclarationStmtPtr& decl = forStmt->getDeclaration();
		ret.add( Iterator(decl->getVariable()) ); 
		
		NodeManager& mgr = forStmt->getNodeManager();
		ASTBuilder builder(mgr);
		
		ConstraintCombinerPtr cons; 
		try {
			// We assume the IR loop semantics to be the following: 
			// i: lb...ub:s 
			// which spawns a domain: lw <= i < ub exists x in Z : lb + x*s = i
			// Check the lower bound of the loop
			AffineFunction lb(ret, 
					builder.callExpr(mgr.basic.getSignedIntSub(), decl->getVariable(), decl->getInitialization())	
				);

			// check the upper bound of the loop
			AffineFunction ub(ret, 
					builder.callExpr(mgr.basic.getSignedIntSub(), decl->getVariable(), forStmt->getEnd())
				);
			// set the constraint: iter >= lb && iter < ub
			cons = makeConjunction( 
					makeCombiner(Constraint(lb, Constraint::GE)), makeCombiner(Constraint(ub, Constraint::LT)) 
				);
			ret = merge(ret,bodyIV);

			forStmt->addAnnotation( 
				std::make_shared<ScopRegion>(ret, cons, subScops, 
					ScopRegion::AccessRefList(refs.arrays_begin(), refs.arrays_end())
				) 
			); 

			subScops.clear();
			// add this statement as a subscop
			subScops.push_back(forStmt);
		
		} catch (const NotAffineExpr& e) { 
			// one of the expressions are not affine constraints, therefore we set this loop to be a
			// non ScopRegion
			throw NotASCoP(forStmt);
		}
		return ret;
	}

	// While stmts cannot be represented in the polyhedral form (at least in the general case). In
	// the future we may develop a more advanced analysis capable of representing while loops in the
	// polyhedral model 
	IterationVector visitWhileStmt(const WhileStmtPtr& whileStmt) { throw NotASCoP( whileStmt ); }

	IterationVector visitCompoundStmt(const CompoundStmtPtr& compStmt) {
		IterationVector ret;
		bool isSCOP = true;
		
		ScopRegion::StmtList scops;

		for_each(compStmt->getStatements().cbegin(), compStmt->getStatements().cend(), 
			[&](const StatementPtr& cur) { 
				try {
					// clear Sub scops
					subScops.clear();
					ret = merge(ret, this->visit(cur));	
					// copy the sub spawned scops 
					std::copy(subScops.begin(), subScops.end(), std::back_inserter(scops));
				} catch(const NotASCoP& e) { isSCOP = false; }
			} 
		);

		// make the SCoPs available for the parent node 	
		subScops = scops;

		if (!isSCOP) { 
			// one of the statements in this compound statement broke a ScopRegion therefore we add
			// to the scop list the roots for valid ScopRegions inside this compound statement 
			std::for_each(compStmt->getStatements().cbegin(), compStmt->getStatements().cend(), 
				[&](const StatementPtr& cur) { 
					if (cur->hasAnnotation(ScopRegion::KEY)) { 
						scopList.push_back( 
							std::make_pair(cur, cur->getAnnotation(ScopRegion::KEY)->getIterationVector()) 
						); 
					}
				} 
			);

			throw NotASCoP(compStmt); 
		}
		return ret;
	}

	IterationVector visitLambda(const LambdaPtr& lambda) {	
		if ( lambda->hasAnnotation(ScopRegion::KEY) ) {
			// if the SCopRegion annotation is already attached, it means we already visited this
			// function, therefore we can return the iteration vector already precomputed 
			return std::static_pointer_cast<ScopRegion>(lambda->getAnnotation(ScopRegion::KEY))->getIterationVector();
		}
		// otherwise we have to visit the body and attach the ScopRegion annotation 
		IterationVector&& bodyIV = visit(lambda->getBody());
		lambda->addAnnotation( std::make_shared<ScopRegion>(bodyIV, ConstraintCombinerPtr(), subScops) );
		scopList.push_back( std::make_pair(lambda, bodyIV) );

		return bodyIV;
	}

	//IterationVector visitCallExpr(const CallExprPtr& callExpr) {
		//// if we have a call to a lambda we have to take care of setting the constraints which
		//// enforce the fact that call expression arguments are assigned to the formal parameters fo
		//// the function. therefore for a call expression f(a,b) of a function int f(int d, int e) 
		//// the two constraints (d == a) and (e == b) needs to be generated 
		//const ExpressionPtr& func = callExpr->getFunctionExpr();

		//IterationVector&& ret = visit(func);
		//if (func->getNodeType() == NT_LambdaExpr) {
			
		//}
	//}

	IterationVector visitProgram(const ProgramPtr& prog) { 
		for_each(prog->getEntryPoints().cbegin(), prog->getEntryPoints().cend(), 
			[&](const ExpressionPtr& cur) { this->visit(cur); } );
		return IterationVector();
	}

	// Generic method which recursively visit IR nodes and merges the resulting 
	// iteration vectors 
	IterationVector visitNode(const NodePtr& node) {
		const Node::ChildList& cl = node->getChildList();
		IterationVector ret;
		std::for_each( cl.begin(), cl.end(), [&](const NodePtr& cur) { 
			ret = merge(ret, this->visit(cur)); 
		} );
		return ret;
	}
	
};

} // end namespace anonymous 

namespace insieme {
namespace analysis {
namespace scop {

using namespace core;
using namespace poly;

//===== ScopRegion ===============================================================
const string ScopRegion::NAME = "SCoPAnnotation";
const utils::StringKey<ScopRegion> ScopRegion::KEY("SCoPAnnotationKey");

std::ostream& ScopRegion::printTo(std::ostream& out) const {
	out << "IterationVector: " << iterVec;
	if (constraints) {
		out << "\\nConstraints: " << *constraints;
	}
	if (!subScops.empty()) {
		out << "\\nSubScops: " << subScops.size();
	}
	if (!accesses.empty()) {
		out << "\\nAccesses: " << accesses.size() << "{" 
			<< join(",", accesses, [&](std::ostream& jout, const RefPtr& cur){ jout << *cur; } ) 
			<< "}";
	}
	return out;
}

namespace {

// Recursively visit ScopRegion appending constraints related to the current domain. Accesses found
// in the region are appended to the accessList object
void visitScop(const poly::IterationVector& iterVec, const poly::ConstraintCombinerPtr& parentDomain, 
		const ScopRegion& region, ScopRegion::AccessInfoList& accessList) 
{
	// assert( parentDomain->getIterationVector() == iterVec );

	poly::ConstraintCombinerPtr currDomain = poly::cloneConstraint(iterVec, region.getConstraints());
	if (parentDomain) {
		currDomain = poly::makeConjunction(parentDomain, currDomain); 
	}
	
	// for every access in this region, convert the affine constraint to the new iteration vector 
	std::for_each(region.getDirectAccesses().begin(), region.getDirectAccesses().end(), 
		[&](const RefPtr& cur) { 
			if ( cur->getType() == Ref::ARRAY ) {
				std::vector<poly::ConstraintCombinerPtr> idx;
				const ArrayRef& array = static_cast<const ArrayRef&>(*cur);
				std::for_each(array.getIndexExpressions().begin(), array.getIndexExpressions().end(), 
					[&](const ExpressionPtr& cur) { 
						assert(cur->hasAnnotation(scop::AccessFunction::KEY));
						scop::AccessFunction& ann = *cur->getAnnotation(scop::AccessFunction::KEY);
						idx.push_back( poly::makeCombiner( ann.getAccessConstraint().toBase(iterVec) ) );
					}
				);
				accessList.push_back( std::make_tuple(cur, poly::IterationDomain(iterVec, currDomain), idx) ); 
				return;
			}
			// accessList.push_back( std::make_tuple(cur, poly::IterationDomain(iterVec, currDomain), std::vector<poly::AffineFunction>()) ); 
		} ); 
	
	// for every nested scop we visit it
	std::for_each(region.getSubScops().begin(), region.getSubScops().end(), 
		[&] (const core::StatementPtr& cur) { 
			assert(cur->hasAnnotation(ScopRegion::KEY));
			visitScop(iterVec, currDomain, *cur->getAnnotation(ScopRegion::KEY), accessList); 
		}
	);
}

} // end anonymous namespace 


const ScopRegion::AccessInfoList ScopRegion::listAccesses() const {
	ScopRegion::AccessInfoList ret;
	
	visitScop(iterVec, constraints, *this, ret);
	return ret;
}

//===== AccessFunction ============================================================
const string AccessFunction::NAME = "AccessFuncAnn";
const utils::StringKey<AccessFunction> AccessFunction::KEY("AccessFuncAnnKey");

std::ostream& AccessFunction::printTo(std::ostream& out) const {
	return out << "IV: " << iterVec << ", CONS: " << eqCons;
}

//===== mark ======================================================================
ScopList mark(const core::NodePtr& root) {
	ScopList ret;
	ScopVisitor sv(ret);
	try {
		sv.visit(root);
	} catch (const NotASCoP& e) { }

	return ret;     
}


//===== printSCoP ===================================================================
void printSCoP(std::ostream& out, const core::NodePtr& scop) {
	out << "SCoP: ";
	// check whether the IR node has a SCoP annotation
	if( !scop->hasAnnotation( ScopRegion::KEY ) ) {
		out << "{ }\n";
		return ;
	}
	
	const ScopRegion& ann = *scop->getAnnotation( ScopRegion::KEY );
	const ScopRegion::AccessInfoList& acc = ann.listAccesses();

	std::for_each(acc.begin(), acc.end(), [&](const ScopRegion::AccessInfo& cur){
		const Ref& ref = *std::get<0>(cur);
		out << "\n\tACCESS: [" << ref.getUsage() << "] " << *ref.getBaseExpression(); 
		out << "\n\t" << std::get<1>(cur);
		out << "\n\tIDX: " << join(";", std::get<2>(cur), 
			[&](std::ostream& jout, const poly::ConstraintCombinerPtr& cur){ jout << *cur; } );
	 	out << std::endl;
	});
}



} // end namespace scop
} // end namespace analysis
} // end namespace insieme
