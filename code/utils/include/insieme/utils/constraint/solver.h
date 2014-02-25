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

#include <set>
#include <map>
#include <tuple>

#include <memory>
#include <type_traits>
#include <initializer_list>

#include "insieme/utils/printable.h"
#include "insieme/utils/set_utils.h"
#include "insieme/utils/map_utils.h"
#include "insieme/utils/typed_map.h"

#include "insieme/utils/assert.h"
#include "insieme/utils/constraint/lattice.h"

namespace insieme {
namespace utils {
namespace constraint {

	// forward declaration
	class ValueID;

	class Constraint;
	typedef std::shared_ptr<Constraint> ConstraintPtr;

	class Assignment;


	// ----------------------------- Set IDs ------------------------------

	class ValueID : public Printable {

		int id;

	public:

		ValueID(int id = -1) : id(id) { };
		ValueID(const ValueID& id) : id(id.id) { };

		int getID() const { return id; }

		bool operator==(const ValueID& other) const { return id == other.id; }
		bool operator!=(const ValueID& other) const { return !(*this == other); }
		bool operator<(const ValueID& other) const { return id < other.id; }

		std::ostream& printTo(std::ostream& out) const { return out << "v" << id; }
	};


	template<typename L>
	struct TypedValueID : public ValueID {
		typedef L lattice_type;
		TypedValueID(int id = -1) : ValueID(id) { };
		TypedValueID(const ValueID& id) : ValueID(id) { };
	};

	template<typename E>
	struct TypedSetID : public TypedValueID<SetLattice<E>> {
		TypedSetID(int id = -1) : TypedValueID<SetLattice<E>>(id) { };
		TypedSetID(const ValueID& id) : TypedValueID<SetLattice<E>>(id) { };
	};


} // end namespace constraint
} // end namespace utils
} // end namespace insieme

namespace std {

	template<>
	struct hash<insieme::utils::constraint::ValueID> {
		size_t operator()(const insieme::utils::constraint::ValueID& id) const {
			return id.getID();
		}
	};

} // end namespace std

namespace insieme {
namespace utils {
namespace constraint {


	// ----------------------------- Constraints ------------------------------

	// a common base type for all kind of constraints

	class Constraint : public VirtualPrintable {

		std::vector<ValueID> inputs;
		std::vector<ValueID> outputs;

		bool assignmentDependentDependencies;		// there is a fixed list, but only partially used
		bool dynamicDependencies;					// there is a flexible list

	public:

		enum UpdateResult {
			Unchanged, Incremented, Altered
		};

		Constraint(const std::vector<ValueID>& in, const std::vector<ValueID>& out,
				bool assignmentDependentDependencies = false, bool dynamicDependencies = false)
			: inputs(in), outputs(out),
			  assignmentDependentDependencies(assignmentDependentDependencies || dynamicDependencies),
			  dynamicDependencies(dynamicDependencies) {}

		virtual ~Constraint() {};

		virtual void init(Assignment& ass, vector<ValueID>& workList) const {

			// update dynamic dependencies if necessary
			if (dynamicDependencies) {
				updateDynamicDependencies(ass);
			}

			if (update(ass) != Unchanged) {
				for(auto cur : getOutputs()) {
					workList.push_back(cur);
				}
			}
		}

		virtual UpdateResult update(Assignment& ass) const { return Unchanged; };
		virtual bool check(const Assignment& ass) const =0;

		virtual std::ostream& writeDotEdge(std::ostream& out) const =0;
		virtual std::ostream& writeDotEdge(std::ostream& out, const Assignment& ass) const { return writeDotEdge(out); }

		const std::vector<ValueID>& getInputs() const { return inputs; };
		const std::vector<ValueID>& getOutputs() const { return outputs; };

		bool hasAssignmentDependentDependencies() const { return assignmentDependentDependencies; };
		bool hasDynamicDependencies() const { return dynamicDependencies; }

		virtual bool updateDynamicDependencies(const Assignment& ass) const {
			return dynamicDependencies;		// return whether something might have changed
		}

		virtual const std::vector<ValueID>& getUsedInputs(const Assignment& ass) const {
			assert_false(assignmentDependentDependencies) << "Needs to be implemented by constraints exhibiting assignment based dependencies.";
			return inputs;
		}

	};

	namespace detail {

