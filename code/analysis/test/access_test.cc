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
#include <limits>
#include <algorithm>

#include "insieme/analysis/access.h"

#include "insieme/core/ir_program.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_statements.h"

#include "insieme/core/parser/ir_parse.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/analysis/polyhedral/scop.h"
#include "insieme/analysis/polyhedral/backends/isl_backend.h"

using namespace insieme;
using namespace insieme::core;
using namespace insieme::analysis;

TEST(Access, Scalars) {

	NodeManager mgr;
	parse::IRParser parser(mgr);
	IRBuilder builder(mgr);

	{
		auto code = parser.parseExpression("ref<int<4>>:a");

		auto access = getImmediateAccess( ExpressionAddress(code) );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::SCALAR, access.getType());
		EXPECT_TRUE(access.isRef());
	}

	{
		auto code = parser.parseExpression("int<4>:a");

		auto access = getImmediateAccess( ExpressionAddress(code) );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::SCALAR, access.getType());
		EXPECT_FALSE(access.isRef());
	}

	{
		auto code = parser.parseExpression("(op<ref.deref>(ref<int<4>>:a))");

		auto access = getImmediateAccess( ExpressionAddress(code) );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::SCALAR, access.getType());
		EXPECT_FALSE(access.isRef());
	}


	{
		auto code = parser.parseExpression("ref<struct<a:int<4>,b:int<4>>>:s");

		auto access = getImmediateAccess( ExpressionAddress(code) );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::SCALAR, access.getType());
		EXPECT_TRUE(access.isRef());
	}

}

TEST(Access, MemberAccess) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);
	IRBuilder builder(mgr);

	{
		auto code = parser.parseExpression(
			"(op<composite.ref.elem>(ref<struct<a:int<4>, b:int<4>>>:s, lit<identifier,a>, lit<type<int<4>>, int<4>))"
		);

		auto access = getImmediateAccess(ExpressionAddress(code));
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::MEMBER, access.getType());
		EXPECT_EQ(1u, access.getAccessedVariable()->getId());
		EXPECT_TRUE(access.isRef());
	}

	{
		auto code = parser.parseExpression(
			"(op<composite.member.access>(struct<a:int<4>, b:int<4>>:s, lit<identifier,a>, lit<type<int<4>>, int<4>))"
		);
		
		auto access = getImmediateAccess(ExpressionAddress(code));
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::MEMBER, access.getType());
		EXPECT_EQ(2u, access.getAccessedVariable()->getId());
		EXPECT_FALSE(access.isRef());
	}

}

