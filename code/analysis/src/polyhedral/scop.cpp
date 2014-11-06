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

#include <iomanip>
#include <isl/schedule.h>
#include <memory>
#include <set>

#include "insieme/analysis/dep_graph.h"
#include "insieme/analysis/polyhedral/backend.h"
#include "insieme/analysis/polyhedral/backends/isl_backend.h"
#include "insieme/analysis/polyhedral/scop.h"
#include "insieme/analysis/polyhedral/scopregion.h"
#include "insieme/core/annotations/source_location.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/utils/logging.h"

namespace insieme { namespace analysis { namespace polyhedral {

using namespace insieme::core;

//==== IterationDomain ==============================================================================

IterationDomain operator&&(const IterationDomain& lhs, const IterationDomain& rhs) {
	assert(lhs.getIterationVector() == rhs.getIterationVector());
	if(lhs.universe()) return rhs;
	if(rhs.universe()) return lhs;

	return IterationDomain( lhs.getConstraint() and rhs.getConstraint() ); 
}

IterationDomain operator||(const IterationDomain& lhs, const IterationDomain& rhs) {
	assert(lhs.getIterationVector() == rhs.getIterationVector());
	if(lhs.universe()) return rhs;
	if(rhs.universe()) return lhs;

	return IterationDomain( lhs.getConstraint() or rhs.getConstraint() ); 
}

IterationDomain operator!(const IterationDomain& other) {
	return IterationDomain( not_( other.getConstraint() ) ); 
}

std::ostream& IterationDomain::printTo(std::ostream& out) const { 
	if (empty()) return out << "{}";
	if (universe()) return out << "{ universe }";
	return out << *constraint; 
}

utils::Piecewise<insieme::core::arithmetic::Formula> 
cardinality(core::NodeManager& mgr, const IterationDomain& dom) {
	auto&& ctx = makeCtx();

	SetPtr<> set = makeSet(ctx, dom);
	return set->getCard()->toPiecewise(mgr);
}

IterationDomain makeVarRange(IterationVector& 				iterVec, 
							 const core::ExpressionPtr& 	var, 
							 const core::ExpressionPtr& 	lb, 
							 const core::ExpressionPtr& 	ub) 
{
	core::ExpressionPtr expr = var;
	// check whether the lb and ub are affine expressions 
	if(expr->getType()->getNodeType() == core::NT_RefType) {
		expr = core::IRBuilder(var->getNodeManager()).deref(var);
	}
	assert(lb && "Lower bound not specified, argument required");
	core::arithmetic::Formula lbf = core::arithmetic::toFormula(lb);
	core::arithmetic::Formula ubf = ub ? core::arithmetic::toFormula(ub) : core::arithmetic::Formula();

	if (!ub || lbf == ubf) {
		AffineFunction af(iterVec, 0-lbf+core::arithmetic::Value(expr));
		return IterationDomain( AffineConstraint(af, utils::ConstraintType::EQ) );
	}
	// else this is a range 
	AffineFunction lbaf(iterVec, 0-lbf+core::arithmetic::Value(expr));
	AffineFunction ubaf(iterVec, 0-ubf+core::arithmetic::Value(expr));
	return IterationDomain( 
		AffineConstraint(lbaf, utils::ConstraintType::GE) and 
		AffineConstraint(ubaf, utils::ConstraintType::LT) 
	);
}

namespace {

template <class Cont>
void extract(const std::vector<AffineConstraintPtr>& conjunctions, const Element& elem, std::set<const Element*>& checked, Cont& cont) {
	
	std::set<const Element*,compare_target<const Element*>> elements;

	if (!checked.insert(&elem).second) { return; }

	// for each conjunction collect the constraints where the interested variables are appearing 
	// do it recursively 
	for ( const auto& cons : conjunctions ) {
		
		const AffineFunction& func = std::static_pointer_cast<RawAffineConstraint>(cons)->getConstraint().getFunction();

		if (func.getCoeff(elem) != 0) { 
			// add this constraint to the result 
			cont.insert(cons);
			
			for_each(func.begin(), func.end(), [&](AffineFunction::Term term) {
					if (term.second != 0 && term.first != elem && term.first.getType() != Element::CONST) {
						elements.insert( &term.first );
					}
				});
		}
	}
	for_each(elements.begin(), elements.end(), [&](const Element* cur) { extract(conjunctions, *cur, checked, cont); });
}

} // end anonymous namespace 

// TODO: this function needs to be simplified, use common infrastructure functions to build up scop from address
std::pair<core::NodeAddress, AffineConstraintPtr> getVariableDomain(const core::ExpressionAddress& addr) {
	
	// Find the enclosing SCoP (if any)
	core::NodeAddress prev = addr;
	core::NodeAddress parent;
	// get the immediate SCoP
	while(!prev.isRoot() && (parent = prev.getParentAddress(1)) && !parent->hasAnnotation( scop::ScopRegion::KEY) ) { prev=parent; } 

	// This statement is not part of a SCoP (also may throw an exception)
	if ( !parent->hasAnnotation( scop::ScopRegion::KEY ) || !parent->getAnnotation( scop::ScopRegion::KEY )->valid) {
		return std::make_pair(NodeAddress(), AffineConstraintPtr()); 
	}

	StatementAddress enclosingScop = parent.as<StatementAddress>();

	prev = parent;
	// Iterate throgh the stateemnts until we find the entry point of the SCoP
	while(!prev.isRoot() && (parent = prev.getParentAddress(1)) && parent->hasAnnotation( scop::ScopRegion::KEY) &&
			parent->getAnnotation( scop::ScopRegion::KEY )->valid)
		prev=parent;

	assert(parent && "Scop entry not found");

	// Resolve the SCoP from the entry point
	Scop& scop = prev->getAnnotation( scop::ScopRegion::KEY )->getScop();

	// navigate throgh the statements of the SCoP until we find the one 
	auto fit = std::find_if(scop.begin(), scop.end(), [&](const StmtPtr& cur) { 
			return isChildOf(cur->getAddr(),addr);
		}); 


	auto extract_surrounding_domain = [&]() {

		if (fit != scop.end()) {
			// found stmt containing the requested expression 
			return (*fit)->getDomain();
		}

		IterationDomain domain( scop.getIterationVector(), 
								enclosingScop->getAnnotation( scop::ScopRegion::KEY )->getDomainConstraints());
		// otherwise the expression is part of a condition expression 
		prev = enclosingScop;
		// Iterate throgh the stateemnts until we find the entry point of the SCoP
		while(!prev.isRoot() && (parent = prev.getParentAddress(1)) && parent->hasAnnotation( scop::ScopRegion::KEY) ) { 
			prev=parent;
			domain &= IterationDomain( scop.getIterationVector(), 
									   prev->getAnnotation( scop::ScopRegion::KEY )->getDomainConstraints() );
		} 
		return domain;
	};

	IterationDomain domain( extract_surrounding_domain() );

	//LOG(INFO) << domain;

	if (domain.universe() || domain.empty()) { 
		return std::make_pair(NodeAddress(), domain.getConstraint()); 
	}

	try {

		AffineFunction func(scop.getIterationVector(), addr.getAddressedNode());

		// otherwise this is a composed by a conjunction of constraints. we need to analyze the
		// contraints in order to determine the minimal constraint which defines the given expression 
		AffineConstraintPtr constraints = domain.getConstraint(); 

		DisjunctionList resultConj;
		for( auto conj : utils::getConjunctions(utils::toDNF(constraints))  ) {
			
			auto cmp  = [](const AffineConstraintPtr& lhs, const AffineConstraintPtr& rhs) -> bool { 
				assert( lhs && rhs );
				return *std::static_pointer_cast<RawAffineConstraint>(lhs) < 
					   *std::static_pointer_cast<RawAffineConstraint>(rhs); 
			};
			std::set<AffineConstraintPtr, decltype(cmp)> curConj(cmp);
			std::set<const Element*> checked;

			for_each(func.begin(), func.end(), [&](const AffineFunction::Term& term) {
				if (term.second != 0 && term.first.getType() != Element::CONST) {
					extract(conj, term.first, checked, curConj);
				}
			});
			
			if (!curConj.empty()) resultConj.push_back( ConjunctionList(curConj.begin(), curConj.end())); 
		}

		auto cons = utils::fromConjunctions(resultConj);

		if (!cons) { return std::make_pair(enclosingScop, AffineConstraintPtr()); }

		// LOG(INFO) << "GEN " << *cons;

		/* Simplify the constraint by using the polyhedral model library */
		//auto&& ctx = makeCtx();
		//auto&& set = makeSet(ctx, IterationDomain(cons));
		//set->simplify();

		//IterationVector iterVec;
		//cons = set->toConstraint(addr->getNodeManager(), iterVec);
		// If we end-up with an empty constraints it means that the result is the universe 
		//if (!cons) { 
		//	return std::make_pair(prev, AffineConstraintPtr()); 
		//}

		// Need to represent the constraint based on the iteration vector stored in the Scop object
		// which will survive the lifetime of this method
		//IterationDomain curr_dom( scop.getIterationVector(), IterationDomain(cons) );

		return make_pair(prev, cons);

	} catch (const NotAffineExpr& e) {
		// The expression is not an affine function, therefore we give up
		return std::make_pair(prev, AffineConstraintPtr());
	}
}	


//==== AffineSystem ==============================================================================

std::ostream& AffineSystem::printTo(std::ostream& out) const {
	for (std::vector<AffineFunctionPtr>::const_iterator cur=funcs.begin(); cur!=funcs.end(); ++cur) {
		out << "\t\t" << (*cur)->toStr(AffineFunction::PRINT_ZEROS);
		if (cur+1==funcs.end()) out << " #";
		out << std::endl;
	}
	return out;
}

void AffineSystem::insert(const iterator& pos, const AffineFunction& af) { 
	// adding a row to this matrix 
	funcs.insert( pos.get(), AffineFunctionPtr(new AffineFunction(af.toBase(iterVec))) );
}

utils::Matrix<int> extractFrom(const AffineSystem& sys) {

	utils::Matrix<int> mat(sys.size(), sys.getIterationVector().size());

	size_t i=0;
	for_each (sys.begin(), sys.end(), [&](const AffineFunction& cur) {
			size_t j=0;
			std::for_each(cur.begin(), cur.end(), [&] (const AffineFunction::Term& term) {
				mat[i][j++] = term.second;
			});
			i++;
		} );

	return mat;
}

std::vector<core::VariablePtr> getOrderedIteratorsFor(const AffineSystem& sched) {
	// For each dimension we need to check whether this entry contains an iterator
	const IterationVector& iterVec = sched.getIterationVector();
	utils::Matrix<int> schedule = extractFrom(sched);

	std::vector<core::VariablePtr> iters;
	for(size_t r = 0, rend = schedule.rows(); r<rend; ++r) {
		for (size_t c = 0, cend = iterVec.getIteratorNum(); c != cend; ++c) {
			if (schedule[r][c] == 1) { 
				iters.push_back( static_cast<const Iterator&>(iterVec[c]).getExpr().as<core::VariablePtr>() ); 
				break;
			}
		}
	}
	return iters;	
}

//==== Stmt ==================================================================================

std::vector<core::VariablePtr> Stmt::loopNest() const {
	
	std::vector<core::VariablePtr> nest;
	for_each(getSchedule(),	[&](const AffineFunction& cur) { 
		const IterationVector& iv = cur.getIterationVector();
		for(IterationVector::iter_iterator it = iv.iter_begin(), end = iv.iter_end(); it != end; ++it) {
			if( cur.getCoeff(*it) != 0) { 
				nest.push_back( core::static_pointer_cast<const core::Variable>( it->getExpr()) );
				break;
			}
		}
	} );

	return nest;
}

/**
 * @brief Stmt::printTo prints the statement in human-readable format to the given stream
 * @param out: the stream where to write the information to
 * @return returns the ostream out stream (same as input)
 */
std::ostream& Stmt::printTo(std::ostream& out) const {
	// TODO: introduce class SCoPMetric
	out << "stmt S_" << id << ": " << std::endl;
	core::annotations::LocationOpt loc=core::annotations::getLocation(addr);
	if (loc) out << "\tlocation   \t" << *loc << std::endl;
	out << "\tnode       \t" << printer::PrettyPrinter( addr.getAddressedNode() ) << std::endl;
	out << "\titer domain\t" << dom << std::endl;
	out << "\tschedule   \t" << std::endl << schedule;

	// Prints the list of accesses for this statement 
	out << "\taccess     \t" << std::endl;
	for_each(access_begin(), access_end(), [&](const AccessInfoPtr& cur){ out << *cur; });

	auto&& ctx = makeCtx();
	out << "\tcardinality\t" << *makeSet(ctx, dom)->getCard() << std::endl;

	return out;
}

boost::optional<const Stmt&> getPolyheadralStmt(const core::StatementAddress& stmt) {

	NodePtr root = stmt.getRootNode();
	scop::AddressList&& addrs = scop::mark( root );

	// we have to fing whether the top level of this scop contains stmt
	auto fit = find_if(addrs.begin(), addrs.end(), [&](const NodeAddress& cur) { 
			if ( core::isChildOf(cur, core::static_address_cast<const Node>(stmt)) ) { 
				return true; 
			}
			return false;
		});
	
	if (fit == addrs.end()) {
		// the address is not inside any of the top level scops for this region
		return boost::optional<const Stmt&>();
	}
	
	assert( fit->getAddressedNode()->hasAnnotation(scop::ScopRegion::KEY) );
	scop::ScopRegion& reg = *fit->getAddressedNode()->getAnnotation(scop::ScopRegion::KEY);

	// find the statement inside this region 
	Scop& scop = reg.getScop();

	auto fit2 = find_if(scop.begin(), scop.end(), 
			[&](const StmtPtr& cur) { return cur->getAddr() == stmt; });

	assert(fit2 != scop.end());

	return boost::optional<const Stmt&>(**fit2);
}

unsigned Stmt::getSubRangeNum() const {
	bool ranges = 0;
	for_each(access_begin(), access_end(), [&] (const AccessInfoPtr& cur) { 
			if (!cur->getDomain().universe()) { ++ranges; }
		});
	return ranges;
}

//==== AccessInfo ==============================================================================

std::ostream& AccessInfo::printTo(std::ostream& out) const {
	out << " -> REF ACCESS: [" << Ref::useTypeToStr( getUsage() ) << "] "
		<< " -> VAR: " << printer::PrettyPrinter( getExpr().getAddressedNode() ) ; 

	const AffineSystem& accessInfo = getAccess();
	out << " INDEX: " << join("", accessInfo.begin(), accessInfo.end(), 
			[&](std::ostream& jout, const AffineFunction& cur){ jout << "[" << cur << "]"; } );
	
	if (hasDomainInfo()) 
		out << " RANGE: " << getDomain();
	
	out << std::endl;

	if (!accessInfo.empty()) {	
		out << accessInfo;
		//auto&& access = makeMap<POLY_BACKEND>(ctx, *accessInfo, tn, 
			//TupleName(cur.getExpr(), cur.getExpr()->toString())
		//);
		//// map.intersect(ids);
		//out << " => ISL: "; 
		//access->printTo(out);
		//out << std::endl;
	}
	return out;
}

//==== Scop ====================================================================================

/// Returns true if the given NodePtr has a SCoP annotation, false otherwise. Can be called directly,
/// with explicit scoping.
bool Scop::hasScopAnnotation(insieme::core::NodePtr p) {
    if (!p->hasAnnotation(scop::ScopRegion::KEY)) return false;
    auto annot=p->getAnnotation(scop::ScopRegion::KEY);
    return annot->valid;
}

/// Returns the outermost node with a Scop annotation, starting from the given node p. Can be called directly, with
/// explicit scoping.
insieme::core::NodePtr Scop::outermostScopAnnotation(insieme::core::NodePtr p) {

	LOG(WARNING) << "Got SCoP " << p << " - checking maximality" << std::endl;
	unsigned int lvl=0;

	insieme::core::NodePtr annotated;
	if (hasScopAnnotation(p)) annotated=p;
	while (!insieme::core::StatementAddress(p).isRoot()) {
		p=insieme::core::StatementAddress(p).getParentNode(1); lvl++;
		if (hasScopAnnotation(p)) annotated=p;
	}

	LOG(WARNING) << "Moved " << lvl << " levels up, outermost SCoP is at " << annotated << ".\n" << std::endl;
	return annotated;
}

/// Returns true if the given NodePtr is a (maximal) SCoP, false if there is no SCoP annotation, and also false
/// if the SCoP is not maximal. Can be called directly, with explicit scoping.
bool Scop::isScop(insieme::core::NodePtr p) {
    if (!hasScopAnnotation(p)) return false;
    //FIXME: outermostScopAnnotation(p);
    return false;
}

/// Return SCoP data from an existing annotation, using a NodePtr. Can be called directly, with explicit scoping.
Scop& Scop::getScop(insieme::core::NodePtr p) {
    if (!isScop(p)) { THROW_EXCEPTION(scop::NotASCoP, "Given node ptr is not marked as a SCoP.", p); }
    return p->getAnnotation(scop::ScopRegion::KEY)->getScop();
}

/// Return all SCoPs below an existing NodePtr as a vector of NodeAddress'es, just like scop::mark does.
std::vector<insieme::core::NodeAddress> Scop::getScops(insieme::core::NodePtr n) {

	// visit all nodes and save those which are SCoPs (have annotations, and are maximal)
	std::vector<insieme::core::NodeAddress> scops;
	insieme::core::visitDepthFirst(n, [&](const insieme::core::NodePtr &x) {
		if (isScop(x)) scops.push_back(insieme::core::StatementAddress(x));
	});

	// return the saved scops
	return scops;
}

/**
 * @brief Scop::printTo prints a SCoP in human-readable format to out
 * @param out
 * @return the out stream (same as input stream)
 */
std::ostream& Scop::printTo(std::ostream& out) const {
	out << "SCoP [#stmt:" << size() << ", itervec:" << getIterationVector() << "]" << std::endl;
	for_each(begin(), end(), [&](const StmtPtr& cur) {
		out << *cur << std::endl;
	} );
	return out;
}

// Adds a stmt to this scop. 
void Scop::push_back( const Stmt& stmt ) {
	
	AccessList access;
	for_each(stmt.access_begin(), stmt.access_end(), 
			[&] (const AccessInfoPtr& cur) { 
				access.push_back( std::make_shared<AccessInfo>( iterVec, *cur ) ); 
			}
		);

	stmts.emplace_back( std::unique_ptr<Stmt>(
			new Stmt(
				stmt.getId(), 
				stmt.getAddr(), 
				IterationDomain(iterVec, stmt.getDomain()),
				AffineSystem(iterVec, stmt.getSchedule()), 
				access
			)
		) 
	);
	size_t dim = stmts.back()->getSchedule().size();
	if (dim > sched_dim) {
		sched_dim = dim;
	}
}

/** This function determines the maximum number of loop nests within this region The analysis should be improved in a
way that also the loopnest size is weighted with the number of statements present at each loop level. */
size_t Scop::nestingLevel() const {
	size_t max_loopnest=0;
	for_each(begin(), end(), 
		[&](const StmtPtr& scopStmt) { 
			size_t cur_loopnest=scopStmt->loopNest().size();
			if (cur_loopnest > max_loopnest) {
				max_loopnest = cur_loopnest;
			}
		} );
	return max_loopnest;
}

namespace {

/** Creates the scattering map for a statement inside the SCoP. This is done by building the domain for such statement
(adding it to the outer domain). Then the scattering map which maps this statement to a logical execution date is
transformed into a corresponding Map. */
MapPtr<> createScatteringMap(CtxPtr<>&    					ctx, 
									const IterationVector&	iterVec,
									SetPtr<>& 				outer_domain, 
									const Stmt& 				cur, 
									TupleName						tn,
									size_t 							scat_size)
{
	
	auto&& domainSet = makeSet(ctx, cur.getDomain(), tn);
	assert( domainSet && "Invalid domain" );

	// Also the accesses can define restriction on the domain (e.g. MPI calls)
	std::for_each(cur.access_begin(), cur.access_end(), [&](const AccessInfoPtr& cur){
			domainSet *= makeSet(ctx, cur->getDomain(), tn);
		});
	outer_domain = outer_domain + domainSet;
	
	AffineSystem sf = cur.getSchedule();

	// Because the scheduling of every statement has to have the same number of elements
	// (same dimensions) we append zeros until the size of the affine system is equal to 
	// the number of dimensions used inside this SCoP for the scheduling functions 
	for ( size_t s = sf.size(); s < scat_size; ++s ) {
		sf.append( AffineFunction(iterVec) );
	}

	return makeMap(ctx, sf, tn);
}

void buildScheduling(
		CtxPtr<>& 						ctx, 
		const IterationVector& 			iterVec,
		SetPtr<>& 						domain,
		MapPtr<>& 						schedule,
		MapPtr<>& 						reads,
		MapPtr<>& 						writes,
		const Scop::const_iterator& 	begin, 
		const Scop::const_iterator& 	end,
		size_t							schedDim)
		
{
	std::for_each(begin, end, [ & ] (const StmtPtr& cur) { 
		// Creates a name mapping which maps an entity of the IR (StmtAddress) 
		// to a name utilied by the framework as a placeholder 
		TupleName tn(cur, "S" + utils::numeric_cast<std::string>(cur->getId()));

		schedule = schedule + createScatteringMap(ctx, iterVec, domain, *cur, tn, schedDim);

		// Access Functions 
		std::for_each(cur->access_begin(), cur->access_end(), [&](const AccessInfoPtr& cur){
			const AffineSystem& accessInfo = cur->getAccess();

			if (accessInfo.empty())  return;

			auto&& access = 
				makeMap(ctx, accessInfo, tn, TupleName(cur->getExpr(), cur->getExpr()->toString()));

			// Get the domain for this statement
			SetPtr<> dom = makeSet(ctx, cur->getDomain(), tn);
			switch ( cur->getUsage() ) {
			// Uses are added to the set of read operations in this SCoP
			case Ref::USE: 		reads += access * dom;
								break;
			// Definitions are added to the set of writes for this SCoP
			case Ref::DEF: 		writes += access * dom;
								break;
			// Undefined accesses are added as Read and Write operations 
			case Ref::UNKNOWN:	reads  += access * dom;
								writes += access * dom;
								break;
			default:
				assert( false && "Usage kind not defined!" );
			}
		});
	});
}

} // end anonymous namespace

core::NodePtr Scop::toIR(core::NodeManager& mgr, const CloogOpts& opts) const {

	auto&& ctx = makeCtx();

	// universe set 
	auto&& domain 	= makeSet(ctx, IterationDomain(iterVec, true));
	auto&& schedule = makeEmptyMap(ctx, iterVec);
	auto&& reads    = makeEmptyMap(ctx, iterVec);
	auto&& writes   = makeEmptyMap(ctx, iterVec);

	buildScheduling(ctx, iterVec, domain, schedule, reads, writes, begin(), end(), schedDim());

	return polyhedral::toIR(mgr, iterVec, ctx, domain, schedule, opts);
}

MapPtr<> Scop::getSchedule(CtxPtr<>& ctx) const {
	auto&& domain 	= makeSet(ctx, IterationDomain(iterVec, true));
	auto&& schedule = makeEmptyMap(ctx, iterVec);
	auto&& empty    = makeEmptyMap(ctx, iterVec);

	buildScheduling(ctx, iterVec, domain, schedule, empty, empty, begin(), end(), schedDim());
	return schedule;
}

SetPtr<> Scop::getDomain(CtxPtr<>& ctx) const {
	auto&& domain 	= makeSet(ctx, IterationDomain(iterVec, true));
	auto&& schedule = makeEmptyMap(ctx, iterVec);
	auto&& empty    = makeEmptyMap(ctx, iterVec);

	buildScheduling(ctx, iterVec, domain, schedule, empty, empty, begin(), end(), schedDim());
	return domain;
}

MapPtr<> Scop::computeDeps(CtxPtr<>& ctx, const unsigned& type) const {
	// universe set 
	auto&& domain   = makeSet(ctx, IterationDomain(iterVec, true));
	auto&& schedule = makeEmptyMap(ctx, iterVec);
	auto&& reads    = makeEmptyMap(ctx, iterVec);
	auto&& writes   = makeEmptyMap(ctx, iterVec);
	// for now we don't handle may dependencies, therefore we use an empty map
	auto&& may		= makeEmptyMap(ctx, iterVec);

	buildScheduling(ctx, iterVec, domain, schedule, reads, writes, begin(), end(), schedDim());

	// NOTE: We only deal with must dependencies for now
	auto&& mustDeps = makeEmptyMap(ctx, iterVec);

	if ((type & dep::RAW) == dep::RAW) {
		auto&& rawDep = buildDependencies( ctx, domain, schedule, reads, writes, may ).mustDep;
		mustDeps = rawDep;
	}
	
	if ((type & dep::WAR) == dep::WAR) {
		auto&& warDep = buildDependencies( ctx, domain, schedule, writes, reads, may ).mustDep;
		mustDeps += warDep;
	}

	if ((type & dep::WAW) == dep::WAW) {
		auto&& wawDep = buildDependencies( ctx, domain, schedule, writes, writes, may ).mustDep;
		mustDeps += wawDep;
	}

	if ((type & dep::RAR) == dep::RAR) {
		auto&& rarDep = buildDependencies( ctx, domain, schedule, reads, reads, may ).mustDep;
		mustDeps += rarDep;
	}
	return mustDeps;
}


core::NodePtr Scop::optimizeSchedule( core::NodeManager& mgr ) {
	auto&& ctx = makeCtx();

	auto&& domain   = makeSet(ctx, IterationDomain(iterVec, true));
	auto&& schedule = makeEmptyMap(ctx, iterVec);
	auto&& empty    = makeEmptyMap(ctx, iterVec);

	buildScheduling(ctx, iterVec, domain, schedule, empty, empty, begin(), end(), schedDim());
	
	auto&& depsKeep = computeDeps(ctx, dep::RAW | dep::WAR | dep::WAW);
	auto&& depsMin  =	computeDeps(ctx, dep::ALL);
	
	isl_schedule* isl_sched = 
		isl_union_set_compute_schedule(domain->getIslObj(), depsKeep->getIslObj(), depsMin->getIslObj());

	isl_union_map* umap = isl_schedule_get_map(isl_sched);
	isl_schedule_free(isl_sched);

	MapPtr<> map(*ctx, umap);
	
	return polyhedral::toIR(mgr, iterVec, ctx, domain, map);
}

bool Scop::isParallel(core::NodeManager& mgr) const {

	dep::DependenceGraph&& depGraph = 
		dep::extractDependenceGraph(mgr, *this, dep::RAW | dep::WAR | dep::WAW);

	dep::DependenceList depList = depGraph.getDependencies();
	bool parallelizable = true;
	for_each(depList, [&](const dep::DependenceInstance& cur) {
			if( std::get<3>(cur).first.size() > 1 && std::get<3>(cur).first[0] != 0 ) {
				// we have a loop carried dep. in the first dimension, this ScoP is not parallelizable
				parallelizable = false;
			}
		});

	return parallelizable;
}

}}}
