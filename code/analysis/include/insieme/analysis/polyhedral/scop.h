/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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

#include <iterator>
#include <list>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

#include "boost/mpl/or.hpp"
#include "boost/operators.hpp"
#include "boost/optional.hpp"

#include "insieme/analysis/defuse_collect.h"
#include "insieme/analysis/dep_graph.h"
#include "insieme/analysis/polyhedral/affine_func.h"
#include "insieme/analysis/polyhedral/affine_sys.h"
#include "insieme/analysis/polyhedral/backend.h"
#include "insieme/analysis/polyhedral/constraint.h"
#include "insieme/analysis/polyhedral/iter_dom.h"
#include "insieme/analysis/polyhedral/iter_vec.h"
#include "insieme/core/arithmetic/arithmetic.h"
#include "insieme/core/ir_node.h"
#include "insieme/core/ir_pointer.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/utils/constraint.h"
#include "insieme/utils/matrix.h"
#include "insieme/utils/printable.h"

namespace insieme { namespace analysis { namespace polyhedral {

typedef std::vector<core::NodeAddress> 					AddressList;
typedef std::pair<core::NodeAddress, IterationDomain> 	SubScop;
typedef std::list<SubScop> 								SubScopList;

// TODO: integrate SubScop infrastructure into Scop infrastructure
std::list<SubScop> toSubScopList(const IterationVector& iterVec, const AddressList& scops);

/** AccessInfo is a tuple which gives the list of information associated to a ref access: i.e.
the pointer to a RefPtr instance (containing the ref to the variable being accessed and the
type of access (DEF or USE). The iteration domain which defines the domain on which this
access is defined and the access functions for each dimensions.

**Access functions** identify the read (USE) and writes (DEF) happening inside a statement */
class AccessInfo : public utils::Printable {

	core::ExpressionAddress	  expr;
	Ref::RefType			  type;
	Ref::UseType			  usage;
	AffineSystemPtr	  		  access;
	IterationDomain	  		  domain;
public:
	AccessInfo(
		const core::ExpressionAddress&	expr, 
		const Ref::RefType& 			type, 
		const Ref::UseType& 			usage, 
		const AffineSystem&				access,
		const IterationDomain&	   		domain
	) : expr(expr), 
		type(type), 
		usage(usage), 
		access( std::make_shared<AffineSystem>(access) ),
		domain(domain) { }

	AccessInfo(const AccessInfo& other) : 
		expr(other.expr), 
		type(other.type), 
		usage(other.usage), 
		access( std::make_shared<AffineSystem>(*other.access) ),
		domain( other.domain ) { }

	/// Copy constructor with base (iterator vector) change
	AccessInfo( const IterationVector& iterVec, const AccessInfo& other) : 
		expr(other.expr), type(other.type), usage(other.usage), 
		access( std::make_shared<AffineSystem>(iterVec, *other.access) ),
		domain( IterationDomain(iterVec, other.domain) ) { } 

	// Getters for expr/type and usage
	inline const core::ExpressionAddress& getExpr() const { return expr; }
	inline const Ref::RefType& getRefType() const { return type; }
	inline const Ref::UseType& getUsage() const { return usage; }

	// Getters/Setters for access functions 
	inline AffineSystem& getAccess() { return *access; }
	inline const AffineSystem& getAccess() const { return *access; }

	inline const IterationDomain& getDomain() const { return domain; }

	inline bool hasDomainInfo() const { return !domain.universe(); }

	// implementing the printable interface
	std::ostream& printTo(std::ostream& out) const;
};

typedef std::shared_ptr<AccessInfo> AccessInfoPtr;

/** Stmt: this class contains all necessary information which are together representing a
    statement in the polyhedral model.

    A statement is represented by:
    - an **iteration domain** (with an associated iteration vector)
    - a **scheduling** (or scattering) matrix (which defines the schedule for the statement to be
      executed)
	- an **access matrix** defining the access (DEF/USE) of elements within a statement
*/
class Stmt: public utils::Printable {
	core::StatementAddress addr; ///< the root of the address is the entry point of the SCoP
	AffineSystem schedule;       ///< scheduling matrix, according to the literature

public:
 unsigned int id;			   ///< a statement number, according to the index x in the term S_x from the literature
 IterationDomain iterdomain;   ///< iteration domain, according to the literature
 std::vector<AccessInfoPtr>    ///  access matrix, together with reference address, type of usage (USE/DEF/UNKNOWN)
	 accessmtx;                ///< (see also class AccessInfo)