// Wait for new parser 
TEST(Access, ArrayAccess) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);
	IRBuilder builder(mgr);

	{
		auto code = parser.parseExpression(
			"(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, 2))"
		);
		// std::cout << code << " " << *code->getType() << std::endl;
		auto access = getImmediateAccess( ExpressionAddress(code) );
		//std::cout << access << std::endl;
		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_FALSE(access.isRef());
		EXPECT_EQ("(v4294967295 + -2 == 0)", toString(*access.getAccessedRange()));
		EXPECT_FALSE(access.getContext());
		EXPECT_FALSE(access.isContextDependent());
	}

	{
		auto code = parser.parseExpression(
			"(op<array.subscript.1D>(array<int<4>,1>:v, 2))"
		);
		auto access = getImmediateAccess( ExpressionAddress(code) );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_FALSE(access.isRef());
		EXPECT_EQ("(v4294967295 + -2 == 0)", toString(*access.getAccessedRange()));
		EXPECT_FALSE(access.getContext());
		EXPECT_FALSE(access.isContextDependent());
	}

	{
		auto code = parser.parseExpression(
			"(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, (lit<uint<4>,3> - lit<uint<4>,1>)))"
		);

		// std::cout << code << " " << *code->getType() << std::endl;
		auto access = getImmediateAccess( ExpressionAddress(code) );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_TRUE(access.isRef());
		EXPECT_EQ("(v4294967295 + -2 == 0)", toString(*access.getAccessedRange()));
		EXPECT_FALSE(access.getContext());
		EXPECT_FALSE(access.isContextDependent());
	}

	{
		auto code = parser.parseExpression(
			"(op<vector.subscript>(vector<int<4>,8>:v, 2))"
		);
		auto access = getImmediateAccess( ExpressionAddress(code) );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_FALSE(access.isRef());
		EXPECT_EQ("(v4294967295 + -2 == 0)", toString(*access.getAccessedRange()));
		EXPECT_FALSE(access.getContext());
		EXPECT_FALSE(access.isContextDependent());
	}

	{
		auto code = parser.parseExpression(
			"(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, uint<4>:b))"
		);

		auto access = getImmediateAccess( ExpressionAddress(code) );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_TRUE(access.isRef());
		// std::cout << access.getConstraint() << std::endl;
	}

	{
		auto code = parser.parseStatement(
			"if( (uint<4>:b>10) )"
			"	(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, b))"
		);

		// perform the polyhedral analysis 
		polyhedral::scop::mark(code);

		auto access = getImmediateAccess( StatementAddress(code).as<IfStmtAddress>()->getThenBody()->getStatement(0).
										  as<ExpressionAddress>() );
		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_TRUE(access.isRef());
		// EXPECT_TRUE(!!access.getConstraint());
		EXPECT_EQ("((v7 + -11 >= 0) ^ (v4294967295 + -v7 == 0))", toString(*access.getAccessedRange()));

		EXPECT_EQ(code, access.getContext().getAddressedNode()); 
		EXPECT_TRUE(access.isContextDependent());
	}

	{
		auto code = parser.parseStatement(
			"if( ((uint<4>:b>10) && (uint<4>:a<20)) ) {"
			"	decl uint<4>:c = (b+a); "
			"	(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, c));"
			"}"
		);

		// perform the polyhedral analysis 
		polyhedral::scop::mark(code);

		DeclarationStmtAddress decl = StatementAddress(code).
				as<IfStmtAddress>()->getThenBody()->getStatement(0).as<DeclarationStmtAddress>();
		// Create an alias for the expression c = b+a;
		TmpVarMap map;
		map.storeTmpVar( decl->getInitialization(), decl->getVariable().getAddressedNode() );

		auto access = getImmediateAccess( StatementAddress(code).
				as<IfStmtAddress>()->getThenBody()->getStatement(1).as<ExpressionAddress>(), map );

		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_TRUE(access.isRef());
		EXPECT_FALSE(access.isContextDependent());
	}

	{
		auto code = parser.parseStatement(
			"if( ((uint<4>:b>10) && (uint<4>:a<20)) ) {"
			"	(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, (a*b)));"
			"}"
		);

		// perform the polyhedral analysis 
		polyhedral::scop::mark(code);

		auto access = getImmediateAccess( StatementAddress(code).as<IfStmtAddress>()->getThenBody()->getStatement(0).
										  as<ExpressionAddress>());

		// std::cout << access << std::endl;
		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_TRUE(access.isRef());
		EXPECT_FALSE( access.getContext() ); 
		EXPECT_FALSE(access.isContextDependent());
	}

	// not affine access => invalid scop
	{
		auto code = parser.parseStatement(
			"if( ((uint<4>:b>10) && (uint<4>:a<20)) ) {"
			"  (op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, (a+b)));"
			"}"
		);

		// perform the polyhedral analysis 
		auto scop = polyhedral::scop::mark(code);
		
		auto access = getImmediateAccess( 
				StatementAddress(code).as<IfStmtAddress>()->getThenBody()->getStatement(0). as<ExpressionAddress>()
			);

		EXPECT_EQ(VarType::ARRAY, access.getType());
		EXPECT_TRUE(access.isRef());
		EXPECT_TRUE(access.getContext());
		EXPECT_TRUE(access.isContextDependent());

		EXPECT_EQ("(((-v21 + 19 >= 0) ^ (v20 + -11 >= 0)) ^ (v4294967295 + -v20 + -v21 == 0))", 
				  toString(*access.getAccessedRange())
				 );

		auto ctx = polyhedral::makeCtx();
		auto set = polyhedral::makeSet(ctx, polyhedral::IterationDomain(access.getAccessedRange()));

		EXPECT_EQ("[v20, v21] -> { [v20 + v21] : v21 <= 19 and v20 >= 11 }", toString(*set));
	}

}

