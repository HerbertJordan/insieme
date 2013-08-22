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

#include "insieme/analysis/cba/framework/set_type.h"

namespace insieme {
namespace analysis {
namespace cba {

	// -------------------- Context Constraints ------------

	template<typename C> class ContextPredecessorResolver;
	typedef TypedSetType<Label,ContextPredecessorResolver> ContextPredecessorType;

	const ContextPredecessorType& pred() {
		static const ContextPredecessorType instance("pred");
		return instance;
	}


	// -------------------------------------- Context Predecessor Constraints -----------------------------

	template<typename Context>
	class ContextPredecessorResolver : public ConstraintResolver<Context> {


	public:

		ContextPredecessorResolver(CBA& cba) : ConstraintResolver<Context>(cba) {}


		void visitCallExpr(const CallExprAddress& call, const Context& ctxt, Constraints& constraints) {
			// restrict context
			assert_true(ctxt == Context()) << "This resolver only operates on the default context - given: " << ctxt;

			// check that call is a dynamic call
			auto funType = call->getFunctionExpr()->getNodeType();
			assert(funType != NT_LambdaExpr && funType != NT_BindExpr);
			if (funType == NT_LambdaExpr || funType == NT_BindExpr) return;		// not interested

			// fill predecessor set
			auto pred_res = context.getSet(pred, context.getLabel(call));

			// get surrounding free function
			auto fun = getSurroundingFreeFunction(call);

			// check whether there is a surrounding free function
			if (!fun) {
				// => this function can only reached statically
				constraints.add(elem(0, pred_res));
				return;
			}

			// ----- check whether function forwarding can be traced statically ----

			if (auto staticUses = context.getAllStaticUses(fun)) {
				// if uses can be determined statically, we can just consider them
				for(Label l_call : *staticUses) {
					constraints.add(elem(l_call, pred_res));
				}
				return;
			}

			// ----- fallback, the function might reach any point in the code -----

			// uses have to be determined dynamically
			vector<Context::CallContext> callContexts;
			generateSequences(context.getDynamicCallLabels(), callContexts);

			// compute all contexts this function may be called at
			auto num_params = fun->getType().as<FunctionTypePtr>()->getParameterTypes().size();
			for(const auto& dynCall : context.getDynamicCalls()) {
				// check number of parameters
				if (dynCall.size() != num_params) continue;

				auto l_call = context.getLabel(dynCall);
				auto l_fun = context.getLabel(dynCall->getFunctionExpr());

				for(const auto& callCtxt : callContexts) {
					auto F_dynCall = context.getSet(F, l_fun, callCtxt);
					constraints.add(elemIf(fun.as<ExpressionAddress>(), F_dynCall, l_call, pred_res));
				}
			}
		}

	};

} // end namespace cba
} // end namespace analysis
} // end namespace insieme