		typedef std::vector<ValueID> ValueIDs;

		inline ValueIDs combine(const ValueIDs& a, const ValueIDs& b) {
			ValueIDs res = a;
			for(auto x : b) {
				if (!contains(a, x)) {
					res.push_back(x);
				}
			}
			return res;
		}



		template<typename Filter, typename Executor>
		class ComposedConstraint : public Constraint {

			Filter filter;
			Executor executor;

			mutable std::vector<ValueID> usedInputs;

		public:

			ComposedConstraint(const Filter& filter, const Executor& executor)
				: Constraint(combine(filter.getInputs(), executor.getInputs()), executor.getOutputs(), !Filter::is_true), filter(filter), executor(executor) {}

			virtual UpdateResult update(Assignment& ass) const {
				return (filter(ass) && executor.update(ass)) ? Incremented : Unchanged;
			}

			virtual bool check(const Assignment& ass) const {
				return !filter(ass) || executor.check(ass);
			}

			virtual std::ostream& printTo(std::ostream& out) const {
				if (!Filter::is_true) {
					filter.print(out);
					out << " => ";
				}
				executor.print(out);
				return out;
			}

			virtual std::ostream& writeDotEdge(std::ostream& out) const {
				std::stringstream label;
				label << "[label=\"" << *this << "\"]\n";
				executor.writeDotEdge(out, label.str());
				return out;
			}

			virtual std::ostream& writeDotEdge(std::ostream& out, const Assignment& ass) const {
				std::stringstream label;
				label << "[label=\"" << *this << "\"";
				if (!filter(ass)) label << " style=dotted";
				label << "]\n";
				executor.writeDotEdge(out, label.str());
				return out;
			}

			virtual const std::vector<ValueID>& getUsedInputs(const Assignment& ass) const {
				usedInputs.clear();
				filter.addUsedInputs(ass, usedInputs);
				if (filter(ass)) executor.addUsedInputs(ass, usedInputs);
				return usedInputs;
			}
		};

		// -------------------- Filter --------------------------------

		template<bool isTrue = false>
		struct Filter {
			enum { is_true = isTrue };
		};

		struct TrueFilter : public Filter<true> {
			bool operator()(const Assignment& ass) const {
				return true;
			}
			void print(std::ostream& out) const {
				out << "true";
			}
			const ValueIDs& getInputs() const {
				static const ValueIDs empty;
				return empty;
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {}
		};

		template<typename A, typename B>
		struct AndFilter : public Filter<> {
			A a; B b;
			AndFilter(const A& a, const B& b)
				: a(a), b(b) {}
			bool operator()(const Assignment& ass) const {
				return a(ass) && b(ass);
			}
			void print(std::ostream& out) const {
				a.print(out); out << " and "; b.print(out);
			}
			ValueIDs getInputs() const {
				return combine(a.getInputs(), b.getInputs());
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {
				a.addUsedInputs(ass, used);
				if (a(ass)) b.addUsedInputs(ass, used);
			}
		};

		template<typename E, typename L>
		struct ElementOfFilter : public Filter<>{
			E e;
			TypedValueID<L> a;
			ElementOfFilter(const E& e, const TypedValueID<L>& a)
				: e(e), a(a) {}
			bool operator()(const Assignment& ass) const {
				static const typename L::less_op_type less_op;
				return less_op(e,ass[a]);
			}
			void print(std::ostream& out) const {
				out << e << " in " << a;
			}
			ValueIDs getInputs() const {
				return toVector<ValueID>(a);
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {
				used.push_back(a);
			}
		};

		template<typename T>
		struct BiggerThanFilter : public Filter<>{
			TypedValueID<SetLattice<T>> a;
			std::size_t s;
			BiggerThanFilter(const TypedValueID<SetLattice<T>>& a, std::size_t s)
				: a(a), s(s) {}
			bool operator()(const Assignment& ass) const {
				return ass[a].size() > s;
			}
			void print(std::ostream& out) const {
				out << "|" << a << "| > " << s;
			}
			ValueIDs getInputs() const {
				return toVector<ValueID>(a);
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {
				used.push_back(a);
			}
		};

