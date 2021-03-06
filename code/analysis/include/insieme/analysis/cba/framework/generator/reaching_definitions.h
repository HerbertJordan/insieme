/**
 * Copyright (c) 2002-2014 Distributed and Parallel Systems Group,
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

#include "insieme/analysis/cba/framework/analysis_type.h"
#include "insieme/analysis/cba/framework/entities/definition.h"
#include "insieme/analysis/cba/framework/generator/basic_program_point.h"
#include "insieme/analysis/cba/framework/generator/killed_definitions.h"

#include "insieme/core/forward_decls.h"
#include "insieme/core/analysis/ir_utils.h"

namespace insieme {
namespace analysis {
namespace cba {

	// forward definitions
	struct killed_defs_out_analysis;
	extern const killed_defs_out_analysis KDout;


	// ----------------- reaching definitions ---------------

	template<typename Context> class ReachingDefsInConstraintGenerator;
	template<typename Context> class ReachingDefsTmpConstraintGenerator;
	template<typename Context> class ReachingDefsOutConstraintGenerator;

	struct reaching_defs_in_analysis  : public location_based_set_analysis<Definition,  ReachingDefsInConstraintGenerator> {};
	struct reaching_defs_tmp_analysis : public location_based_set_analysis<Definition,  ReachingDefsTmpConstraintGenerator> {};
	struct reaching_defs_out_analysis : public location_based_set_analysis<Definition, ReachingDefsOutConstraintGenerator> {};

	extern const reaching_defs_in_analysis  RDin;
	extern const reaching_defs_tmp_analysis RDtmp;
	extern const reaching_defs_out_analysis RDout;


	// a auxiliary value type utilized within merge nodes for code-reuse
	template<typename Context> class ReachingDefsMergeConstraintGenerator;
	struct reaching_defs_merge_analysis : public location_based_set_analysis<Definition, ReachingDefsMergeConstraintGenerator> {};
	extern const reaching_defs_merge_analysis RDmerge;

	namespace {

		template<
			typename Context,
			typename ReachingDefValue,
			typename RefValue
		>
		class ReachingDefsConstraint : public Constraint {

			typedef typename ReachingDefValue::less_op_type less_op;

			const Location<Context> loc;
			const Definition<Context> def;
			const TypedVariable<ReachingDefValue> in;
			const TypedVariable<ReachingDefValue> out;
			const TypedVariable<RefValue> ref;

			mutable std::vector<Variable> inputs;

		public:

			ReachingDefsConstraint(
					const Location<Context>& loc, const Definition<Context>& def,
					const TypedVariable<ReachingDefValue>& in, const TypedVariable<ReachingDefValue>& out,
					const TypedVariable<RefValue>& ref)
				: Constraint(toVector<Variable>(in, ref), toVector<Variable>(out), true),
				  loc(loc), def(def), in(in), out(out), ref(ref) {}

			virtual Constraint::UpdateResult update(Assignment& ass) const {
				const static less_op less;

				// get reference to current value
				set<Definition<Context>>& value = ass[out];

				// compute new value
				set<Definition<Context>> updated = getUpdatedValue(ass);

				// check whether something has changed
				if (less(value,updated) && less(updated,value)) return Constraint::Unchanged;

				// check whether new value is proper subset
				auto res = (less(value, updated) ? Constraint::Incremented : Constraint::Altered);

				// update value
				value = updated;

				// return change-summary
				return res;
			}

			virtual bool check(const Assignment& ass) const {
				const static less_op less;
				return less(getUpdatedValue(ass), ass[out]);
			}

			virtual std::ostream& writeDotEdge(std::ostream& out) const {
				return
					out << ref << " -> " << this->out << "[label=\"" << *this << "\"]\n"
						<< in << " -> " << this->out << "[label=\"" << *this << "\"]\n";
			}

			virtual std::ostream& writeDotEdge(std::ostream& out, const Assignment& ass) const {

				const set<Reference<Context>>& refs = ass[ref];

				bool isDefinition = any(refs, [&](const Reference<Context>& ref)->bool { return ref == loc; });
				bool isNotUnique  = !(refs.size() == 1u && *refs.begin() == Reference<Context>(loc));

				return
					out << ref << " -> " << this->out << "[label=\"" << loc << " touched by " << ref << "\"" << ((isDefinition)?"":" style=dotted") << "]\n"
						<< in << " -> " << this->out << "[label=\"if not " << ref << " uniquely address " << loc << "\"" << ((isNotUnique)?"":" style=dotted") << "]\n";
			}

			virtual std::ostream& printTo(std::ostream& out) const {
				return out << " Reference of " << loc << " " << ref << " => combine(" << ref << "," << in << ") in " << this->out;
			}

			virtual const std::vector<Variable>& getUsedInputs(const Assignment& ass) const {

				// reset the list of used inputs
				inputs.clear();

				// we always have to read the ref set
				inputs.push_back(ref);

				// check whether reference is unique
				const set<Reference<Context>>& refs = ass[ref];
				if (refs.empty()) return inputs;
				if (refs.size() == 1u && *refs.begin() == Reference<Context>(loc)) return inputs;

				// if not, we also need the in set
				inputs.push_back(in);
				return inputs;
			}

		private:

			set<Definition<Context>> getUpdatedValue(const Assignment& ass) const {

				set<Definition<Context>> res;

				// check reference
				const set<Reference<Context>>& refs = ass[ref];

				// if still empty => nothing happens
				if (refs.empty()) return res;

				// check whether a local reference is included

				// if it is only referencing the observed location => it is a new definition
				if (refs.size() == 1u && *refs.begin() == Reference<Context>(loc)) {
					res.insert(def);
					return res;
				}

				// otherwise out is subset of in
				const set<Definition<Context>>& in_values = ass[in];
				res.insert(in_values.begin(), in_values.end());

				// and if loc is referenced, this one might be a new definition
				if (any(refs, [&](const Reference<Context>& ref)->bool { return ref.getLocation() == loc; })) {
					res.insert(def);
				}

				// done
				return res;
			}

		};


		template<typename RefValue, typename RDValue, typename Context>
		ConstraintPtr reachingDefs(const Location<Context>& loc, const Definition<Context>& def, const TypedVariable<RefValue>& updatedRef, const TypedVariable<RDValue>& in_state, const TypedVariable<RDValue>& out_state) {
			return std::make_shared<ReachingDefsConstraint<Context,RDValue,RefValue>>(loc, def, in_state, out_state, updatedRef);
		}

	}


	template<typename Context>
	class ReachingDefsInConstraintGenerator
		: public BasicInConstraintGenerator<
		  	  reaching_defs_in_analysis,
		  	  reaching_defs_tmp_analysis,
		  	  reaching_defs_out_analysis,
		  	  ReachingDefsInConstraintGenerator<Context>,
		  	  Context,
		  	  Location<Context>
		  > {

		typedef BasicInConstraintGenerator<
			  	  reaching_defs_in_analysis,
			  	  reaching_defs_tmp_analysis,
			  	  reaching_defs_out_analysis,
			  	  ReachingDefsInConstraintGenerator<Context>,
			  	  Context,
			  	  Location<Context>
			 > super;

		CBA& cba;

	public:

		ReachingDefsInConstraintGenerator(CBA& cba) : super(cba, RDin, RDtmp, RDout), cba(cba) {}

	};

	template<typename Context>
	class ReachingDefsTmpConstraintGenerator : public BasicTmpConstraintGenerator<reaching_defs_in_analysis, reaching_defs_tmp_analysis, reaching_defs_out_analysis, Context, Location<Context>> {

		typedef BasicTmpConstraintGenerator<reaching_defs_in_analysis, reaching_defs_tmp_analysis, reaching_defs_out_analysis, Context, Location<Context>> super;

	public:

		ReachingDefsTmpConstraintGenerator(CBA& cba) : super(cba, RDin, RDtmp, RDout) {}

	};

	namespace detail {

		bool isAssignmentFree(const NodePtr& code);

	}

	template<typename Context>
	class ReachingDefsOutConstraintGenerator
		: public BasicOutConstraintGenerator<
		  	  reaching_defs_in_analysis,
		  	  reaching_defs_tmp_analysis,
		  	  reaching_defs_out_analysis,
		  	  ReachingDefsOutConstraintGenerator<Context>,
		  	  Context,
		  	  Location<Context>
		  > {

		typedef BasicOutConstraintGenerator<
				  reaching_defs_in_analysis,
				  reaching_defs_tmp_analysis,
				  reaching_defs_out_analysis,
				  ReachingDefsOutConstraintGenerator<Context>,
				  Context,
				  Location<Context>
			 > super;


		CBA& cba;

	public:

		ReachingDefsOutConstraintGenerator(CBA& cba) : super(cba, RDin, RDtmp, RDout), cba(cba) {}

		virtual void visit(const NodeInstance& addr, const Context& ctxt, const Location<Context>& loc, Constraints& constraints) {
			// we can stop at the creation point - no definitions will be killed before
			if (loc.getCreationPoint() == addr) {

				auto RD_out = cba.getVar(RDout, loc.getCreationPoint(), ctxt, loc);
				constraints.add(elem(set<Definition<Context>>(), RD_out));
				return;
			}

			// if the current expression is assignment free we can skip the inner part
			if (detail::isAssignmentFree(addr)) {
				auto RD_in  = cba.getVar( RDin, addr.as<StatementInstance>(), ctxt, loc);
				auto RD_out = cba.getVar(RDout, addr.as<StatementInstance>(), ctxt, loc);
				constraints.add(subset(RD_in, RD_out));
				return;
			}

			// all others should be handled as usual
			super::visit(addr, ctxt, loc, constraints);
		}

		void visitCallExpr(const CallExprInstance& call, const Context& ctxt, const Location<Context>& loc, Constraints& constraints) {
			const auto& base = call->getNodeManager().getLangBasic();

			// one special case: assignments
			auto fun = call.as<CallExprPtr>()->getFunctionExpr();
			if (base.isRefAssign(fun)) {

				// TODO: check referenced location
				//		- if not referenced => out = in
				//		- if only reference => in definitions are not forwarded
				//		- if on of many => in definition + local definition

				// collect effects of parameters (and function evaluation) into RDtmp


				// get involved sets
				auto RD_tmp = cba.getVar(RDtmp, call, ctxt, loc);		// the definitions reaching the assignment (after processing all arguments)
				auto RD_out = cba.getVar(RDout, call, ctxt, loc);		// the definitions
				auto R_trg = cba.getVar(R, call[0], ctxt);				// set of references locations

				// add constraint
				constraints.add(reachingDefs(loc, Definition<Context>(call, ctxt), R_trg, RD_tmp, RD_out));

				// done
				return;
			}

			// other special case: merge
			if (base.isMerge(fun)) {

				// In this case we have to:
				// 		- get the union of the reaching definitions of all merged threads (=> RDmerge)
				//		- subtract the set of killed definitions (KDout of merge call)

				// get involved sets
				auto RD_merge = cba.getVar(RDmerge, call, ctxt, loc);
				auto KD_out = cba.getVar(KDout, call, ctxt, loc);
				auto RD_out = cba.getVar(RDout, call, ctxt, loc);

				// compute set difference
				constraints.add(subsetBinary(RD_merge, KD_out, RD_out, [](const set<Definition<Context>>& a, const iset<Definition<Context>>& b)->set<Definition<Context>> {
					auto res = a;
					for(auto cur : b) {
						res.erase(cur);
					}
					return res;
				}));

				// done
				return;

			}


			// use default treatment
			super::visitCallExpr(call, ctxt, loc, constraints);


		}

	};

	// forward definition
	namespace detail {
		template<typename Context, typename TGValue, typename ThreadOutAnalysisType, typename DataValue, typename ... ExtraParams>
		ConstraintPtr parallelMerge(CBA& cba, const ThreadOutAnalysisType& out, const TypedVariable<TGValue>& threadGroup, const TypedVariable<DataValue>& in_state, const TypedVariable<DataValue>& out_state, const ExtraParams& ... params);
	}


	template<typename Context>
	class ReachingDefsMergeConstraintGenerator
		: public BasicOutConstraintGenerator<
			  reaching_defs_in_analysis,
			  reaching_defs_tmp_analysis,
			  reaching_defs_merge_analysis,
			  ReachingDefsMergeConstraintGenerator<Context>,
			  Context,
			  Location<Context>
		  > {

		typedef BasicOutConstraintGenerator<
				  reaching_defs_in_analysis,
				  reaching_defs_tmp_analysis,
				  reaching_defs_merge_analysis,
				  ReachingDefsMergeConstraintGenerator<Context>,
				  Context,
				  Location<Context>
			 > super;


		CBA& cba;

	public:

		ReachingDefsMergeConstraintGenerator(CBA& cba) : super(cba, RDin, RDtmp, RDmerge), cba(cba) {}

		void visitCallExpr(const CallExprInstance& call, const Context& ctxt, const Location<Context>& loc, Constraints& constraints) {
			const auto& base = call->getNodeManager().getLangBasic();

			// one special case: assignments
			auto fun = call.as<CallExprPtr>()->getFunctionExpr();

			// other special case: merge
			if (base.isMerge(fun)) {

				// In this case we have utilize the base implementation with a minor modification

				// get involved sets
				auto RD_tmp = cba.getVar(RDtmp, call, ctxt, loc);
				auto RD_merge = cba.getVar(RDmerge, call, ctxt, loc);

				auto tg = cba.getVar(ThreadGroups, call[0], ctxt);

				// add constraint									v---v   this is the change, by default it would be RDmerge again
				constraints.add(detail::parallelMerge<Context>(cba, RDout, tg, RD_tmp, RD_merge, loc));

				// done
				return;
			}


			// use default treatment
			super::visitCallExpr(call, ctxt, loc, constraints);

		}
	};

} // end namespace cba
} // end namespace analysis
} // end namespace insieme
