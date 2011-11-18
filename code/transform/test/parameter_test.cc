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

#include "insieme/transform/parameter.h"
#include "insieme/utils/string_utils.h"
#include "insieme/utils/test/test_utils.h"

namespace insieme {
namespace transform {
namespace parameter {


	TEST(Parameter, Composition) {

		const ParameterPtr single = atom<int>();
		EXPECT_TRUE(!!single);

		const ParameterPtr pair = tuple(atom<int>(), atom<string>());
		EXPECT_TRUE(!!pair);

		const ParameterPtr empty = tuple();
		EXPECT_TRUE(!!empty);

		const ParameterPtr nested = tuple(atom<int>(), tuple(atom<string>()));
		EXPECT_TRUE(!!nested);

	}

	TEST(Parameter, Printer) {

		const ParameterPtr nested = tuple("A nested pair", atom<int>("param A"), tuple(atom<string>("param B")));
		const string info = toString(InfoPrinter(nested));
		EXPECT_PRED2(containsSubString, info, "param A");
		EXPECT_PRED2(containsSubString, info, "param B");

	}


	TEST(Values, Composition) {

		Value a = makeValue(12);
		EXPECT_EQ("12", toString(a));

		Value b = makeValue("hello");
		EXPECT_EQ("hello", toString(b));


		Value c = combineValues(a,b);
		EXPECT_EQ("[12,hello]", toString(c));


		Value d = combineValues(a, b, c, a);
		EXPECT_EQ("[12,hello,[12,hello],12]", toString(d));

	}

	TEST(Values, TypeCheck) {

		Value a = makeValue(12);
		EXPECT_EQ("12", toString(a));
		EXPECT_TRUE(atom<int>()->isValid(a));
		EXPECT_FALSE(atom<string>()->isValid(a));

		Value b = makeValue("hello");
		EXPECT_EQ("hello", toString(b));
		EXPECT_TRUE(atom<string>()->isValid(b));
		EXPECT_FALSE(atom<int>()->isValid(b));


		Value c = combineValues(a,b);
		EXPECT_EQ("[12,hello]", toString(c));
		EXPECT_TRUE(tuple(atom<int>(),atom<string>())->isValid(c));
		EXPECT_FALSE(atom<int>()->isValid(c));


		Value d = combineValues(a, b, c, a);
		EXPECT_EQ("[12,hello,[12,hello],12]", toString(d));
		EXPECT_TRUE(tuple(atom<int>(),atom<string>(),tuple(atom<int>(),atom<string>()), atom<int>())->isValid(d));
		EXPECT_FALSE(tuple(atom<int>(),atom<string>(),tuple(atom<string>(),atom<int>()), atom<int>())->isValid(d));
		EXPECT_FALSE(atom<int>()->isValid(d));

	}


	TEST(Values, ReadValue) {

		Value a = makeValue(12);
		EXPECT_EQ("12", toString(a));
		EXPECT_EQ(12, getValue<int>(a));

		Value b = makeValue("hello");
		EXPECT_EQ("hello", toString(b));
		EXPECT_EQ("hello", getValue<string>(b));

		Value c = combineValues(a,b);
		EXPECT_EQ("[12,hello]", toString(c));
		EXPECT_EQ(12, getValue<int>(c,0));
		EXPECT_EQ("hello", getValue<string>(c,1));

		Value d = combineValues(a, b, c, a);
		EXPECT_EQ("[12,hello,[12,hello],12]", toString(d));
		EXPECT_EQ(12, getValue<int>(d,0));
		EXPECT_EQ("hello", getValue<string>(d,1));
		EXPECT_EQ(c, getValue<Value>(d,2));
		EXPECT_EQ(12, getValue<int>(d,2,0));
		EXPECT_EQ("hello", getValue<string>(d,2,1));
		EXPECT_EQ(12, getValue<int>(d,3));

	}


} // end namespace parameter
} // end namespace transform
} // end namespace insieme