TEST(Access, SameAccess) {

	NodeManager mgr;
	parse::IRParser parser(mgr);
	IRBuilder builder(mgr);

	{
		auto code = parser.parseStatement(
			"for(decl uint<4>:i = 0 .. 10 : 1) { " \
				"(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, i));"
				"(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, i));"
			"}"
		);

		// perform the polyhedral analysis 
		auto scop = polyhedral::scop::mark(code);
		
		auto access1 = getImmediateAccess( 
				StatementAddress(code).as<ForStmtAddress>()->getBody()->getStatement(0).as<ExpressionAddress>() 
			);
		auto access2 = getImmediateAccess( 
				StatementAddress(code).as<ForStmtAddress>()->getBody()->getStatement(1).as<ExpressionAddress>() 
			);

		EXPECT_TRUE(access1.getContext());
		EXPECT_FALSE(access1.isContextDependent());
		EXPECT_EQ(access1.getContext(), code);
		EXPECT_EQ("(((-v1 + 9 >= 0) ^ (v1 >= 0)) ^ (-v1 + v4294967295 == 0))", toString(*access1.getAccessedRange()));

		EXPECT_TRUE(access2.getContext());
		EXPECT_FALSE(access2.isContextDependent());
		EXPECT_EQ(access2.getContext(), code);
		EXPECT_EQ("(((-v1 + 9 >= 0) ^ (v1 >= 0)) ^ (-v1 + v4294967295 == 0))", toString(*access2.getAccessedRange()));

	}
}

TEST(Access, DifferentAccess) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);
	IRBuilder builder(mgr);

	{
		auto code = parser.parseStatement(
			"for(decl uint<4>:i = 0 .. 10 : 1) { " \
				"(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, i));"
				"(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, (i+1)));"
			"}"
		);

		// perform the polyhedral analysis 
		auto scop = polyhedral::scop::mark(code);
		
		auto access1 = getImmediateAccess( 
				StatementAddress(code).as<ForStmtAddress>()->getBody()->getStatement(0).as<ExpressionAddress>() 
			);
		auto access2 = getImmediateAccess( 
				StatementAddress(code).as<ForStmtAddress>()->getBody()->getStatement(1).as<ExpressionAddress>() 
			);
		
		EXPECT_TRUE(access1.getContext());
		EXPECT_FALSE(access1.isContextDependent());
		EXPECT_EQ(access1.getContext(), code);
		EXPECT_EQ("(((-v1 + 9 >= 0) ^ (v1 >= 0)) ^ (-v1 + v4294967295 == 0))", toString(*access1.getAccessedRange()));


		EXPECT_TRUE(access2.getContext());
		EXPECT_FALSE(access2.isContextDependent());
		EXPECT_EQ(access2.getContext(), code);
		EXPECT_EQ("(((-v1 + 9 >= 0) ^ (v1 >= 0)) ^ (-v1 + v4294967295 + -1 == 0))", toString(*access2.getAccessedRange()));

		auto ctx = polyhedral::makeCtx();
		auto set1 = polyhedral::makeSet(ctx, polyhedral::IterationDomain(access1.getAccessedRange()));

		EXPECT_EQ("{ [v4294967295] : v4294967295 <= 9 and v4294967295 >= 0 }", toString(*set1));
	}
}

