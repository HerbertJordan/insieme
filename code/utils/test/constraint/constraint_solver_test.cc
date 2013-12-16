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

#include <gtest/gtest.h>

#include "insieme/utils/constraint/solver.h"

#include "insieme/utils/container_utils.h"

namespace insieme {
namespace utils {
namespace constraint {

	TEST(Assignment, Check) {

		Assignment a;

		TypedSetID<int> i1(1);
		TypedSetID<float> i2(2);

		EXPECT_EQ("{}", toString(a));

		a[i1].insert(1);
		a[i2].insert(2.3f);

		EXPECT_EQ("{v2={2.3},v1={1}}", toString(a));

		a[i1] = { 1, 2, 3 };
		a[i2] = { 1, 3 };
		EXPECT_EQ("{v2={1,3},v1={1,2,3}}", toString(a));

	}

	TEST(Constraint, Basic) {

		typedef TypedSetID<int> Set;

		// simple set tests
		Set a = 1;
		Set b = 2;
		Set c = 3;

		EXPECT_EQ("v1", toString(a));

		EXPECT_EQ("v1 sub v2", toString(*subset(a,b)));

		EXPECT_EQ("5 in v1 => v2 sub v3", toString(*subsetIf(5,a,b,c)));

		EXPECT_EQ("|v1| > 5 => v2 sub v3", toString(*subsetIfBigger(a,5,b,c)));

		// constraint test
		Constraints problem = {
				elem(3, a),
				subset(a,b),
				subsetIf(5,a,b,c),
				subsetIfBigger(a,5,b,c),
				subsetIfReducedBigger(a, 3, 2, b, c)
		};

		EXPECT_EQ("{3 in v1,v1 sub v2,5 in v1 => v2 sub v3,|v1| > 5 => v2 sub v3,|v1 - {3}| > 2 => v2 sub v3}", toString(problem));

	}


	TEST(Solver, ConstraintInputSetsIfElem) {

		typedef TypedSetID<int> Set;

		Set a = 1;
		Set b = 2;
		Set c = 3;

		// create constraint to be tested
		auto constraint = subsetIf(0,a,b,c);

		Assignment ass;

		EXPECT_TRUE(constraint->hasAssignmentDependentDependencies());

		EXPECT_EQ("[v1,v2]", toString(constraint->getInputs()));
		EXPECT_EQ("[v3]", toString(constraint->getOutputs()));
		EXPECT_EQ("[v1]", toString(constraint->getUsedInputs(ass)));

		ass[a].insert(0);
		EXPECT_EQ("[v1,v2]", toString(constraint->getUsedInputs(ass)));

	}


