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


#include "insieme/analysis/cba/cba.h"

#include "insieme/analysis/cba/framework/cba.h"

#include "insieme/analysis/cba/analysis/arithmetic.h"
#include "insieme/analysis/cba/analysis/boolean.h"
#include "insieme/analysis/cba/analysis/simple_constant.h"
#include "insieme/analysis/cba/analysis/callables.h"
#include "insieme/analysis/cba/analysis/call_context.h"
#include "insieme/analysis/cba/analysis/reachability.h"
#include "insieme/analysis/cba/analysis/data_paths.h"

#include "insieme/core/ir_builder.h"
#include "insieme/core/checks/full_check.h"

#include "insieme/utils/timer.h"
#include "insieme/utils/test/test_utils.h"

#include "cba_test.inc.h"

namespace insieme {
namespace analysis {
namespace cba {

	using namespace core;

	TEST(CBA, Locations) {
		NodeManager mgr;
		IRBuilder builder(mgr);


	}

	TEST(CBA, SimpleStruct) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	let point = struct { int a; int b; };"
				"	"
				"	decl point p1 = struct point { 1, 2 };"
				"	decl point p2 = struct point { 3, 4 };"
				"	"
				"	p1;"
				"	p2;"
				"	p1.a;"
				"	p1.b;"
				"	p2.a;"
				"	p2.b;"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("[a={1},b={2}]", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={3},b={4}]", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), A)));

	}

