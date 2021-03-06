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

#include <gtest/gtest.h>

#include "insieme/core/ir_builder.h"

#include "insieme/analysis/features/code_feature_catalog.h"
#include "insieme/analysis/polyhedral/scopregion.h"

namespace insieme {
namespace analysis {
namespace features {

	using namespace core;

	TEST(ComposedFeatures, Basic) {
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto& basic = mgr.getLangBasic();

		std::map<std::string, NodePtr> symbols;
		symbols["v"] = builder.variable(builder.parseType("ref<vector<int<4>,100>>"));
		symbols["w"] = builder.variable(builder.parseType("ref<vector<int<4>,100>>"));

		// load some code sample ...
		auto forStmt = builder.parseStmt(
			"for(uint<4> i = 10u .. 50u : 1u) {"
			"	v[i] = *(w[i]);"
			"	for(uint<4> j = 5u .. 25u : 1u) {"
			"		if ( (j < 10u ) ) {"
			"			v[i+j]; v[i+j];"
			"		} else {"
			"			v[i-j]; v[i-j];"
			"		}"
			"	}"
			"}", symbols).as<ForStmtPtr>();

		FeaturePtr nativeArrayRefElem1D = createSimpleCodeFeature("NUM_getArrayRefElem1D", "number of getArrayRefElem1D",
				basic.getArrayRefElem1D(), FA_Weighted);
		FeaturePtr composedRefArrayAccesses = createComposedFeature("composed1DArrayAccesses", "simple testing a one composed feature with only one element",
				GEN_COMPOSING_FCT( return (component(0)); ),
				toVector(nativeArrayRefElem1D));

		FeaturePtr nativeArraySubscript1D = createSimpleCodeFeature("NUM_getArraySubscript1D", "number of getArraySubscript1D",
				basic.getArraySubscript1D(), FA_Weighted);

		FeaturePtr composedAnyArrayAccesses = createComposedFeature("composed1DArrayAccesses", "simple testing a one composed featrue with only one element",
				GEN_COMPOSING_FCT( return component(0) + component(1); ),
				toVector(nativeArrayRefElem1D, nativeArraySubscript1D));

		EXPECT_EQ( getValue<double>(nativeArrayRefElem1D->extractFrom(forStmt))
				 + getValue<double>(nativeArraySubscript1D->extractFrom(forStmt)),
				getValue<double>(composedAnyArrayAccesses->extractFrom(forStmt)));

		FeatureCatalog catalog;
		catalog.addAll(getFullCodeFeatureCatalog());

		double allOps = getValue<double>(catalog.getFeature("SCF_NUM_any_all_OPs_real")->extractFrom(forStmt));
		double allAccesses = getValue<double>(catalog.getFeature("SCF_IO_NUM_any_read/write_OPs_real")->extractFrom(forStmt));

 		EXPECT_EQ( allOps / allAccesses, getValue<double>(catalog.getFeature("SCF_COMP_scalarOPs-memoryAccess_real_ratio")->extractFrom(forStmt)));
	}

} // end namespace features
} // end namespace analysis
} // end namespace insieme
