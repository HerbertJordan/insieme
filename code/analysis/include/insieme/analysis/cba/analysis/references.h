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

#include "insieme/analysis/cba/framework/analysis_type.h"
#include "insieme/analysis/cba/framework/entities/location.h"
#include "insieme/analysis/cba/framework/generator/basic_data_flow.h"

#include "insieme/analysis/cba/analysis/data_paths.h"

#include "insieme/analysis/cba/utils/cba_utils.h"
#include "insieme/analysis/cba/utils/constraint_utils.h"

#include "insieme/core/forward_decls.h"
#include "insieme/core/ir_builder.h"
#include "insieme/utils/printable.h"

namespace insieme {
namespace analysis {
namespace cba {

	// forward declarations
	struct data_path_analysis_data;
	struct data_path_analysis_var;
	extern const data_path_analysis_data DP;
	extern const data_path_analysis_var  dp;

	// ----------------- references ---------------

	template<typename Context> class ReferenceConstraintGenerator;

	struct reference_analysis_data : public dependent_data_analysis<Reference, ReferenceConstraintGenerator> {};
	struct reference_analysis_var  : public dependent_data_analysis<Reference, ReferenceConstraintGenerator> {};

	extern const reference_analysis_data R;
	extern const reference_analysis_var  r;


	template<typename Context>
	class ReferenceConstraintGenerator : public DataFlowConstraintGenerator<reference_analysis_data, reference_analysis_var, Context> {

		typedef DataFlowConstraintGenerator<reference_analysis_data, reference_analysis_var, Context> super;

		CBA& cba;

		const core::lang::BasicGenerator& base;

	public:

		ReferenceConstraintGenerator(CBA& cba)
			: super(cba, R, r, getUnknownExternalReference(cba)),
			  cba(cba), base(cba.getRoot().getNodeManager().getLangBasic()) { };

		using super::elem;

		void visitLiteral(const LiteralInstance& literal, const Context& ctxt, Constraints& constraints) {

			// only interested in memory location constructors
			if (!isMemoryConstructor(literal)) {
				// default handling for those
				if (literal->getType().isa<RefTypePtr>()) {
					super::visitLiteral(literal, ctxt, constraints);
				}
				return;
			}

			// add constraint literal \in R(lit)
			auto value = getLocation(literal, ctxt);
			auto l_lit = cba.getLabel(literal);

			auto R_lit = cba.getVar(R, l_lit, ctxt);
			constraints.add(elem(value, R_lit));

		}

		void visitCallExpr(const CallExprInstance& call, const Context& ctxt, Constraints& constraints) {

			// introduce memory location in some cases
			if (isMemoryConstructor(call)) {

				// add constraint location \in R(call)
				auto value = getLocation(call, ctxt);
				auto l_call = cba.getLabel(call);

				auto R_call = cba.getVar(R, l_call, ctxt);
				constraints.add(elem(value, R_call));

				// done
				return;
			}

			// check whether the operation is a narrow or expand (data path operation)
			const auto& fun = call->getFunctionExpr();
			if (base.isRefNarrow(fun)) {

				// obtain involved sets
				auto R_in  = cba.getVar(R, call[0], ctxt);	// the input reference
				auto DP_in = cba.getVar(DP, call[1], ctxt);				// the data path values
				auto R_out = cba.getVar(R, call, ctxt);		// the resulting context

				// add constraint linking in and out values
				constraints.add(combine(this->getValueManager(), R_in, DP_in, R_out,
						[](const Reference<Context>& ref, const DataPath& path)->Reference<Context> {
							return Reference<Context>(ref.getLocation(), ref.getDataPath() << path);
						}
				));

			} else if (base.isRefExpand(fun)) {

				// obtain involved sets
				auto R_in  = cba.getVar(R, call[0], ctxt);	// the input reference
				auto DP_in = cba.getVar(DP, call[1], ctxt);				// the data path values
				auto R_out = cba.getVar(R, call, ctxt);		// the resulting context

				// add constraint linking in and out values
				constraints.add(combine(this->getValueManager(), R_in, DP_in, R_out,
						[](const Reference<Context>& ref, const DataPath& path)->Reference<Context> {
							return Reference<Context>(ref.getLocation(), ref.getDataPath() >> path);
						}
				));

			} else if (base.isRefReinterpret(fun)) {

				// re-interpret casts are ignored ... does not alter the reference
				// obtain involved sets
				auto R_in  = cba.getVar(R, call[0], ctxt);	// the input reference
				auto R_out = cba.getVar(R, call, ctxt);		// the resulting context

				// add constraint linking in and out values
				constraints.add(subset(R_in, R_out));

			} else {
				// use default handling
				super::visitCallExpr(call, ctxt, constraints);
			}
		}

	private:

		static typename super::value_type getUnknownExternalReference(CBA& cba) {

			auto& mgr = cba.getRoot()->getNodeManager();
			IRBuilder builder(mgr);
			auto type = mgr.getLangBasic().getAnyRef();

			set<Reference<Context>> res;
			res.insert(Reference<Context>(Location<Context>(ExpressionInstance(builder.literal("__artificial_ext_ref_A", type)))));
			res.insert(Reference<Context>(Location<Context>(ExpressionInstance(builder.literal("__artificial_ext_ref_B", type)))));
			return cba.getDataManager<typename lattice<reference_analysis_data, analysis_config<Context>>::type>().atomic(res);
		}

	};

} // end namespace cba
} // end namespace analysis
} // end namespace insieme