	TEST(CBA, NestedStruct) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	let point = struct { int x; int y; };"
				"	let cycle = struct { point center; int r; };"
				"	"
				"	decl cycle c1 = struct cycle { struct point { 1, 2 } , 3 };"
				"	"
				"	c1;"
				"	c1.center;"
				"	c1.center.x;"
				"	c1.center.y;"
				"	c1.r;"
				"	"
				"	decl point p1 = c1.center;"
				"	p1;"
				"	p1.x;"
				"	p1.y;"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("[center=[x={1},y={2}],r={3}]", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[x={1},y={2}]", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), A)));

		EXPECT_EQ("[x={1},y={2}]", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[8].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[9].as<ExpressionAddress>(), A)));

	}

	TEST(CBA, DataPaths) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(R"(
				{
					decl auto a = dp_root;
					decl auto b = dp_member(a, lit("a")); 
					
					a;
					b;
					
					dp_element(b, 17u+4u);
					dp_element(b, 17u+4u+lit("c":uint<4>));
					
					decl ref<datapath> x = var(dp_root);
					if (lit("?":bool)) {					// to bring in some confusion
						x = dp_member(*x, lit("a")); 
					}
					*x;
					dp_member(*x, lit("b"));
				}
		)").as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{#}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), DP)));
		EXPECT_EQ("{#.a}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), DP)));
		EXPECT_EQ("{#.a.21}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), DP)));
		EXPECT_EQ("{#.a.*}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), DP)));

		EXPECT_EQ("{#,#.a}", toString(analysis.getValuesOf(code[8].as<ExpressionAddress>(), DP)));

		EXPECT_TRUE(
				"{#.a.b,#.b}" == toString(analysis.getValuesOf(code[9].as<ExpressionAddress>(), DP)) ||
				"{#.b,#.a.b}" == toString(analysis.getValuesOf(code[9].as<ExpressionAddress>(), DP))
		);

//		createDotDump(analysis);
	}

	TEST(CBA, TowardMutableStructs) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	let point = struct { int a; int b; };"
				"	let cycle = struct { point c; int r; };"
				"	"
				"	decl ref<int> a = var(4);"
				"	decl ref<point> p = var(struct point { 2, 3 });"
				"	decl ref<cycle> c = var(struct cycle { *p, 4 });"
				"	"
				"	a;"
				"	p;"
				"	c;"
				"	"
				"	p.a;"
				"	p.b;"
				"	"
				"	c.c;"
				"	c.c.a;"
				"	c.c.b;"
				"	c.r;"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{(0-0-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#)}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), R)));
		EXPECT_EQ("{(0-1-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#)}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), R)));
		EXPECT_EQ("{(0-2-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#)}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), R)));

		EXPECT_EQ("{(0-1-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#.a)}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), R)));
		EXPECT_EQ("{(0-1-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#.b)}", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), R)));

		EXPECT_EQ("{(0-2-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#.c)}", toString(analysis.getValuesOf(code[8].as<ExpressionAddress>(), R)));
		EXPECT_EQ("{(0-2-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#.c.a)}", toString(analysis.getValuesOf(code[9].as<ExpressionAddress>(), R)));
		EXPECT_EQ("{(0-2-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#.c.b)}", toString(analysis.getValuesOf(code[10].as<ExpressionAddress>(), R)));
		EXPECT_EQ("{(0-2-1-1-2-0-1-2-0-1,[[0,0],[<0,[0,0],0>,<0,[0,0],0>]],#.r)}", toString(analysis.getValuesOf(code[11].as<ExpressionAddress>(), R)));

//		createDotDump(analysis);
	}

	TEST(CBA, MutableStruct) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	let point = struct { int a; int b; };"
				"	"
				"	decl auto p1 = var(struct point { 1, 2 });"
				"	decl auto p2 = var(struct point { 3, 4 });"
				"	decl ref<point> p3 = p1;"				// an alias
				"	decl ref<ref<point>> p4 = var(p1);"		// a pointer
				"	"
				"	*p1;"
				"	*p2;"
				"	*p3;"
				"	**p4;"
				"	*p1.a;"
				"	*p1.b;"
				"	*p2.a;"
				"	*p2.b;"
				"	*p3.a;"
				"	*p3.b;"
				"	*(*p4).a;"
				"	*(*p4).b;"
				"	"
				"	"
				"	p1.a = 5;"
				"	"
				"	*p1;"
				"	*p2;"
				"	*p3;"
				"	**p4;"
				"	*p1.a;"
				"	*p1.b;"
				"	*p2.a;"
				"	*p2.b;"
				"	*p3.a;"
				"	*p3.b;"
				"	*(*p4).a;"
				"	*(*p4).b;"
				"	"
				"	"
				"	p3.b = 6;"
				"	"
				"	*p1;"
				"	*p2;"
				"	*p3;"
				"	**p4;"
				"	*p1.a;"
				"	*p1.b;"
				"	*p2.a;"
				"	*p2.b;"
				"	*p3.a;"
				"	*p3.b;"
				"	*(*p4).a;"
				"	*(*p4).b;"
				"	"
				"	"
				"	p4 = p2;"				// updating the pointer
				"	"
				"	*p1;"
				"	*p2;"
				"	*p3;"
				"	**p4;"
				"	*p1.a;"
				"	*p1.b;"
				"	*p2.a;"
				"	*p2.b;"
				"	*p3.a;"
				"	*p3.b;"
				"	*(*p4).a;"
				"	*(*p4).b;"
				"	"
				"	"
				"	*p4 = struct point { 7, 8 };"				// updating the values
				"	"
				"	*p1;"
				"	*p2;"
				"	*p3;"
				"	**p4;"
				"	*p1.a;"
				"	*p1.b;"
				"	*p2.a;"
				"	*p2.b;"
				"	*p3.a;"
				"	*p3.b;"
				"	*(*p4).a;"
				"	*(*p4).b;"
				"	"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		int pos = 4;
		EXPECT_EQ("[a={1},b={2}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={3},b={4}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={1},b={2}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={1},b={2}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));

		pos++;
		EXPECT_EQ("[a={5},b={2}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={3},b={4}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={5},b={2}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={5},b={2}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));

		pos++;
		EXPECT_EQ("[a={5},b={6}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={3},b={4}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={5},b={6}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={5},b={6}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));

		pos++;
		EXPECT_EQ("[a={5},b={6}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={3},b={4}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={5},b={6}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={3},b={4}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));

		pos++;
		EXPECT_EQ("[a={5},b={6}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={7},b={8}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={5},b={6}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[a={7},b={8}]", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{7}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{8}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{7}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{8}", toString(analysis.getValuesOf(code[pos++].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);

		pos++;

	}

	TEST(CBA, SimpleArray) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	decl ref<array<int,1>> a = var(array_create_1D(lit(int), 10u));"
				"	"
				"	a[0] = 10;"
				"	a[1] = 12;"
				"	*a[0];"	// = 10
				"	*a[1];" // = 12
				"	"
				"	for(int i = 0 .. 10 ) {"
				"		a[i] = i;"
				"	}"
				"	"
				"	*a[7];"	// = i (=v7)
				"	"
				"	a[3] = 14;"
				"	*a[3];" // = 14
				"	"
				"	*a[0];"
				"	a[0] = 16;"
				"	*a[0];" // = 16

				"}"
		).as<CompoundStmtPtr>();


		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{10}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{12}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{v7}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{14}", toString(analysis.getValuesOf(code[8].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{v7,10}", toString(analysis.getValuesOf(code[9].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{16}", toString(analysis.getValuesOf(code[11].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}


	TEST(CBA, Tuples) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	let pair = (int,int);"
				"	"
				"	decl ref<pair> a = var((1,2));"
				"	"
				"	*a;"
				"	a.0 = 3;"
				"	*a;"
				"	a.1 = 4;"
				"	*a;"
				"	a = (5,6);"
				"	*a;"
				"	*a.0;"
				"	*a.1;"
				"	(*a).0;"
				"	(*a).1;"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("[0={1},1={2}]", toString(analysis.getValuesOf(code[1], A)));
		EXPECT_EQ("[0={3},1={2}]", toString(analysis.getValuesOf(code[3], A)));
		EXPECT_EQ("[0={3},1={4}]", toString(analysis.getValuesOf(code[5], A)));
		EXPECT_EQ("[0={5},1={6}]", toString(analysis.getValuesOf(code[7], A)));

		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[8], A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[9], A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[10], A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[11], A)));

	}

	TEST(CBA, SimpleTuple) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	let tuple = (int<4>, int<4>);"
				"	"
				""
				"	decl tuple t1 = (1, 2);"
				"	decl tuple t2 = (3, 4);"
				""
				""
				"	"
				"	t1;"
				"	t2;"
				"	tuple_member_access(t1,0u, lit(int<4>));"
				"	tuple_member_access(t1,1u, lit(int<4>));"
				"	tuple_member_access(t2,0u, lit(int<4>));"
				"	tuple_member_access(t2,1u, lit(int<4>));"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("[0={1},1={2}]", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[0={3},1={4}]", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), A)));
	}

	TEST(CBA, RefTuple) {

		// a simple test cases checking the handling of simple value structs
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	let tuple = (int<4>, int<4>);"
				"	"
				"	let writeToTuple = lambda (ref<tuple> lt, int<4> x)->unit {"
				"		tuple_ref_elem(lt,0u, lit(int<4>)) = x;"
				"	};"
				""
				"	decl ref<tuple> t1;"
				"	decl ref<tuple> t2;"
				""
				"	tuple_ref_elem(t1,0u, lit(int<4>)) = 1;"
				"	tuple_ref_elem(t1,1u, lit(int<4>)) = 2;"
				"	tuple_ref_elem(t2,0u, lit(int<4>)) = 3;"
				"	tuple_ref_elem(t2,1u, lit(int<4>)) = 4;"
				""
				"	"
				"	*t1;"
				"	*t2;"
				"	tuple_member_access(*t1,0u, lit(int<4>));"
				"	tuple_member_access(*t1,1u, lit(int<4>));"
				"	tuple_member_access(*t2,0u, lit(int<4>));"
				"	tuple_member_access(*t2,1u, lit(int<4>));"
				""
				"	decl ref<int<4>> s = var(5);"
				"	writeToTuple(t1, *s);"
				"	tuple_member_access(*t1,0u, lit(int<4>));"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		std::cout << core::checks::check(code) << std::endl;

		CBA analysis(code);

		EXPECT_EQ("[0={1},1={2}]", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[0={3},1={4}]", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[8].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[9].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[10].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[11].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[14].as<ExpressionAddress>(), A)));

	}
} // end namespace cba
} // end namespace analysis
} // end namespace insieme
