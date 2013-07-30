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

#include <set>
#include <map>
#include <memory>

#include "insieme/utils/printable.h"
#include "insieme/utils/set_utils.h"
#include "insieme/utils/map_utils.h"

namespace insieme {
namespace utils {
namespace set_constraint {

	typedef int value;

	class Set;
	class Constraint;

	typedef std::shared_ptr<Constraint> ConstraintPtr;

	typedef std::set<Constraint> Constraints;
	typedef std::map<Set, std::set<value>> Assignment;



	class Set : public Printable {

		int id;

	public:

		Set(int id = -1) : id(id) { };

		bool operator==(const Set& other) const { return id == other.id; }
		bool operator!=(const Set& other) const { return !(*this == other); }
		bool operator<(const Set& other) const { return id < other.id; }

		virtual std::ostream& printTo(std::ostream& out) const { return out << "s" << id; }
	};

	// TODO: turn constraints into composable structure
	//			- support checks on set sizes
	//			- support conjunction / chains of checks
	//			- support computation of intersections / differences / unions in checks

	class Constraint : public Printable {

	public:

		enum Kind {
			Elem, Subset, SubsetIfElem, SubsetIfBigger, SubsetIfReducedBigger
		};

	private:

		Kind kind;

		value e, f;

		Set a, b, c;

	public:

		Constraint(const value& e, const Set& a)
			: kind(Elem), e(e), f(-1), a(a), b(-1), c(-1) {}

		Constraint(const Set& a, const Set& b)
			: kind(Subset), e(0), f(-1), a(-1), b(a), c(b) {}

		Constraint(const value& e, const Set& a, const Set& b, const Set& c)
			: kind(SubsetIfElem), e(e), f(-1), a(a), b(b), c(c) {}

		Constraint(const Set& a, const value& e, const Set& b, const Set& c)
			: kind(SubsetIfBigger), e(e), f(-1), a(a), b(b), c(c) {}

		Constraint(const Set& a, const value& t, const value& e, const Set& b, const Set& c)
			: kind(SubsetIfReducedBigger), e(e), f(t), a(a), b(b), c(c) {}

		Kind getKind() const { return kind; }
		const value& getE() const { return e; }
		const value& getF() const { return f; }
		const Set& getA() const { return a; }
		const Set& getB() const { return b; }
		const Set& getC() const { return c; }

		virtual std::ostream& printTo(std::ostream& out) const {

			switch(kind) {
			case Elem: 					return out << e << " in " << a;
			case Subset: 				return out << b << " sub " << c;
			case SubsetIfElem: 			return out << e << " in " << a << " => " << b << " sub " << c;
			case SubsetIfBigger: 		return out << "|" << a << "| > " << e << " => " << b << " sub " << c;
			case SubsetIfReducedBigger: return out << "|" << a << " - {" << f << "}| > " << e << " => " << b << " sub " << c;
			}
			return out << "-invalid constraint-";
		}

		bool operator<(const Constraint& other) const {
			// first criteria: type of constraint
			if (kind != other.kind) return kind < other.kind;
			if (e != other.e) return e < other.e;
			if (f != other.f) return f < other.f;
			if (a != other.a) return a < other.a;
			if (b != other.b) return b < other.b;
			return c < other.c;
		}

		bool check(const Assignment& ass) const {
			static std::set<value> empty;

			auto get = [&](const Set& s)->const std::set<value>& {
				auto pos = ass.find(s);
				return (pos != ass.end())? pos->second : empty;
			};

			return  (kind == Elem && contains(get(a), e)) ||
					(kind == Subset && set::isSubset(get(b), get(c))) ||
					(kind == SubsetIfElem && (!contains(get(a),e) || set::isSubset(get(b), get(c)))) ||
					(kind == SubsetIfBigger && (get(a).size() <= (std::size_t)e || set::isSubset(get(b), get(c)))) ||
					(kind == SubsetIfReducedBigger && (get(a).size() - ((contains(get(a),f))?1:0) <= (std::size_t)e || set::isSubset(get(b), get(c))));
		}

	};

	/**
	 * Creates a constraint ensuring that element e is in subset a.
	 */
	inline Constraint elem(const value& e, const Set& a) {
		return Constraint(e, a);
	}

	/**
	 * Creates a constraint ensuring that set a is a subset of set b.
	 *
	 * a subset b
	 */
	inline Constraint subset(const Set& a, const Set& b) {
		return Constraint(a, b);
	}

	/**
	 * if e is in a then b is a subset of c
	 */
	inline Constraint subsetIf(int e, const Set& a, const Set& b, const Set& c) {
		return Constraint(e,a,b,c);
	}

	/**
	 * if | a | > s  then b \subset c
	 */
	inline Constraint subsetIfBigger(const Set& a, int s, const Set& b, const Set& c) {
		return Constraint(a, s, b, c);
	}

	/**
	 * if | a \ {t} | > s  then b \subset c
	 */
	inline Constraint subsetIfReducedBigger(const Set& a, const value& t, int s, const Set& b, const Set& c) {
		return Constraint(a, t, s, b, c);
	}

	Assignment solve(const Constraints& constraints, const Assignment& initial = Assignment());


} // end namespace set_constraint
} // end namespace utils
} // end namespace insieme