	TEST(Constraint, Check) {

		auto s1 = TypedSetID<int>(1);
		auto s2 = TypedSetID<int>(2);

		Assignment a;
		a[s1] = { 1, 2 };
		a[s2] = { 1, 2, 3 };

		auto c = subset(s1, s2);
		EXPECT_TRUE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(1u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = subset(s2,s1);
		EXPECT_FALSE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(1u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = elem( 3, s1);
		EXPECT_FALSE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(0u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = elem( 3, s2);
		EXPECT_TRUE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(0u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = subsetIf( 3, s2, s1, s2);
		EXPECT_TRUE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(2u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = subsetIf( 3, s2, s2, s1);
		EXPECT_FALSE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(2u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = subsetIf( 3, s1, s1, s2);
		EXPECT_TRUE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(1u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = subsetIf( 3, s1, s2, s1);
		EXPECT_TRUE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(1u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = subsetIfBigger(s1, 1, s1, s2);
		EXPECT_TRUE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(2u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = subsetIfBigger(s1, 5, s1, s2);
		EXPECT_TRUE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(1u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

		c = subsetIfBigger(s1, 1, s2, s1);
		EXPECT_FALSE(c->check(a)) << *c << " on " << a;
		EXPECT_EQ(2u, c->getUsedInputs(a).size()) << c->getUsedInputs(a);

	}


	TEST(Solver, Basic) {

		auto s = [](int id) { return TypedSetID<int>(id); };

		Constraints problem = {
				elem(5,s(1)),
				elem(6,s(1)),
				subset(s(1),s(2)),
				subset(s(2),s(3)),
				subset(s(4),s(3)),

				elem(7,s(5)),
				subsetIf(6,s(3),s(5),s(3)),
				subsetIfBigger(s(2),1,s(3),s(6)),
				subsetIfBigger(s(2),3,s(3),s(7)),
				subsetIfBigger(s(2),4,s(3),s(8)),

				subsetIfReducedBigger(s(2),5,0,s(3),s(9)),
				subsetIfReducedBigger(s(2),5,1,s(3),s(10)),
				subsetIfReducedBigger(s(3),5,1,s(3),s(11)),

		};

		auto res = solve(problem);
		EXPECT_EQ("{v1={5,6},v2={5,6},v3={5,6,7},v5={7},v6={5,6,7},v9={5,6,7},v11={5,6,7}}", toString(res));

		EXPECT_EQ("{5,6}", toString(res[s(1)])) << res;
		EXPECT_EQ("{5,6}", toString(res[s(2)])) << res;
		EXPECT_EQ("{5,6,7}", toString(res[s(3)])) << res;
		EXPECT_EQ("{}", toString(res[s(4)])) << res;
		EXPECT_EQ("{7}", toString(res[s(5)])) << res;
		EXPECT_EQ("{5,6,7}", toString(res[s(6)])) << res;
		EXPECT_EQ("{}", toString(res[s(7)])) << res;
		EXPECT_EQ("{}", toString(res[s(8)])) << res;
		EXPECT_EQ("{5,6,7}", toString(res[s(9)])) << res;
		EXPECT_EQ("{}", toString(res[s(10)])) << res;
		EXPECT_EQ("{5,6,7}", toString(res[s(11)])) << res;

		// check the individual constraints
		for (const auto& cur : problem) {
			EXPECT_TRUE(cur->check(res)) << cur;
		}

	}

	TEST(Solver, Functions) {

		auto s = [](int id) { return TypedSetID<int>(id); };

		auto inc = [](const std::set<int>& a)->std::set<int> {
			std::set<int> res;
			for(auto x : a) { res.insert(x+1); }
			return res;
		};

		auto add = [](const std::set<int>& a, const std::set<int>& b)->std::set<int> {
			std::set<int> res;
			for(auto x : a) {
				for (auto y : b) {
					res.insert(x+y);
				}
			}
			return res;
		};

		Constraints problem = {

				elem(5, s(1)),
				elem(6, s(2)),
				elem(7, s(2)),

				subsetUnary(s(1), s(3), inc),
				subsetUnary(s(2), s(4), inc),

				subsetBinary(s(1), s(2), s(5), add),
		};

		auto res = solve(problem);
		EXPECT_EQ("{v1={5},v2={6,7},v3={6},v4={7,8},v5={11,12}}", toString(res));

		EXPECT_EQ("{5}", toString(res[s(1)])) << res;
		EXPECT_EQ("{6,7}", toString(res[s(2)])) << res;
		EXPECT_EQ("{6}", toString(res[s(3)])) << res;
		EXPECT_EQ("{7,8}", toString(res[s(4)])) << res;
		EXPECT_EQ("{11,12}", toString(res[s(5)])) << res;

	}

	TEST(Solver, Lazy) {

		// lazy-evaluated faculty values
		auto resolver = [](const std::set<ValueID>& sets)->Constraints {
			Constraints res;
			for(auto cur : sets) {
				int id = cur.getID();
				if (id == 0) {
					res.add(elem(0, TypedSetID<int>(id)));
				} else if (id == 1 || id == 2) {
					res.add(elem(1, TypedSetID<int>(id)));
				} else {
					TypedSetID<int> a(id-1);
					TypedSetID<int> b(id-2);
					TypedSetID<int> r(id);
					res.add(subsetBinary(a, b, r, [](const std::set<int>& a, const std::set<int>& b)->std::set<int> {
						std::set<int> res;
						for( int x : a) for (int y : b) res.insert(x+y);
						return res;
					}));
				}
			}
			return res;
		};

		// see whether we can compute something
		auto res = solve(TypedSetID<int>(4), resolver);
//		std::cout << res << "\n";
		EXPECT_EQ("{3}", toString(res[TypedSetID<int>(4)]));

		// see whether we can compute something
		res = solve(TypedSetID<int>(46), resolver);
//		std::cout << res << "\n";
		EXPECT_EQ("{1836311903}", toString(res[TypedSetID<int>(46)]));
	}


	namespace {

		struct Pair : public std::pair<int,int> {
			Pair(int a = 10, int b = 10) : std::pair<int,int>(a,b) {}
		};

		struct pair_meet_assign_op {
			bool operator()(Pair& a, const Pair& b) const {
				bool res = false;
				if (a.first > b.first) {
					a.first = b.first;
					res = true;
				}
				if (a.second > b.second) {
					a.second = b.second;
					res = true;
				}
				return res;
			}
		};

		struct pair_less_op {
			bool operator()(const Pair& a, const Pair& b) const {
				return a.first >= b.first && a.second >= b.second;
			}
		};
	}


	TEST(Solver, Lattice) {

		typedef Lattice<Pair, pair_meet_assign_op, pair_less_op> PairLattice;
		auto s = [](int id) { return TypedValueID<PairLattice>(id); };

		Constraints problem = {
				subset(Pair(5,8),s(1)),
				subset(Pair(8,5),s(1)),
				subset(Pair(5,8),s(2)),
				subset(Pair(8,5),s(3)),
				subset(s(2),s(4)),
				subset(s(3),s(4))
		};

		auto res = solve(problem);
		EXPECT_EQ("{v1=(5,5),v2=(5,8),v3=(8,5),v4=(5,5)}", toString(res));

		// check the individual constraints
		for (const auto& cur : problem) {
			EXPECT_TRUE(cur->check(res)) << "Constraint: " << *cur;
		}
	}


	namespace {

		struct IncrementConstraint : public Constraint {

			TypedSetID<int> in;
			TypedSetID<int> out;

			IncrementConstraint(const TypedSetID<int>& in, const TypedSetID<int>& out)
				: Constraint(toVector<ValueID>(in), toVector<ValueID>(out)), in(in), out(out) {}

			virtual Constraint::UpdateResult update(Assignment& ass) const {
				const auto& sin  = ass[in];
				auto& sout = ass[out];

				// if there is not only one value in the in-set reset counter
				if (sin.size() != 1u) {
					sout.clear();
					sout.insert(1);
					return Constraint::Altered;
				}

				// if there is one value in the set, increment it
				int value = *sin.begin();
				if (value < 10) {
					sout.clear();
					sout.insert(value+1);
					return Constraint::Altered;
				}

				// otherwise do nothing
				return Constraint::Unchanged;

			};
			virtual bool check(const Assignment& ass) const { return true; }

			virtual std::ostream& writeDotEdge(std::ostream& out) const { return out << in << "->" << out << "\n"; }
			virtual std::ostream& writeDotEdge(std::ostream& out, const Assignment& ass) const { return writeDotEdge(out); }

			virtual bool hasAssignmentDependentDependencies() const { return false; }
			virtual std::vector<ValueID> getUsedInputs(const Assignment& ass) const { return toVector<ValueID>(in); }

			virtual std::ostream& printTo(std::ostream& out) const { return out << this->out << " += " << in; }

		};


		ConstraintPtr increment(const TypedSetID<int>& in, const TypedSetID<int>& out) {
			return std::make_shared<IncrementConstraint>(in, out);
		}

	}


	TEST(Solver, ResetConstraintsEager) {

		auto s = [](int id) { return TypedSetID<int>(id); };

		Constraints problem = {
				subset   (s(1),s(2)),
				subset	 (s(2),s(3)),
				increment(s(3),s(1))
		};

		auto res = solve(problem);
		EXPECT_EQ("{v1={10},v2={10},v3={10}}", toString(res));

		// check the individual constraints
		for (const auto& cur : problem) {
			EXPECT_TRUE(cur->check(res)) << "Constraint: " << *cur;
		}

	}

	TEST(Solver, ResetConstraintsLazy) {

		auto s = [](int id) { return TypedSetID<int>(id); };

		Constraints problem = {
				subset   (s(1),s(2)),
				subset	 (s(2),s(3)),
				increment(s(3),s(1))
		};


		// lazy-evaluated faculty values
		auto resolver = [&](const std::set<ValueID>& sets)->Constraints {
			Constraints res;
			for(auto cur : sets) {
				int id = cur.getID();
				if (id == 1) {
					res.add(increment(s(3),s(1)));
				} else if (id == 2) {
					res.add(subset(s(1),s(2)));
				} else if (id == 3) {
					res.add(subset(s(2),s(3)));
				}
			}
			return res;
		};

		// LazySolver
		auto res = solve(s(1), resolver);
		EXPECT_EQ("{v1={10},v2={10},v3={10}}", toString(res));

	}


	namespace {

		struct DynamicConstraint : public Constraint {

			TypedSetID<TypedSetID<int>> set;			// the set containing elements to be aggregated
			TypedSetID<int> out;						// the result set

			DynamicConstraint(const TypedSetID<TypedSetID<int>>& set, const TypedSetID<int>& out)
				: Constraint(toVector<ValueID>(set), toVector<ValueID>(out), true, true), set(set), out(out) {}

			virtual Constraint::UpdateResult update(Assignment& ass) const {
				bool changed = false;
				const std::set<TypedSetID<int>>& sets = ass[set];
				for(auto cur : sets) {
					for(auto e : (const std::set<int>&)(ass[cur])) {
						changed = ass[out].insert(e).second || changed;
					}
				}
				return (changed) ? Constraint::Incremented : Constraint::Unchanged;
			};

			virtual bool check(const Assignment& ass) const {
				const std::set<int>& value = ass[out];
				const std::set<TypedSetID<int>>& sets = ass[set];
				for(auto cur : sets) {
					for(auto e : (const std::set<int>&)(ass[cur])) {
						if (!contains(value, e)) return false;
					}
				}
				return true;
			}

			virtual std::ostream& writeDotEdge(std::ostream& out) const { assert_not_implemented(); return out; }
			virtual std::ostream& writeDotEdge(std::ostream& out, const Assignment& ass) const { return writeDotEdge(out); }

			virtual std::vector<ValueID> getUsedInputs(const Assignment& ass) const {
				std::vector<ValueID> res;
				res.push_back(set);
				const std::set<TypedSetID<int>>& sets = ass[set];
				res.insert(res.end(), sets.begin(), sets.end());
				return res;
			}

			virtual std::ostream& printTo(std::ostream& out) const { return out << "union(all s in " << set << ") sub " << this->out; }

		};


		ConstraintPtr collect(const TypedSetID<TypedSetID<int>>& sets, const TypedSetID<int>& out) {
			return std::make_shared<DynamicConstraint>(sets, out);
		}


		typedef std::map<ValueID, std::vector<ConstraintPtr>> ConstraintMap;

		struct MapResolver {

			ConstraintMap map;

		public:

			MapResolver(const ConstraintMap& map) : map(map) {}

			Constraints operator()(const std::set<ValueID>& values) const {
				Constraints res;

				for(auto value : values) {
					auto pos = map.find(value);
					if (pos != map.end()) {
						for(auto cur : pos->second) {
							res.add(cur);
						}
					}
				}
				return res;
			}

		};
	}

	TEST(Solver, DynamicDependenciesEager) {

		auto s = [](int id) { return TypedSetID<int>(id); };
		auto m = [](int id) { return TypedSetID<TypedSetID<int>>(id); };

		Constraints problem = {
				elem	 ( 1, s(1)),
				elem	 ( 2, s(1)),
				elem	 ( 4, s(2)),
				elem	 ( 6, s(2)),
				elem	 ( 8, s(3)),
				elem	 (10, s(4)),
				elem	 (s(1), m(10)),
				elemIf	 (2, s(5), s(2), m(10)),
				elemIf   (4, s(5), s(3), m(10)),
				collect  (m(10), s(5))
		};

		auto res = solve(problem);
		EXPECT_EQ("{v10={v1,v2,v3},v1={1,2},v2={4,6},v3={8},v4={10},v5={1,2,4,6,8}}", toString(res));

		// check the individual constraints
		for (const auto& cur : problem) {
			EXPECT_TRUE(cur->check(res)) << "Constraint: " << *cur;
		}

	}

	TEST(Solver, DynamicDependenciesLazy) {

		auto s = [](int id) { return TypedSetID<int>(id); };
		auto m = [](int id) { return TypedSetID<TypedSetID<int>>(id); };

		ConstraintMap map;

		map[s(1)] = toVector(elem(1, s(1)), elem( 2, s(1)));
		map[s(2)] = toVector(elem(4, s(2)), elem( 6, s(2)));
		map[s(3)] = toVector(elem(8, s(3)));
		map[s(4)] = toVector(elem(10, s(4)));
		map[s(5)] = toVector(collect  (m(10), s(5)));

		map[m(10)] = toVector(
				elem(s(1), m(10)),
				elemIf(2, s(5), s(2), m(10)),
				elemIf(4, s(5), s(3), m(10))
		);

		// LazySolver
		auto res = solve(s(5), MapResolver(map));
		EXPECT_EQ("{v10={v1,v2,v3},v1={1,2},v2={4,6},v3={8},v5={1,2,4,6,8}}", toString(res));

	}


} // end namespace set_constraint
} // end namespace utils
} // end namespace insieme