TEST(Access, CommonSubset) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);
	IRBuilder builder(mgr);

	{
		auto code = parser.parseStatement(
			"{"
			"	for(decl uint<4>:i1 = 1 .. 11 : 1) { " \
			"		(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, i1));"
			"	};"
			"	for(decl uint<4>:i2 = 0 .. 10 : 1) { " \
			"		(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, (i2+1)));"
			"	};"
			"}"
		);

		// perform the polyhedral analysis 
		auto scop = polyhedral::scop::mark(code);
		
		auto access1 = getImmediateAccess( 
				StatementAddress(code).as<CompoundStmtAddress>()->getStatement(0).
									   as<ForStmtAddress>()->getBody()->getStatement(0).as<ExpressionAddress>() 
			);
		auto access2 = getImmediateAccess( 
				StatementAddress(code).as<CompoundStmtAddress>()->getStatement(1).
									   as<ForStmtAddress>()->getBody()->getStatement(0).as<ExpressionAddress>() 
			);
		
		EXPECT_TRUE(access1.getContext());
		EXPECT_FALSE(access1.isContextDependent());
		EXPECT_EQ(access1.getContext(), code);
		EXPECT_EQ("(((-v1 + 10 >= 0) ^ (v1 + -1 >= 0)) ^ (-v1 + v4294967295 == 0))", toString(*access1.getAccessedRange()));

		EXPECT_TRUE(access2.getContext());
		EXPECT_FALSE(access2.isContextDependent());
		EXPECT_EQ(access2.getContext(), code);
		EXPECT_EQ("(((-v3 + 9 >= 0) ^ (v3 >= 0)) ^ (-v3 + v4294967295 + -1 == 0))", toString(*access2.getAccessedRange()));

		auto ctx = polyhedral::makeCtx();
		auto set1 = polyhedral::makeSet(ctx, polyhedral::IterationDomain(access1.getAccessedRange()));
		auto set2 = polyhedral::makeSet(ctx, polyhedral::IterationDomain(access2.getAccessedRange()));

		EXPECT_EQ("{ [v4294967295] : v4294967295 <= 10 and v4294967295 >= 1 }", toString(*set1));
		EXPECT_EQ("{ [v4294967295] : v4294967295 <= 10 and v4294967295 >= 1 }", toString(*set2));

		EXPECT_EQ("{ [v4294967295] : v4294967295 <= 10 and v4294967295 >= 1 }", toString(*(set1 * set2)));

		// the two sets are equal
		EXPECT_EQ(*set1, *set2);
		EXPECT_FALSE((set1*set2)->empty());
		EXPECT_EQ(*set1, *(set1*set2));

		// Set difference is empty 
		EXPECT_TRUE((set1-set2)->empty());
	}
}

TEST(Access, EmptySubset) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);
	IRBuilder builder(mgr);

	{
		auto code = parser.parseStatement(
			"{"
			"	for(decl uint<4>:i1 = 1 .. 5 : 1) { " \
			"		(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, i1));"
			"	};"
			"	for(decl uint<4>:i2 = 4 .. 9 : 1) { " \
			"		(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, (i2+1)));"
			"	};"
			"}"
		);

		// perform the polyhedral analysis 
		auto scop = polyhedral::scop::mark(code);
		
		auto access1 = getImmediateAccess( 
				StatementAddress(code).as<CompoundStmtAddress>()->getStatement(0).
									   as<ForStmtAddress>()->getBody()->getStatement(0).as<ExpressionAddress>() 
			);
		auto access2 = getImmediateAccess( 
				StatementAddress(code).as<CompoundStmtAddress>()->getStatement(1).
									   as<ForStmtAddress>()->getBody()->getStatement(0).as<ExpressionAddress>() 
			);
		
		EXPECT_TRUE(access1.getContext());
		EXPECT_FALSE(access1.isContextDependent());
		EXPECT_EQ(access1.getContext(), code);
		EXPECT_EQ("(((-v1 + 4 >= 0) ^ (v1 + -1 >= 0)) ^ (-v1 + v4294967295 == 0))", toString(*access1.getAccessedRange()));

		EXPECT_TRUE(access2.getContext());
		EXPECT_FALSE(access2.isContextDependent());
		EXPECT_EQ(access2.getContext(), code);
		EXPECT_EQ("(((-v3 + 8 >= 0) ^ (v3 + -4 >= 0)) ^ (-v3 + v4294967295 + -1 == 0))", toString(*access2.getAccessedRange()));

		auto ctx = polyhedral::makeCtx();
		auto set1 = polyhedral::makeSet(ctx, polyhedral::IterationDomain(access1.getAccessedRange()));
		auto set2 = polyhedral::makeSet(ctx, polyhedral::IterationDomain(access2.getAccessedRange()));

		EXPECT_EQ("{ [v4294967295] : v4294967295 <= 4 and v4294967295 >= 1 }", toString(*set1));
		EXPECT_EQ("{ [v4294967295] : v4294967295 <= 9 and v4294967295 >= 5 }", toString(*set2));

		EXPECT_TRUE((set1 * set2)->empty());
	}
}