		template<typename T>
		struct BiggerThanReducedFilter : public Filter<>{
			TypedValueID<SetLattice<T>> a;
			T e;
			std::size_t s;
			BiggerThanReducedFilter(const TypedValueID<SetLattice<T>>& a, const T& e, std::size_t s)
				: a(a), e(e), s(s) {}
			bool operator()(const Assignment& ass) const {
				auto& set = ass[a];
				return (set.size() - (contains(set, e)?1:0)) > s;
			}
			void print(std::ostream& out) const {
				out << "|" << a << " - {" << e << "}| > " << s;
			}
			ValueIDs getInputs() const {
				return toVector<ValueID>(a);
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {
				used.push_back(a);
			}
		};


		// -------------------- Executor --------------------------------

		struct Executor {
		protected:

			// a utility function merging sets
			template<typename meet_assign_op, typename A, typename B>
			bool addAll(const A& src, B& trg) const {
				static const meet_assign_op meet_assign;
				// compute meet operation and check for modification
				return meet_assign(trg, src);
			};

			// a utility function merging sets
			template<typename A, typename L>
			bool addAll(Assignment& ass, const A& srcSet, const TypedValueID<L>& trgSet) const {
				return addAll<typename L::meet_assign_op_type>(srcSet, ass[trgSet]);
			};

			// a utility function merging sets
			template<typename L>
			bool addAll(Assignment& ass, const TypedValueID<L>& srcSet, const TypedValueID<L>& trgSet) const {
				const Assignment& cass = ass;
				return addAll(ass, cass[srcSet], trgSet);
			};

			// a utility to check whether a certain set is a subset of another set
			template<typename less_op, typename A, typename B>
			bool isSubset(const A& a, const B& b) const {
				static const less_op less;
				return less(a,b);
			}

			// a utility to check whether a certain set is a subset of another set
			template<typename L>
			bool isSubset(Assignment& ass, const TypedValueID<L>& a, const TypedValueID<L>& b) const {
				return isSubset<typename L::less_op_type>(ass[a], ass[b]);
			}

		};

		template<typename E, typename L>
		class ElementOf : public Executor {
			E e;
			TypedValueID<L> a;
		public:
			ElementOf(const E& e, const TypedValueID<L>& a)
				: e(e), a(a) {}
			const ValueIDs& getInputs() const {
				static const ValueIDs empty;
				return empty;
			}
			ValueIDs getOutputs() const {
				return toVector<ValueID>(a);
			}
			void print(std::ostream& out) const {
				out << e << " in " << a;
			}
			bool update(Assignment& ass) const {
				return addAll<typename L::meet_assign_op_type>(e, ass[a]);
			}
			bool check(const Assignment& ass) const {
				return isSubset<typename L::less_op_type>(e, ass[a]);
			}
			void writeDotEdge(std::ostream& out, const string& label) const {
				out << "e" << (int*)&e << " [label=\"" << e << "\"]\n";
				out << "e" << (int*)&e << " -> " << a << " " << label;
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {
				// nothing
			}
		};

		template<typename L>
		class Subset : public Executor {
			TypedValueID<L> a;
			TypedValueID<L> b;
		public:
			Subset(const TypedValueID<L>& a, const TypedValueID<L>& b)
				: a(a), b(b) { assert(a != b); }
			ValueIDs getInputs() const {
				return toVector<ValueID>(a);
			}
			ValueIDs getOutputs() const {
				return toVector<ValueID>(b);
			}
			void print(std::ostream& out) const {
				out << a << " sub " << b;
			}
			bool update(Assignment& ass) const {
				return addAll(ass, a, b);
			}
			bool check(const Assignment& ass) const {
				return isSubset<typename L::less_op_type>(ass[a], ass[b]);
			}
			void writeDotEdge(std::ostream& out, const string& label) const {
				out << a << " -> " << b << label;
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {
				used.push_back(a);
			}
		};

		template<typename A, typename R>
		class SubsetUnary : public Executor {

			typedef typename A::value_type A_value_type;
			typedef typename R::value_type R_value_type;

			typedef std::function<R_value_type(const A_value_type&)> fun_type;

			TypedValueID<A> a;
			TypedValueID<R> r;
			fun_type f;

