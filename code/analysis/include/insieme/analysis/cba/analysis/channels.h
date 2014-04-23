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

#include "insieme/analysis/cba/framework/analysis_type.h"
#include "insieme/analysis/cba/framework/entities/channel.h"
#include "insieme/analysis/cba/framework/generator/basic_data_flow.h"

#include "insieme/analysis/cba/utils/cba_utils.h"
#include "insieme/analysis/cba/utils/constraint_utils.h"

#include "insieme/core/forward_decls.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/utils/printable.h"

namespace insieme {
namespace analysis {
namespace cba {

	// ----------------- channels ---------------

	template<typename Context> class ChannelConstraintGenerator;

	struct channel_analysis_data : public dependent_data_analysis<Channel, ChannelConstraintGenerator> {};
	struct channel_analysis_var  : public dependent_data_analysis<Channel, ChannelConstraintGenerator> {};

	extern const channel_analysis_data Ch;
	extern const channel_analysis_var  ch;


	template<typename Context>
	class ChannelConstraintGenerator : public DataFlowConstraintGenerator<channel_analysis_data, channel_analysis_var, Context> {

		typedef DataFlowConstraintGenerator<channel_analysis_data, channel_analysis_var, Context> super;

		CBA& cba;

		const core::lang::BasicGenerator& base;

	public:

		ChannelConstraintGenerator(CBA& cba) : super(cba, Ch, ch), cba(cba), base(cba.getRoot().getNodeManager().getLangBasic()) { };

		using super::elem;

		void visitLiteral(const LiteralInstance& literal, const Context& ctxt, Constraints& constraints) {

			// and default handling
			super::visitLiteral(literal, ctxt, constraints);

			// only interested in memory location constructors
			if (!isChannelConstructor(literal)) return;

			// add constraint literal \in R(lit)
			auto value = getChannelFromConstructor(literal, ctxt);
			auto l_lit = cba.getLabel(literal);

			auto C_lit = cba.getSet(Ch, l_lit, ctxt);
			constraints.add(elem(value, C_lit));

		}

		void visitCallExpr(const CallExprInstance& call, const Context& ctxt, Constraints& constraints) {

			// and default handling
			super::visitCallExpr(call, ctxt, constraints);

			// introduce channels in case the constructor is called
			if (!isChannelConstructor(call)) return;

			// add constraint location \in R(call)
			auto value = getChannelFromConstructor(call, ctxt);
			auto l_lit = cba.getLabel(call);

			auto C_lit = cba.getSet(Ch, l_lit, ctxt);
			constraints.add(elem(value, C_lit));

		}

	};

} // end namespace cba
} // end namespace analysis
} // end namespace insieme
