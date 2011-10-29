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

#include <boost/lexical_cast.hpp>

#include "insieme/core/encoder/encoder.h"

#include "insieme/utils/container_utils.h"

#include "insieme/core/ast_node.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/checks/ir_checks.h"

namespace insieme {
namespace core {
namespace encoder {

TEST(Lists, TestBaseTypes) {

	NodeManager manager;
	IRBuilder builder(manager);
	const auto& basic = manager.basic;

	core::ExpressionPtr expr = toIR(manager, 12);

	EXPECT_EQ(manager.basic.getInt4(), expr->getType());
	EXPECT_EQ("12", toString(*expr));
	EXPECT_EQ(12, toValue<int>(expr));
	EXPECT_TRUE(isEncodingOf<int>(expr));
	EXPECT_FALSE(isEncodingOf<short>(expr));

	expr = toIR(manager, false);
	EXPECT_EQ(manager.basic.getBool(), expr->getType());
	EXPECT_EQ("0", toString(*expr));
	EXPECT_FALSE(toValue<bool>(expr));

	EXPECT_TRUE(isEncodingOf<bool>(expr));
	EXPECT_FALSE(isEncodingOf<int>(expr));
	EXPECT_FALSE(isEncodingOf<short>(expr));


	expr = toIR(manager, (short)4);
	EXPECT_EQ("int<2>", toString(*expr->getType()));
	EXPECT_EQ("4", toString(*expr));

	expr = toIR(manager, (unsigned)14);
	EXPECT_EQ("uint<4>", toString(*expr->getType()));
	EXPECT_EQ("14", toString(*expr));


	double x = 123.123;
	expr = toIR(manager, x);
	EXPECT_EQ("real<8>", toString(*expr->getType()));
	EXPECT_EQ("123.123", toString(*expr));
	EXPECT_EQ(x, toValue<double>(expr));


	// check exceptions
	expr = builder.variable(manager.basic.getInt4());
	EXPECT_THROW(toValue<double>(expr), InvalidExpression); // wrong type
	EXPECT_THROW(toValue<int>(expr), InvalidExpression); // wrong node type


	// check types
	EXPECT_EQ(basic.getInt4(), getTypeFor<int>(manager));
	EXPECT_EQ(basic.getDouble(), getTypeFor<double>(manager));

}

TEST(Lists, SubTypeSupport) {

	NodeManager manager;
	IRBuilder builder(manager);
	const auto& basic = manager.basic;

	core::TypePtr uint4 = basic.getUInt4();
	core::TypePtr uint8 = basic.getUInt8();

	core::ExpressionPtr exprA = builder.literal(uint4, "12");
	core::ExpressionPtr exprB = builder.literal(uint8, "14");

	EXPECT_EQ(uint4, getTypeFor<uint32_t>(manager));
	EXPECT_EQ(uint8, getTypeFor<uint64_t>(manager));

	EXPECT_TRUE(isEncodingOf<uint32_t>(exprA));
	EXPECT_FALSE(isEncodingOf<uint32_t>(exprB));
	EXPECT_TRUE(isEncodingOf<uint64_t>(exprA));
	EXPECT_TRUE(isEncodingOf<uint64_t>(exprB));

	EXPECT_EQ(12, toValue<uint32_t>(exprA));
	EXPECT_EQ(12, toValue<uint64_t>(exprA));
	EXPECT_EQ(14, toValue<uint64_t>(exprB));
}



} // end namespace lists
} // end namespace core
} // end namespace insieme

