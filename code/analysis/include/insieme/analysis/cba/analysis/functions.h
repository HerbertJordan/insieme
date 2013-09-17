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

#include "insieme/analysis/cba/framework/cba.h"
#include "insieme/analysis/cba/framework/generator/basic_data_flow.h"

#include "insieme/core/forward_decls.h"
#include "insieme/utils/printable.h"

namespace insieme {
namespace analysis {
namespace cba {

	// a light version only tracking functions, no context
	typedef core::ExpressionAddress ContextFreeCallable;
	template<typename C> class FunctionConstraintGenerator;

	typedef TypedSetType<ContextFreeCallable,FunctionConstraintGenerator> FunctionSetType;
	extern const FunctionSetType F;
	extern const FunctionSetType f;



	template<typename Context>
	class FunctionConstraintGenerator : public BasicDataFlowConstraintGenerator<ContextFreeCallable,FunctionSetType,Context> {

		typedef BasicDataFlowConstraintGenerator<ContextFreeCallable,FunctionSetType,Context> super;

		CBA& cba;

	public:

		FunctionConstraintGenerator(CBA& cba)
			: super(cba, F, f), cba(cba) { };

		void visitLiteral(const LiteralAddress& literal, const Context& ctxt, Constraints& constraints) {

			// and default handling
			super::visitLiteral(literal, ctxt, constraints);

			// only interested in functions ...
			if (!literal->getType().isa<FunctionTypePtr>()) return;

			// add constraint: literal \in C(lit)
			auto value = literal.as<ExpressionAddress>();
			auto l_lit = cba.getLabel(literal);

			auto F_lit = cba.getSet(F, l_lit, ctxt);
			constraints.add(elem(value, F_lit));

		}

		void visitVariable(const VariableAddress& var, const Context& ctxt, Constraints& constraints) {

			// special case - lambda-bindings
			if (!var.isRoot() && var.getParentNode().isa<LambdaBindingPtr>()) {

				auto value = var.getParentAddress(3).as<ExpressionAddress>();
				auto l_var = cba.getLabel(var);
				auto f_var = cba.getSet(f, l_var, ctxt);

				constraints.add(elem(value, f_var));

			} else {

				// default handling
				super::visitVariable(var, ctxt, constraints);

			}

		}

		void visitLambdaExpr(const LambdaExprAddress& lambda, const Context& ctxt, Constraints& constraints) {

			// and default handling
			super::visitLambdaExpr(lambda, ctxt, constraints);

			// add constraint: lambda \in C(lambda)
			auto value = lambda.as<ExpressionAddress>();
			auto label = cba.getLabel(lambda);

			constraints.add(elem(value, cba.getSet(F, label, ctxt)));

			// TODO: handle recursions

		}

		void visitBindExpr(const BindExprAddress& bind, const Context& ctxt, Constraints& constraints) {

			// and default handling
			super::visitBindExpr(bind, ctxt, constraints);

			// add constraint: bind \in C(bind)
			auto value = bind.as<ExpressionAddress>();
			auto label = cba.getLabel(bind);

			auto F_bind = cba.getSet(F, label, ctxt);
			constraints.add(elem(value, F_bind));

		}

	};

} // end namespace cba
} // end namespace analysis
} // end namespace insieme