	Stmt(unsigned int id,
		 const core::StatementAddress &addr,
		 const IterationDomain &iterdomain,
		 const AffineSystem &schedule,
		 const std::vector<AccessInfoPtr> &accessmtx=std::vector<AccessInfoPtr>())
		: addr(addr), schedule(schedule), id(id), iterdomain(iterdomain), accessmtx(accessmtx) {}
	Stmt(const IterationVector &iterVec, size_t id, const Stmt &other);

	std::ostream& printTo(std::ostream& out) const;
	inline const core::StatementAddress& getAddr() const { return addr; }
	inline AffineSystem& getSchedule() { return schedule; }
	inline const AffineSystem& getSchedule() const { return schedule; }

	std::vector<core::VariablePtr> loopNest() const;
	unsigned getSubRangeNum();
};
typedef std::shared_ptr<Stmt> StmtPtr;

/** The Scop class is the entry point for all polyhedral model analyses and transformations. The
 purpose is to fully represent all the information of a polyhedral Static Control Part (SCoP).
 Copies of this class can be created so that transformations to the model can be applied without
 changing other instances of this SCoP.

 By default a Scop object is associated to a polyhedral region using the ScopRegion annotation.
 When a transformation needs to be performed a deep copy of the Scop object is created and
 transformations are applied to it. */
struct Scop : public utils::Printable {

public:
	IterationVector iterVec;
	typedef std::vector<StmtPtr> StmtVect;
	typedef StmtVect::iterator iterator;
	typedef StmtVect::const_iterator const_iterator;
	StmtVect stmts; /// <- all statements in the SCoP, root address is the entry point of SCoP
	size_t sched_dim;

	Scop(const IterationVector& iterVec, const StmtVect& stmts = StmtVect()) :
		iterVec(iterVec), sched_dim(0) {
		// rewrite all the access functions in terms of the new iteration vector
		for (auto s: stmts) {
			insieme::analysis::polyhedral::Stmt poly_stmt=Stmt(this->iterVec, s->id, *s);
			this->push_back(poly_stmt);
		};
	}

	/// Copy constructor builds a deep copy of this SCoP.
	explicit Scop(const Scop& other) : iterVec(other.iterVec), sched_dim(other.sched_dim) {
		for_each(other.stmts, [&] (const StmtPtr& stmt) { this->push_back( *stmt ); });
	}

	/// Move constructor
	Scop(Scop&& that): iterVec(std::move(that.iterVec)), stmts(std::move(that.stmts)), sched_dim(that.sched_dim) {}

	static bool hasScopAnnotation(insieme::core::NodePtr p);
	static insieme::core::NodePtr outermostScopAnnotation(insieme::core::NodePtr p);
	static bool isScop(insieme::core::NodePtr p);
	static Scop& getScop(insieme::core::NodePtr p);
	static std::vector<insieme::core::NodeAddress> getScops(insieme::core::NodePtr n);
	std::ostream& printTo(std::ostream& out) const;

	/// push_back adds a stmt to this SCoP
	void push_back(Stmt &stmt);

	inline const IterationVector& getIterationVector() const { return iterVec; }
	inline IterationVector& getIterationVector() { return iterVec; }

	inline const StmtVect& getStmts() const { return stmts; }

	// Get iterators thorugh the statements contained in this SCoP
	inline iterator begin() { return stmts.begin(); }
	inline iterator end() { return stmts.end(); }

	inline const_iterator begin() const { return stmts.begin(); }
	inline const_iterator end() const { return stmts.end(); }

	// Access statements based on their ID
	inline const Stmt& operator[](size_t pos) const { return *stmts[pos]; }
	inline Stmt& operator[](size_t pos) { return *stmts[pos]; }

	inline size_t size() const { return stmts.size(); }
	inline const size_t& schedDim() const { return sched_dim; }
	inline size_t& schedDim() { return sched_dim; }

	size_t nestingLevel() const;

	/**
	 * Produces IR code from this SCoP. 
	 */
	core::NodePtr toIR(core::NodeManager& mgr, const CloogOpts& opts = CloogOpts()) const;
	
	MapPtr<> getSchedule(CtxPtr<>& ctx) const;

	SetPtr<> getDomain(CtxPtr<>& ctx) const;

	/**
	 * Computes analysis information for this SCoP
	 */
	MapPtr<> computeDeps(CtxPtr<>& ctx, const unsigned& d = 
			analysis::dep::RAW | analysis::dep::WAR | analysis::dep::WAW) const;


	bool isParallel(core::NodeManager& mgr) const;

	core::NodePtr optimizeSchedule(core::NodeManager& mgr);
};

}}}