		public:
			SubsetUnary(const TypedValueID<A>& a, const TypedValueID<R>& r, const fun_type& f)
				: a(a), r(r), f(f) {}
			ValueIDs getInputs() const {
				return toVector<ValueID>(a);
			}
			ValueIDs getOutputs() const {
				return toVector<ValueID>(r);
			}
			void print(std::ostream& out) const {
				out << "f(" << a << ") sub " << r;
			}
			bool update(Assignment& ass) const {
				return addAll(ass, f(ass[a]), r);
			}
			bool check(const Assignment& ass) const {
				return isSubset<typename R::less_op_type>(f(ass[a]), ass[r]);
			}
			void writeDotEdge(std::ostream& out, const string& label) const {
				out << a << " -> " << r << label;
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {
				used.push_back(a);
			}
		};


		template<typename A, typename B, typename R>
		class SubsetBinary : public Executor {

			typedef typename A::value_type A_value_type;
			typedef typename B::value_type B_value_type;
			typedef typename R::value_type R_value_type;

			typedef std::function<R_value_type(const A_value_type&, const B_value_type&)> fun_type;

			TypedValueID<A> a;
			TypedValueID<B> b;
			TypedValueID<R> r;
			fun_type f;

		public:
			SubsetBinary(const TypedValueID<A>& a, const TypedValueID<B>& b, const TypedValueID<R>& r, const fun_type& f)
				: a(a), b(b), r(r), f(f) {}
			ValueIDs getInputs() const {
				return toVector<ValueID>(a,b);
			}
			ValueIDs getOutputs() const {
				return toVector<ValueID>(r);
			}
			void print(std::ostream& out) const {
				out << "f(" << a << "," << b << ") sub " << r;
			}
			bool update(Assignment& ass) const {
				return addAll(ass, f(ass[a],ass[b]), r);
			}
			bool check(const Assignment& ass) const {
				return isSubset<typename R::less_op_type>(f(ass[a],ass[b]), ass[r]);
			}
			void writeDotEdge(std::ostream& out, const string& label) const {
				out << a << " -> " << r << label << "\n";
				out << b << " -> " << r << label;
			}
			void addUsedInputs(const Assignment& ass, std::vector<ValueID>& used) const {
				used.push_back(a);
				used.push_back(b);
			}
		};

	}	// end of details namespace

	// ----------------------------- Filter Factory Functions ------------------------------

	template<typename E, typename L>
	detail::ElementOfFilter<E,L> f_in(const E& e, const TypedValueID<L>& set) {
		return detail::ElementOfFilter<E,L>(e,set);
	}

	template<typename F1, typename F2>
	typename std::enable_if<std::is_base_of<detail::Filter<false>, F1>::value && std::is_base_of<detail::Filter<false>, F2>::value, detail::AndFilter<F1, F2>>::type
	operator&&(const F1& f1, const F2& f2) {
		return detail::AndFilter<F1,F2>(f1,f2);
	}


	// ----------------------------- Executor Factory Functions ------------------------------

	template<typename E, typename L>
	detail::ElementOf<E,L> e_in(const E& e, const TypedValueID<L>& a) {
		return detail::ElementOf<E,L>(e,a);
	}

	template<typename L>
	detail::Subset<L> e_sub(const TypedValueID<L>& a, const TypedValueID<L>& b) {
		return detail::Subset<L>(a,b);
	}

	// ----------------------------- Constraint Factory Functions ------------------------------


	template<typename F, typename E>
	typename std::enable_if<std::is_base_of<detail::Filter<F::is_true>, F>::value && std::is_base_of<detail::Executor, E>::value, ConstraintPtr>::type
	combine(const F& filter, const E& executor) {
		return std::make_shared<detail::ComposedConstraint<F,E>>(filter, executor);
	}

	template< typename E>
	typename std::enable_if<std::is_base_of<detail::Executor, E>::value, ConstraintPtr>::type
	build(const E& executor) {
		return combine(detail::TrueFilter(), executor);
	}

	template<typename A, typename L>
	ConstraintPtr elem(const A& a, const TypedValueID<L>& b) {
		return combine(detail::TrueFilter(), e_in(a,b));
	}

	template<typename E, typename A, typename F, typename B>
	ConstraintPtr elemIf(const E& e, const TypedValueID<A>& a, const F& f, const TypedValueID<B>& b) {
		return combine(f_in(e,a), e_in(f,b));
	}

	template<typename A, typename L>
	typename std::enable_if<!std::is_base_of<TypedValueID<L>,A>::value, ConstraintPtr>::type
	subset(const A& a, const TypedValueID<L>& b) {
		return combine(detail::TrueFilter(), e_in(a,b));
	}