TEST(Access, StridedSubset) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);
	IRBuilder builder(mgr);

	{
		auto code = parser.parseStatement(
			"{"
			"	for(decl uint<4>:i1 = 1 .. 5 : 2) { " 
			"		(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, i1));"
			"	};"
			"	for(decl uint<4>:i2 = 1 .. 9 : 2) { " 
			"		(op<vector.ref.elem>(ref<vector<int<4>,4>>:v, (i2+1)));"
			"	};"
			"}"
		);

		// perform the polyhedral analysis 
		auto scop = polyhedral::scop::mark(code);
		
		auto access1 = getImmediateAccess( 
				StatementAddress(code).as<CompoundStmtAddress>()->getStatement(0).
									   as<ForStmtAddress>()->getBody()->getStatement(0).as<ExpressionAddress>() 
			);
		auto access2 = getImmediateAccess( 
				StatementAddress(code).as<CompoundStmtAddress>()->getStatement(1).
									   as<ForStmtAddress>()->getBody()->getStatement(0).as<ExpressionAddress>() 
			);
		
		EXPECT_TRUE(access1.getContext());
		EXPECT_FALSE(access1.isContextDependent());
		EXPECT_EQ(access1.getContext(), code);
		EXPECT_EQ("((((-v1 + 4 >= 0) ^ (v1 + -2*v4 + -1 == 0)) ^ (v1 + -1 >= 0)) ^ (-v1 + v4294967295 == 0))", 
				toString(*access1.getAccessedRange()));

		EXPECT_TRUE(access2.getContext());
		EXPECT_FALSE(access2.isContextDependent());
		EXPECT_EQ(access2.getContext(), code);
		EXPECT_EQ("((((-v3 + 8 >= 0) ^ (v3 + -2*v5 + -1 == 0)) ^ (v3 + -1 >= 0)) ^ (-v3 + v4294967295 + -1 == 0))", 
				toString(*access2.getAccessedRange()));

		auto ctx = polyhedral::makeCtx();
		auto set1 = polyhedral::makeSet(ctx, polyhedral::IterationDomain(access1.getAccessedRange()));
		auto set2 = polyhedral::makeSet(ctx, polyhedral::IterationDomain(access2.getAccessedRange()));

		EXPECT_EQ("{ [v4294967295] : exists (e0 = [(-1 + v4294967295)/2]: 2e0 = -1 + v4294967295 and v4294967295 <= 3 and v4294967295 >= 1) }", 
				toString(*set1));
		EXPECT_EQ("{ [v4294967295] : exists (e0 = [(v4294967295)/2]: 2e0 = v4294967295 and v4294967295 <= 8 and v4294967295 >= 2) }", 
				toString(*set2));

		EXPECT_TRUE((set1 * set2)->empty());
	}
}

