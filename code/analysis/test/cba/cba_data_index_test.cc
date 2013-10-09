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

#include "insieme/analysis/cba/framework/data_index.h"

#include "insieme/core/ir.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"

#include "insieme/utils/string_utils.h"

namespace insieme {
namespace analysis {
namespace cba {


	namespace {

		using namespace core;

		template<typename T>
		void testIndexTypeConcepts() {

			// the type has to be default-constructable
			T a;

			// also from a formula
			NodeManager mgr;
			IRBuilder builder(mgr);

			// .. a linear one
			arithmetic::Formula f1 = arithmetic::toFormula(builder.parseExpr("12"));
			EXPECT_TRUE(f1.isInteger());
			T b(f1);

			// .. as well as a more complex one
			arithmetic::Formula f2 = arithmetic::toFormula(builder.parseExpr("lit(\"a\":int<4>)"));
			EXPECT_FALSE(f2.isInteger());
			T c(f2);

			// instances need to be comparable
			a == b;
			a < b;
		}

	}


	TEST(CBA, UnitIndex) {

		testIndexTypeConcepts<UnitIndex>();

		// just a few basic tests
		UnitIndex a;
		UnitIndex b;

		// copy-constructible
		UnitIndex c = a;
		c = b;	// and assignable

		EXPECT_EQ("unit", toString(a));
		EXPECT_EQ(a,b);

	}

	TEST(CBA, SingleIndex) {

		testIndexTypeConcepts<SingleIndex>();

		// a few tests
		SingleIndex is;
		SingleIndex i1 = 1;
		SingleIndex i2 = 2;

		SingleIndex ix = i1;
		ix = i2;

		EXPECT_EQ("*", toString(is));
		EXPECT_EQ("1", toString(i1));
		EXPECT_EQ("2", toString(i2));
		EXPECT_EQ("2", toString(ix));

		EXPECT_LT(i1,i2);
		EXPECT_LT(i1,is);
		EXPECT_LT(i2,is);
	}


} // end namespace cba
} // end namespace analysis
} // end namespace insieme