	template<typename L>
	ConstraintPtr subset(const TypedValueID<L>& a, const TypedValueID<L>& b) {
		return combine(detail::TrueFilter(), e_sub(a,b));
	}

	template<typename E, typename A, typename B>
	ConstraintPtr subsetIf(const E& e, const TypedValueID<A>& a, const TypedValueID<B>& b, const TypedValueID<B>& c) {
		return combine(f_in(e,a), e_sub(b,c));
	}

	template<typename A, typename B, typename C, typename L1, typename L2>
	ConstraintPtr subsetIf(const A& a, const TypedValueID<L1>& as, const B& b, const TypedValueID<L2>& bs, const TypedValueID<C>& in, const TypedValueID<C>& out) {
		return combine(f_in(a,as) && f_in(b,bs), e_sub(in, out));
	}

	template<typename A, typename B>
	ConstraintPtr subsetIfBigger(const TypedValueID<SetLattice<A>>& a, std::size_t s, const TypedValueID<B>& b, const TypedValueID<B>& c) {
		return combine(detail::BiggerThanFilter<A>(a,s), e_sub(b,c));
	}

	template<typename E, typename A>
	ConstraintPtr subsetIfReducedBigger(const TypedValueID<SetLattice<E>>& a, const E& e, std::size_t s, const TypedValueID<A>& b, const TypedValueID<A>& c) {
		return combine(detail::BiggerThanReducedFilter<E>(a,e,s), e_sub(b,c));
	}

	template<typename A, typename R, typename F>
	ConstraintPtr subsetUnary(const TypedValueID<A>& in, const TypedValueID<R>& out, const F& fun) {
		return combine(detail::TrueFilter(), detail::SubsetUnary<A,R>(in, out, fun));
	}

	template<typename A, typename B, typename R, typename F>
	ConstraintPtr subsetBinary(const TypedValueID<A>& a, const TypedValueID<B>& b, const TypedValueID<R>& r, const F& fun) {
		return combine(detail::TrueFilter(), detail::SubsetBinary<A,B,R>(a, b, r, fun));
	}

	// ----------------------------- Constraint Container ------------------------------


	class Constraints : public Printable {

	public:

		typedef std::vector<ConstraintPtr> data_type;
		typedef typename data_type::const_iterator const_iterator;

	private:

		// think about making this a set
		data_type data;

	public:

		Constraints() : data() {}

		Constraints(const std::initializer_list<ConstraintPtr>& list) : data(list) {}

		void add(const ConstraintPtr& constraint) {
			if(constraint) data.push_back(constraint);
		}

		void add(const Constraints& constraints) {
			data.insert(data.end(), constraints.data.begin(), constraints.data.end());
		}

		const std::vector<ConstraintPtr>& getList() const {
			return data;
		}

		const_iterator begin() const { return data.begin(); }
		const_iterator end() const { return data.end(); }

		std::size_t size() const { return data.size(); }

		std::ostream& printTo(std::ostream& out) const {
			return out << "{" << join(",", data, print<deref<ConstraintPtr>>()) << "}";
		}
	};



	// ----------------------------- Assignment ------------------------------

	class Assignment : public Printable {

		struct Container : public VirtualPrintable {
			virtual ~Container() {};
			virtual void append(std::map<ValueID,string>& res) const =0;
			virtual Container* copy() const =0;
			virtual void clear(const ValueID& value) =0;
		};

		template<typename L>
		struct TypedContainer : public Container, public std::map<TypedValueID<L>, typename L::value_type> {
			typedef std::map<TypedValueID<L>, typename L::value_type> map_type;
			typedef typename map_type::value_type value_type;

			virtual std::ostream& printTo(std::ostream& out) const {
				return out << join(",",*this, [](std::ostream& out, const value_type& cur) {
					out << cur.first << "=" << cur.second;
				});
			}
			virtual void append(std::map<ValueID,string>& res) const {
				for(auto& cur : *this) {
					res.insert({cur.first, toString(cur.second)});
				}
			}
			virtual Container* copy() const {
				return new TypedContainer<L>(*this);
			}
			virtual void clear(const ValueID& value) {
				this->erase(value);
			}
		};

		typedef TypedMap<TypedContainer, Container, SetLattice<int>> container_index_type;

		container_index_type data;

	public:

		Assignment() {};

		Assignment(Assignment&& other) = default;

		Assignment(const Assignment& other) : data(other.data) { }

		template<typename L>
		typename L::value_type& get(const TypedValueID<L>& value) {
			return data.get<L>()[value];
		}

		template<typename L>
		const typename L::value_type& get(const TypedValueID<L>& value) const {
			static const typename L::value_type empty;
			auto& map = data.get<L>();
			auto pos = map.find(value);
			if (pos != map.end()) { return pos->second; }
			return empty;
		}

		template<typename L>
		typename L::value_type& operator[](const TypedValueID<L>& value) {
			return get(value);
		}

		template<typename L>
		const typename L::value_type& operator[](const TypedValueID<L>& value) const {
			return get(value);
		}

		Assignment& operator=(const Assignment& other) = default;

		bool operator==(const Assignment& other) const {
			return data == other.data;
		}

		void clear(const ValueID& value) {
			for(auto& cur : data) {
				cur.second->clear(value);
			}
		}

		std::ostream& printTo(std::ostream& out) const {
			return out << data;
		}

		std::map<ValueID,string> toStringMap() const {
			std::map<ValueID,string> res;
			for(auto& cur : data) {
				cur.second->append(res);
			}
			return res;
		}

	};


	// ----------------------------- Solver ------------------------------

	// The type of entities capable of resolving constraints.
	typedef std::function<Constraints(const std::set<ValueID>&)> ConstraintResolver;

	class LazySolver {

		/**
		 * The source of lazy-generated constraints.
		 */
		ConstraintResolver resolver;

		/**
		 * The list of maintained constraints.
		 */
		Constraints constraints;

		/**
		 * The current partial solution.
		 */
		Assignment ass;

		/**
		 * The set of sets for which constraints have already been resolved.
		 */
		std::unordered_set<ValueID> resolved;

		/**
		 * A lazily constructed graph of constraint dependencies.
		 */
		typedef std::unordered_map<ValueID, std::set<const Constraint*>> Edges;
		Edges edges;

		/**
		 * A set of fully resolved constraints (all inputs resolved, just for performance)
		 */
		std::unordered_set<const Constraint*> resolvedConstraints;

	public:

		LazySolver(const ConstraintResolver& resolver, const Assignment& initial = Assignment())
			: resolver(resolver), ass(initial) {}

		/**
		 * Obtains an assignment including the solution of the requested set. This is an incremental
		 * approach and may be used multiple times. Previously computed results will be reused.
		 */
		const Assignment& solve(const ValueID& set);

		/**
		 * Obtains an assignment including solutions for the given sets. This is an incremental
		 * approach and may be used multiple times. Previously computed results will be reused.
		 */
		const Assignment& solve(const std::set<ValueID>& sets);

		/**
		 * Obtains a reference to the list of constraints maintained internally.
		 */
		const Constraints& getConstraints() const {
			return constraints;
		}

		/**
		 * Obtains all constraints for the given value set.
		 */
		const std::set<const Constraint*>& getConstraintsFor(const ValueID& value) const {
			const static std::set<const Constraint*> empty;
			auto pos = edges.find(value);
			return (pos == edges.end()) ? empty : pos->second;
		}

		/**
		 * Obtains a reference to the current assignment maintained internally.
		 */
		const Assignment& getAssignment() const {
			return ass;
		}

		bool isResolved(const ValueID& set) const {
			return resolved.find(set) != resolved.end();
		}

	private:

		// -- internal utility functions ---

		bool hasUnresolvedInput(const Constraint& cur);

		void resolveConstraints(const Constraint& cur, vector<ValueID>& worklist);

		void resolveConstraints(const std::vector<ValueID>& values, vector<ValueID>& worklist);

	};

	// an eager solver implementation
	Assignment solve(const Constraints& constraints, Assignment initial = Assignment());

	// a lazy solver for a single set
	Assignment solve(const ValueID& set, const ConstraintResolver& resolver, Assignment initial = Assignment());

	// a lazy solver implementation
	Assignment solve(const std::set<ValueID>& sets, const ConstraintResolver& resolver, Assignment initial = Assignment());

} // end namespace constraint
} // end namespace utils
} // end namespace insieme

