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

#include "insieme/core/ir_builder.h"

#include "insieme/utils/timer.h"
#include "insieme/utils/test/test_utils.h"

#include "cba_test.inc.h"

namespace insieme {
namespace analysis {
namespace cba {

	using namespace core;

	TEST(CBA, Context) {
		typedef DefaultContext Context;

		Context c;
		EXPECT_EQ("[[0,0],[<0,[0,0],0>,<0,[0,0],0>]]", toString(c));

		c.callContext <<= 1;
		EXPECT_EQ("[[0,1],[<0,[0,0],0>,<0,[0,0],0>]]", toString(c));

		c.callContext = c.callContext << 2;
		EXPECT_EQ("[[1,2],[<0,[0,0],0>,<0,[0,0],0>]]", toString(c));

		c.callContext = c.callContext << 3;
		EXPECT_EQ("[[2,3],[<0,[0,0],0>,<0,[0,0],0>]]", toString(c));

		c.callContext = c.callContext >> 4;
		EXPECT_EQ("[[4,2],[<0,[0,0],0>,<0,[0,0],0>]]", toString(c));

		c.callContext >>= 5;
		EXPECT_EQ("[[5,4],[<0,[0,0],0>,<0,[0,0],0>]]", toString(c));
	}


	TEST(CBA, GetDefinitionPoint) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto w = builder.variable(builder.parseType("A"), 123);
		map<string, NodePtr> symbols;
		symbols["w"] = w;

		auto pos = builder.parseAddressesStatement(R"(
                lambda (int<4> x, int<4> y)->unit { 
					$x$;
					$y$;
					$w$;
					decl int<4> z = 123;
					$z$;
					{
						$z$;
						decl auto z = 1234;
						$z$;
					};
					$z$;
					decl int<4> w2= 123;
					$w2$;
					decl int<4> w3 = 123;
					$w3$;
				};
                )", symbols
		);

		EXPECT_EQ(9u, pos.size());

		vector<VariableAddress> vars;
		for (auto cur : pos) { vars.push_back(cur.as<VariableAddress>()); }

		auto root = LambdaExprAddress(pos[0].getRootNode().as<LambdaExprPtr>());

		auto paramX = root->getParameterList()[0];
		auto paramY = root->getParameterList()[1];
		auto freeW = VariableAddress(w);
		auto localZ1 = pos[0].getParentAddress().getAddressOfChild(3,0).as<VariableAddress>();
		auto localZ2 = pos[0].getParentAddress().getAddressOfChild(5,1,0).as<VariableAddress>();
		auto localW1 = pos[0].getParentAddress().getAddressOfChild(7,0).as<VariableAddress>();
		auto localW2 = pos[0].getParentAddress().getAddressOfChild(9,0).as<VariableAddress>();

		// simple cases
		EXPECT_EQ(paramX,  getDefinitionPoint(paramX));
		EXPECT_EQ(paramY,  getDefinitionPoint(paramY));
		EXPECT_EQ(freeW,   getDefinitionPoint(freeW));
		EXPECT_EQ(localZ1, getDefinitionPoint(localZ1));
		EXPECT_EQ(localZ2, getDefinitionPoint(localZ2));
		EXPECT_EQ(localW1, getDefinitionPoint(localW1));
		EXPECT_EQ(localW2, getDefinitionPoint(localW2));

		EXPECT_EQ(paramX,  getDefinitionPoint(vars[0]));
		EXPECT_EQ(paramY,  getDefinitionPoint(vars[1]));
		EXPECT_EQ(freeW,   getDefinitionPoint(vars[2]));
		EXPECT_EQ(localZ1, getDefinitionPoint(vars[3]));
		EXPECT_EQ(localZ1, getDefinitionPoint(vars[4]));
		EXPECT_EQ(localZ2, getDefinitionPoint(vars[5]));
		EXPECT_EQ(localZ1, getDefinitionPoint(vars[6]));
		EXPECT_EQ(localW1, getDefinitionPoint(vars[7]));
		EXPECT_EQ(localW2, getDefinitionPoint(vars[8]));


		// check within CBA context
		CBA context(root);
		auto varX = context.getVariableLabel(paramX);
		EXPECT_EQ(varX, context.getVariableLabel(paramX));
		EXPECT_EQ(varX, context.getVariableLabel(vars[0]));

		auto varY = context.getVariableLabel(vars[1]);
		EXPECT_EQ(varY, context.getVariableLabel(paramY));
		EXPECT_NE(varX, varY);

		auto varW = context.getVariableLabel(freeW);
		EXPECT_EQ(varW, context.getVariableLabel(vars[2]));

		auto varW1 = context.getVariableLabel(localW1);
		EXPECT_EQ(varW1, context.getVariableLabel(vars[7]));

		auto varW2 = context.getVariableLabel(vars[8]);
		EXPECT_EQ(varW2, context.getVariableLabel(localW2));

		auto varZ1 = context.getVariableLabel(localZ1);
		auto varZ2 = context.getVariableLabel(localZ2);

		EXPECT_NE(varZ1, varZ2);
		EXPECT_EQ(varZ1, context.getVariableLabel(vars[3]));
		EXPECT_EQ(varZ1, context.getVariableLabel(vars[4]));
		EXPECT_EQ(varZ2, context.getVariableLabel(vars[5]));
		EXPECT_EQ(varZ1, context.getVariableLabel(vars[6]));

		// check that all variables are distinct
		std::set<Variable> allVars = { varX, varY, varZ1, varZ2, varW, varW1, varW2 };
		EXPECT_EQ(7u, allVars.size()) << allVars;

	}

	TEST(CBA, MemoryConstructor) {
		NodeManager mgr;
		IRBuilder builder(mgr);

		TypePtr A = builder.genericType("A");
		TypePtr refType = builder.refType(A);

		auto litA = builder.literal("const", A);
		auto litB = builder.literal("var", refType);

		auto expr = builder.integerLit(12);
		auto alloc = builder.parseExpr("ref_alloc(  lit(int<4>), memloc_stack)");

		EXPECT_FALSE(isMemoryConstructor(StatementAddress(litA)));
		EXPECT_TRUE(isMemoryConstructor(StatementAddress(litB)));

		EXPECT_FALSE(isMemoryConstructor(StatementAddress(expr)));
		EXPECT_TRUE(isMemoryConstructor(StatementAddress(alloc)));

	}

	TEST(CBA, UndefinedValues) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		std::map<string,NodePtr> symbols;
		symbols["c"] = builder.parse("lit(\"c\":ref<int<4>>)");
		symbols["d"] = builder.parse("lit(\"c\":struct { int<4> x; vector<int<4>,3> y; })");
		symbols["e"] = builder.parse("lit(\"c\":ref<struct { int<4> x; vector<int<4>,3> y; }>)");

		auto in = builder.parseStmt(
				"{"
				"	decl int<4> a;"
				"	decl ref<int<4>> b;"
				"	a;"
				"	*b;"
				"	*c;"
				"	d;"
				"	*e;"
				"	"
				"	b = 2;"
				"	c = 3;"
				"	e.x = 4;"
				"	"
				"	*b;"
				"	*c;"
				"	*e;"
				"}", symbols
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{-unknown-}", toString(analysis.getValuesOf(code[2], A)));
		EXPECT_EQ("{-unknown-}", toString(analysis.getValuesOf(code[3], A)));
		EXPECT_EQ("{-unknown-}", toString(analysis.getValuesOf(code[4], A)));
		EXPECT_EQ("[x={-unknown-},y=[*={-unknown-}]]", toString(analysis.getValuesOf(code[5], A)));
		EXPECT_EQ("[x={-unknown-},y=[*={-unknown-}]]", toString(analysis.getValuesOf(code[6], A)));

		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[10], A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[11], A)));
		EXPECT_EQ("[x={4},y=[*={-unknown-}]]", toString(analysis.getValuesOf(code[12], A)));

//		createDotDump(analysis);
	}

	TEST(CBA, BasicControlFlow) {
		typedef DefaultContext Context;

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	decl int<4> x = 12;"
				"	decl auto y = lambda (int<4> z)->unit {};"
				"	y(14);"
				"	y(16);"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		auto declX = code[0].as<DeclarationStmtAddress>();
		VariableAddress varX = declX->getVariable();
		ExpressionAddress initX = declX->getInitialization();

		EXPECT_EQ("{AP(12)}", toString(analysis.getValuesOf(initX, D)));
		EXPECT_EQ("{AP(12)}", toString(analysis.getValuesOf(varX, d)));

		auto declY = code[1].as<DeclarationStmtAddress>();
		VariableAddress varY = declY->getVariable();
		ExpressionAddress initY = declY->getInitialization();

//		std::cout << *varY << " = " << analysis.getValuesOf(varY) << "\n";
//		std::cout << *initY << " = " << analysis.getValuesOf(initY) << "\n";
		EXPECT_EQ("{((Lambda@0-1-1-2-0-1),[[0,0],[<0,[0,0],0>,<0,[0,0],0>]])}", toString(analysis.getValuesOf(initY, C)));
		EXPECT_EQ("{((Lambda@0-1-1-2-0-1),[[0,0],[<0,[0,0],0>,<0,[0,0],0>]])}", toString(analysis.getValuesOf(varY, c)));


		auto varZ = initY.as<LambdaExprAddress>()->getParameterList()[0];
//		std::cout << *varZ << " = " << analysis.getValuesOf(varZ) << "\n";

		// check out the context specific control flow handling
		auto l1 = analysis.getLabel(code[2]);
		auto l2 = analysis.getLabel(code[3]);

		EXPECT_EQ("{AP(14)}", toString(analysis.getValuesOf(varZ, d, Context(Context::call_context(0,l1)))));
		EXPECT_EQ("{AP(16)}", toString(analysis.getValuesOf(varZ, d, Context(Context::call_context(0,l2)))));
//		createDotDump(analysis);
	}

	TEST(CBA, ReturnValue) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	decl int<4> x = 12;"
				"	decl auto y = lambda (int<4> z)->int<4> { return 10; };"
				"	decl int<4> z = y(x);"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		auto declZ = code[2].as<DeclarationStmtAddress>();
		VariableAddress varZ = declZ->getVariable();

		// std::cout << *varZ << " = " << analysis.getValuesOf(varZ) << "\n";
		EXPECT_EQ("{AP(10)}", toString(analysis.getValuesOf(varZ, D)));
//		createDotDump(analysis);
	}

	TEST(CBA, ReturnValue2) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto code = builder.parseStmt(
				"{"
				"	let id = lambda ('a x)->'a { return x; };"
				"	decl auto a = 1;"
				"	decl auto b = id(2);"
				"	decl auto c = id(b);"
				"	decl auto d = id(id(3));"
				"	decl auto e = id(id(d));"
				"	decl auto f = id(id(id(4)));"
				"}"
		).as<CompoundStmtPtr>();

		EXPECT_TRUE(code);

		StatementAddress root(code);
		CBA analysis(root);

		vector<DeclarationStmtAddress> decls;
		for(auto cur : CompoundStmtAddress(code)) { decls.push_back(cur.as<DeclarationStmtAddress>()); }

		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(decls[0]->getVariable(), d)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(decls[1]->getVariable(), d)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(decls[2]->getVariable(), d)));
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(decls[3]->getVariable(), d)));
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(decls[4]->getVariable(), d)));
		EXPECT_EQ("{AP(4)}", toString(analysis.getValuesOf(decls[5]->getVariable(), d)));
//		createDotDump(analysis);
	}

	TEST(CBA, References1) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto code = builder.parseStmt(
				"{"
				"	decl ref<int<4>> x = var(1);"		// set x to 1
				"	*x;"							// should be 1
				"}"
		).as<CompoundStmtPtr>();

		EXPECT_TRUE(code);

		StatementAddress root(code);
		CBA analysis(root);

		auto decl = CompoundStmtAddress(code)[0].as<DeclarationStmtAddress>();
		auto val = CompoundStmtAddress(code)[1].as<ExpressionAddress>();

		// this would be the ideal case
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(val, D)));
//		createDotDump(analysis);
	}

	TEST(CBA, References2) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto code = builder.parseStmt(
				"{"
				"	decl ref<int<4>> x = var(1);"		// set x to 1
				"	decl auto y = *x;"					// y should be 1
				"	x = 2;"							// set x to 2
				"	decl auto z = *x;"					// z should be 2
				"	x = 3;"							// set x to 3
				"	decl auto w = *x;"					// z should be 3
				"}"
		).as<CompoundStmtPtr>();

		EXPECT_TRUE(code);

		StatementAddress root(code);
		CBA analysis(root);

		auto varY = CompoundStmtAddress(code)[1].as<DeclarationStmtAddress>()->getVariable();
		auto varZ = CompoundStmtAddress(code)[3].as<DeclarationStmtAddress>()->getVariable();
		auto varW = CompoundStmtAddress(code)[5].as<DeclarationStmtAddress>()->getVariable();

		// this would be the ideal case
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(varY, d)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(varZ, d)));
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(varW, d)));

	}

	TEST(CBA, References3) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"

				// init variables
				"	decl ref<int<4>> x = var(1);"		// set x to 1
				"	decl ref<int<4>> y = var(2);"		// set y to 2
				"	decl ref<int<4>> z = x;"				// z is an alias of x
				"	decl ref<ref<int<4>>> p = var(y);"	// p is a pointer on y

				// read values
				"	*x;"
				"	*y;"
				"	*z;"
				"	**p;"

				// update values
				"	x = 3;"							// x and z should be 3

				// read values
				"	*x;"
				"	*y;"
				"	*z;"
				"	**p;"

				// update pointer
				"	p = z;"							// p should now reference z = x

				// read values
				"	*x;"
				"	*y;"
				"	*z;"
				"	**p;"

				// update pointer
				"	*p = 4;"							// *x = *z = **p should now be 4

				// read values
				"	*x;"
				"	*y;"
				"	*z;"
				"	**p;"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// read first set of values
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), D)));

		// read second set of values
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(code[ 9].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code[10].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(code[11].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code[12].as<ExpressionAddress>(), D)));

		// read third set of values
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(code[14].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code[15].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(code[16].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(code[17].as<ExpressionAddress>(), D)));

		// read fourth set of values
		EXPECT_EQ("{AP(4)}", toString(analysis.getValuesOf(code[19].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code[20].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(4)}", toString(analysis.getValuesOf(code[21].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(4)}", toString(analysis.getValuesOf(code[22].as<ExpressionAddress>(), D)));
//		createDotDump(analysis);
	}

	TEST(CBA, References4) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto code = builder.parseStmt(
				"{"
				"	decl ref<int<4>> x = var(1);"		// set x to 1
				"	x = 2;"
				"	x = 3;"
				"	x = 4;"
				"	x = 5;"
				"	*x;"
				"}"
		).as<CompoundStmtPtr>();

		EXPECT_TRUE(code);

		CompoundStmtAddress root(code);
		CBA analysis(root);

		// the easy part - x should be correct
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(root[5].as<ExpressionAddress>(), A)));

		// the tricky part - there should only be a small number of sets involved
		EXPECT_LE(analysis.getNumSets(), 40);

//		createDotDump(analysis);
	}

	TEST(CBA, ExternalLiterals) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		map<string,NodePtr> symbols;
		symbols["e"] = builder.literal("e", builder.parseType("ref<int<4>>"));

		auto code = builder.parseStmt(
				"{"
				"	*e;"
				"	e = 1;"
				"	*e;"
				"	e = 2;"
				"	*e;"
				"	e = 3;"
				"	*e;"
				"}",
				symbols
		).as<CompoundStmtPtr>();

		EXPECT_TRUE(code);

		CompoundStmtAddress root(code);
		CBA analysis(root);

		// check whether globals are propery handled
		EXPECT_EQ("{-unknown-}", toString(analysis.getValuesOf(root[0].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(root[2].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(root[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(root[6].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}

	TEST(CBA, ExternalLiteralsStructured) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		map<string,NodePtr> symbols;
		symbols["e"] = builder.literal("e", builder.parseType("ref<struct{ vector<int<4>,20> value; bool flag; }>"));

		auto code = builder.parseStmt(
				"{"
				"	*e;"
				"	e.flag = true;"
				"	*e;"
				"	e.value[2] = 5;"
				"	*e;"
				"	e.value[2] = 8;"
				"	*e;"
				"}",
				symbols
		).as<CompoundStmtPtr>();

		EXPECT_TRUE(code);

		CompoundStmtAddress root(code);
		CBA analysis(root);

		// check whether globals are propery handled
		EXPECT_EQ("[flag={-unknown-},value=[*={-unknown-}]]", toString(analysis.getValuesOf(root[0].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[flag={0,1},value=[*={0,1}]]", toString(analysis.getValuesOf(root[0].as<ExpressionAddress>(), B)));

		EXPECT_EQ("[flag={-unknown-},value=[*={-unknown-}]]", toString(analysis.getValuesOf(root[2].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[flag={1},value=[*={0,1}]]", toString(analysis.getValuesOf(root[2].as<ExpressionAddress>(), B)));

		EXPECT_EQ("[flag={-unknown-},value=[2={5},*={-unknown-}]]", toString(analysis.getValuesOf(root[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[flag={1},value=[2={0,1},*={0,1}]]", toString(analysis.getValuesOf(root[4].as<ExpressionAddress>(), B)));

		EXPECT_EQ("[flag={-unknown-},value=[2={8},*={-unknown-}]]", toString(analysis.getValuesOf(root[6].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[flag={1},value=[2={0,1},*={0,1}]]", toString(analysis.getValuesOf(root[6].as<ExpressionAddress>(), B)));

//		createDotDump(analysis);
	}


	TEST(CBA, IfStmt1) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"

				// init variables
				"	decl ref<int<4>> x = var(1);"		// set x to 1
				"	*x;"							// should be 1
				"	if (x > 0) {"
				"		*x;"						// should be 1
				"		x = 2;"
				"		*x;"						// should be 2
				"	} else {"
				"		*x;"						// should be 1
				"		x = 3;"
				"		*x;"						// should be 3
				"	}"
				"	*x;"							// what is x? - conservative: {2,3}
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// check value of condition
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code.getAddressOfChild(2,0).as<ExpressionAddress>(), B)));

		// check value of *x
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code.getAddressOfChild(1).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,1,0).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,1,2).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{}", toString(analysis.getValuesOf(code.getAddressOfChild(2,2,0).as<ExpressionAddress>(), D)));		// never evaluated!
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,2,2).as<ExpressionAddress>(), D)));

		// the last is fixed since we know the condition
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code.getAddressOfChild(3).as<ExpressionAddress>(), D)));
	}

	TEST(CBA, IfStmt2) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		std::map<string, NodePtr> symbols;
		symbols["e"] = builder.literal("e", builder.getLangBasic().getInt4());

		auto in = builder.parseStmt(
				"{"

				// init variables
				"	decl ref<int<4>> x = var(1);"		// set x to 1
				"	*x;"							// should be 1
				"	if (x > e) {"
				"		*x;"						// should be 1
				"		x = 2;"
				"		*x;"						// should be 2
				"	} else {"
				"		*x;"						// should be 1
				"		x = 3;"
				"		*x;"						// should be 3
				"	}"
				"	*x;"							// what is x? - conservative: {2,3}
				"}",
				symbols
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// check value of condition
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code.getAddressOfChild(2,0).as<ExpressionAddress>(), B)));

		// check value of *x
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code.getAddressOfChild(1).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,1,0).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,1,2).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,2,0).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,2,2).as<ExpressionAddress>(), D)));

		// the last one may be both
		std::set<ExpressionPtr> should;
		should.insert(builder.intLit(2));
		should.insert(builder.intLit(3));
		EXPECT_EQ(should, std::set<ExpressionPtr>(analysis.getValuesOf(code.getAddressOfChild(3).as<ExpressionAddress>(), D)));

	}

	TEST(CBA, WhileStmt) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	decl ref<int<4>> x = var(1);"		// set x to 1
				"	*x;"							// should be 1
				"	while (x > 0) {"
				"		*x;"						// should be 1 or unknown
				"		x = x - 1;"
				"		*x;"						// should be unknown
				"	}"
				"	*x;"							// what is x? - should be unknown
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// check value of *x
		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code.getAddressOfChild(1).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(NULL),AP(1)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,1,0).as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(NULL)}", toString(analysis.getValuesOf(code.getAddressOfChild(2,1,2).as<ExpressionAddress>(), D)));

		// the last one may be both
		std::set<ExpressionPtr> should;
		should.insert(ExpressionPtr());
		should.insert(builder.intLit(1));
		EXPECT_EQ(should, std::set<ExpressionPtr>(analysis.getValuesOf(code.getAddressOfChild(3).as<ExpressionAddress>(), D)));

	}

	TEST(CBA, WhileStmt_2) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	decl ref<int<4>> x = var(1);"		// set x to 1
				"	*x;"							// should be 1
				"	while (false) {"
				"		x = 2;"
				"	}"
				"	*x;"							// should still be 1
				"	while (true) {"
				"		x = 3;"
				"	}"
				"	*x;"							// should nothing (unreachable)
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// check values of *x
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), A)));

		EXPECT_EQ("{reachable}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), Rin)));
		EXPECT_EQ("{reachable}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), Rin)));
		EXPECT_EQ("{}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), Rin)));

	}

	TEST(CBA, ForStmt) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.normalize(builder.parseStmt(
				"{"
				"	for (int<4> i = 0 .. 10 : 1) {"
				"		i;"								// should be i .. symbolic
				"		2*i+3;"							// should be 2*i+3 .. symbolic
				"		(3*i+2*i)*i;"					// should be 5*i^2 .. symbolic
				"	}"
				"}"
		)).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		auto forStmt = code[0].as<ForStmtAddress>();
		auto iter = forStmt->getIterator();
		auto body = forStmt->getBody();

		// check iterator definition points
		EXPECT_EQ(iter, getDefinitionPoint(iter));

		// within the loop
		EXPECT_EQ("{v0}",     toString(analysis.getValuesOf(body[0].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2*v0+3}", toString(analysis.getValuesOf(body[1].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5*v0^2}", toString(analysis.getValuesOf(body[2].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}

	TEST(CBA, ForStmt_2) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.normalize(builder.parseStmt(
				"{"
				"	decl ref<int<4>> x = var(0);"			// set x to 0
				"	*x;"								// should be 0
				"	for (int<4> i = 0 .. 10 : 1) {"
				"		i;"								// should be i .. symbolic
				"		2*i+3;"							// should be 2*i+3 .. symbolic
				"		x = x + i;"
				"		*x;"							// should be unknown
				"	}"
				"	*x;"								// what is x? - should be unknown
				"}"
		)).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		auto forStmt = code[2].as<ForStmtAddress>();
		auto body = forStmt->getBody();

		// check value of *x before loop
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));

		// within the loop
		EXPECT_EQ("{v1}", toString(analysis.getValuesOf(body[0].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2*v1+3}", toString(analysis.getValuesOf(body[1].as<ExpressionAddress>(), A)));

		lattice<decltype(A)>::type::less_op_type less_op;

		// the value of x should be unknown within the loop (although some examples are recorded)
		EXPECT_PRED2(less_op, Formula(), analysis.getValuesOf(body[3].as<ExpressionAddress>(), A));

		// after the loop
		EXPECT_PRED2(less_op, Formula(), analysis.getValuesOf(code[3].as<ExpressionAddress>(), A));

//		createDotDump(analysis);
	}

	TEST(CBA, ForStmt_3) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.normalize(builder.parseStmt(
				"{"
				"	for (int<4> i = 0 .. 10 : 1) {"
				"		decl auto x = 2*i + 3;"
				"		for(int<4> j = 0 .. 10 : 1) {"
				"			i;"
				"			j;"
				"			i+j;"
				"			i+j+x;"
				"		}"
				"	}"
				"}"
		)).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		auto forStmtA = code[0].as<ForStmtAddress>();
		auto bodyA = forStmtA->getBody();

		auto forStmtB = bodyA[1].as<ForStmtAddress>();
		auto bodyB = forStmtB->getBody();

		// check the inner indices
		EXPECT_EQ("{v0}", 	  	 toString(analysis.getValuesOf(bodyB[0].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{v2}", 	  	 toString(analysis.getValuesOf(bodyB[1].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{v0+v2}",  	 toString(analysis.getValuesOf(bodyB[2].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3*v0+v2+3}", toString(analysis.getValuesOf(bodyB[3].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}

	TEST(CBA, ForStmt_4) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.normalize(builder.parseStmt(
				"{"
				"	decl ref<int<4>> y = var(0);"
				"	decl ref<int<4>> z = var(0);"
				"	for (int<4> i = 0 .. 10 : 1) {"
				"		decl auto x = var(2*i + 3);"
				"		x = x + 2 * i;"
				"		*x;"
				"		*y;"
				"		z = *x;"
				"		*z;"
				"	}"
				"}"
		)).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		auto forStmt = code[2].as<ForStmtAddress>();
		auto body = forStmt->getBody();

		// check the inner indices
		EXPECT_EQ("{4*v2+3}",  	 toString(analysis.getValuesOf(body[2].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{0}",  	 	 toString(analysis.getValuesOf(body[3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4*v2+3}",  	 toString(analysis.getValuesOf(body[5].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}

	TEST(CBA, ForStmt_5) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	decl ref<int> r = var(1);"
				"	for(int i = 1 .. 5 : 1) {"
				"		r = r * i;"
				"	}"
				"	*r;"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);

		CompoundStmtAddress code(in);

		Formula unknown;

		CBA analysis(code);
		lattice<decltype(A)>::type::less_op_type less_op;
		EXPECT_PRED2(less_op, unknown, analysis.getValuesOf(code[2].as<ExpressionAddress>(), A));

//		createDotDump(analysis);
	}

	TEST(CBA, ForStmt_6) {

		NodeManager mgr;
		IRBuilder builder(mgr);
		std::map<string,NodePtr> symbols;
		symbols["c"] = builder.literal("c", builder.parseType("int<4>"));

		auto in = builder.normalize(builder.parseStmt(
				"{"
				"	decl ref<int<4>> x = var(0);"
				"	*x;"		// should be 0
				"	for (int<4> i = 0 .. 10 : 1) {"
				"		x = 1;"
				"	}"
				"	*x;"		// should be 1
				"	for (int<4> i = 10 .. 0 : 1) {"
				"		x = 2;"
				"	}"
				"	*x;"		// should still be 1
				"	for (int<4> i = 0 .. 10 : -1) {"
				"		x = 3;"
				"	}"
				"	*x;"		// should still be 1
				"	for (int<4> i = 10 .. 0 : -1) {"
				"		x = 4;"
				"	}"
				"	*x;"		// should be 4 now
				"	for (int<4> i = 10 .. 0 : c) {"
				"		x = 5;"
				"	}"
				"	*x;"		// should be 4 or 5
				"	for (int<4> i = c .. 0 : 1) {"
				"		x = 6;"
				"	}"
				"	*x;"		// should be 4, 5 or 6
				"	for (int<4> i = c .. c : 1) {"
				"		x = 7;"
				"	}"
				"	*x;"		// should be 7
				"	for (int<4> i = c .. c+1 : 1) {"
				"		x = 8;"
				"	}"
				"	*x;"		// should be 8
				"	for (int<4> i = c .. c-1 : 1) {"
				"		x = 9;"
				"	}"
				"	*x;"		// should be 8
				"	for (int<4> i = c .. c-1 : c) {"
				"		x = 10;"
				"	}"
				"	*x;"		// should be 8 or 10
				"}", symbols
		)).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// within the loop
		EXPECT_EQ("{0}",    toString(analysis.getValuesOf(code[ 1].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", 	toString(analysis.getValuesOf(code[ 3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", 	toString(analysis.getValuesOf(code[ 5].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", 	toString(analysis.getValuesOf(code[ 7].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", 	toString(analysis.getValuesOf(code[ 9].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5,4}", 	toString(analysis.getValuesOf(code[11].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6,5,4}",toString(analysis.getValuesOf(code[13].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{7}", 	toString(analysis.getValuesOf(code[15].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{8}", 	toString(analysis.getValuesOf(code[17].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{8}", 	toString(analysis.getValuesOf(code[19].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{10,8}", toString(analysis.getValuesOf(code[21].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}

	TEST(CBA, Arithmetic_101) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		std::map<string, NodePtr> symbols;
		symbols["e"] = builder.literal("e", builder.getLangBasic().getInt4());

		auto in = builder.parseStmt(
				"{"
				// constants
				"	0;"						// should be 0
				"	1;"						// should be 1
				"	e;"						// should be unknown

				// constant expressions
				"	1+1;"
				"	1+1+1;"
				"	1+2*3;"
				"	17+4;"

				// constant expressions with variables
				"	decl auto x = 2;"
				"	decl auto y = 3;"
				"	x;"
				"	2*x+1;"
				"	2*x+4*y;"
				"	2*x+4*y+e;"

				"	decl ref<int<4>> z = var(10);"
				"	decl ref<int<4>> w = var(5);"
				"	decl ref<int<4>> a = z;"
				"	decl ref<ref<int<4>>> p = var(z);"

				"	*z;"
				"	*p+2*z;"

				"	p = w;"
				"	*p+2*z+4*e*(*p)+a;"

				// boolean constraints
				"	1 < 2;"
				"	2*x+1 < 2;"
				"	2*x+1 > 2*x;"
				"	2*x+1 == (2+1)*x;"
				"	2*x < e;"

//				// ternary operator
//				"	(x<2)?1:2;"				// x is 2 => should be 2
//				"	(x<3)?1:2;"				// x is 2 => should be 1
//				"	(x<y)?1:2;"				// y is 3 => should be 1
//				"	(x<e)?1:2;"				// unknown => should be {1,2}

				"}",
				symbols
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// check values
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[0].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{e}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A)));

		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{7}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{21}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), A)));

		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[9].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[10].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{16}", toString(analysis.getValuesOf(code[11].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{e+16}", toString(analysis.getValuesOf(code[12].as<ExpressionAddress>(), A)));

		EXPECT_EQ("{10}", toString(analysis.getValuesOf(code[17].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{30}", toString(analysis.getValuesOf(code[18].as<ExpressionAddress>(), A)));

		EXPECT_EQ("{20*e+35}", toString(analysis.getValuesOf(code[20].as<ExpressionAddress>(), A)));

		// boolean constraints
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[21].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[22].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[23].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[24].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[25].as<ExpressionAddress>(), B)));
	}

	TEST(CBA, Arithmetic_102) {

		NodeManager manager;
		IRBuilder builder(manager);

		auto in = builder.parseStmt(
			"{"
			"	let int = int<4>;"
			"	decl ref<int> a = var(3);"
			"	decl ref<int> b = var(9 + (a * 5));"
			"	decl ref<int> c;"
			"	"
			"	c = b * 4;"
			"	if (c > 10) {"
			"		c = c - 10;"
			"	}"
			"	c * (60 + a);"
			"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// check values
		EXPECT_EQ("{5418}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}

	TEST(CBA, Arithmetic_103) {

		NodeManager manager;
		IRBuilder builder(manager);

		auto in = builder.parseStmt(
			"{"
			"	let int = int<4>;"
			"	let f = expr lit(\"xyz\": () -> int);"
			"	f();"
			"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		// check values
//		EXPECT_EQ("{-unknown-}", toString(analysis.getValuesOf(code[0].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}

	TEST(CBA, Arithmetic_Cast) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		auto& base = builder.getLangBasic();

		ExpressionAddress expr(builder.castExpr(base.getInt4(), builder.intLit(12)));

		EXPECT_EQ("cast<int<4>>(12)", toString(*expr));

		EXPECT_EQ("{12}", toString(getValues(expr, A)));
	}

	TEST(CBA, Boolean_101) {
		NodeManager mgr;
		IRBuilder builder(mgr);

		std::map<string, NodePtr> symbols;
		symbols["b"] = builder.literal("b", builder.getLangBasic().getBool());
		symbols["e"] = builder.literal("e", builder.getLangBasic().getInt4());
		symbols["v"] = builder.variable(builder.getLangBasic().getBool());

		auto in = builder.parseStmt(
				"{"
				// constants
				"	true;"						// should be 0
				"	false;"						// should be 1
				"	b;"							// should be unknown
				"	v;"							// should be unknown

				// boolean relations
				"	true == false;"
				"	true == true;"
				"	true != false;"
				"	true != true;"

				"	true == b;"
				"	false == b;"
				"	true != b;"
				"	false != b;"
				"	b == b;"
				"	b != b;"


//				// arithmetic comparison
				"	1<2;"
				"	2<1;"
				"	2<2;"
				"	1<e;"
				"	e<1;"

				"	1<=2;"
				"	2<=1;"
				"	2<=2;"
				"	1<=e;"
				"	e<=1;"

				"	1>=2;"
				"	2>=1;"
				"	2>=2;"
				"	1>=e;"
				"	e>=1;"

				"	1>2;"
				"	2>1;"
				"	2>2;"
				"	1>e;"
				"	e>1;"

				"	1==2;"
				"	2==2;"
				"	1==e;"
				"	e==e;"

				"	1!=2;"
				"	2!=2;"
				"	1!=e;"
				"	e!=e;"

				"}",
				symbols
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		int i = 0;
		// check constants
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		// check boolean relations
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		// arithmetic comparison
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0,1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

	}

	TEST(CBA, CallContext) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(R"(
				{

				// simple function
					decl auto y = lambda (int<4> z)->int<4> { return z; };
					y(1);								// this one should be { 1 }
					y(2);								// this one should be { 2 }
					y(3);								// this one should be { 2 }

				// higher order functions
					decl auto z = lambda ((int<4>)->int<4> x, int<4> i)->int<4> { return x(i); };
					z(y,4);								// this one should be { 4 }
					z(y,5);								// this one should be { 5 }

				}
		)").as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{AP(1)}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(2)}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(3)}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), D)));

		EXPECT_EQ("{AP(4)}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), D)));
		EXPECT_EQ("{AP(5)}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), D)));
//		createDotDump(analysis);

	}

	TEST(CBA, SideEffectHigherOrder) {

		// call a higher-order function causing some effect
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"

				// prepare some functions
				"	let inc = lambda (ref<int<4>> x)->unit { x = x+1; };"
				"	decl auto f = inc;"
				"	decl auto apply = lambda (ref<'a> l, (ref<'a>)->unit op)->unit { op(l); };"

				// apply them
				"	decl ref<int<4>> x = var(1);"
				"	*x;"							// should be 1
				"	x = x+1;"
				"	*x;"							// should be 2
				"	inc(x);"
				"	*x;"							// should be 3
				"	f(x);"
				"	*x;"							// should be 4
				"	apply(x, inc);"
				"	*x;"							// should be 5
				"	apply(x, f);"
				"	*x;"							// should be 6
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		const auto& C = cba::C;

		// check functions
		EXPECT_EQ("{((Lambda@0-6-1-2-0-1),[[0,0],[<0,[0,0],0>,<0,[0,0],0>]])}", toString(analysis.getValuesOf(code[6].as<CallExprAddress>()->getFunctionExpr(), C)));
		EXPECT_EQ("{((Lambda@0-0-1-2-0-1),[[0,0],[<0,[0,0],0>,<0,[0,0],0>]])}", toString(analysis.getValuesOf(code[8].as<CallExprAddress>()->getFunctionExpr(), C)));
		EXPECT_EQ("{((Lambda@0-1-1-2-0-1),[[0,0],[<0,[0,0],0>,<0,[0,0],0>]])}", toString(analysis.getValuesOf(code[10].as<CallExprAddress>()->getFunctionExpr(), C)));
		EXPECT_EQ("{((Lambda@0-10-3-2-0-1),[[0,0],[<0,[0,0],0>,<0,[0,0],0>]])}", toString(analysis.getValuesOf(code[10].as<CallExprAddress>()[1], C)));
		EXPECT_EQ("{((Lambda@0-1-1-2-0-1),[[0,0],[<0,[0,0],0>,<0,[0,0],0>]])}", toString(analysis.getValuesOf(code[12].as<CallExprAddress>()->getFunctionExpr(), C)));
		EXPECT_EQ("{((Lambda@0-0-1-2-0-1),[[0,0],[<0,[0,0],0>,<0,[0,0],0>]])}", toString(analysis.getValuesOf(code[12].as<CallExprAddress>()[1], C)));

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[9].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[11].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[13].as<ExpressionAddress>(), A)));

		EXPECT_LE(analysis.getNumSets(), 400);
		EXPECT_LE(analysis.getNumConstraints(), 500);

//		std::cout << "Num Sets:  " << analysis.getNumSets() << "\n";
//		std::cout << "Num Const: " << analysis.getNumConstraints() << "\n";
//		createDotDump(analysis);
	}

	TEST(CBA, SideEffectHigherOrder2) {

		// call a higher-order function causing some effect
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"

				// prepare some functions
				"	decl auto a = lambda (ref<int<4>> x)->unit { x = x+1; };"
				"	decl auto b = lambda (ref<int<4>> x)->unit { x = x*2; };"
				"	decl auto f = lambda (ref<int<4>> x, (ref<int<4>>)->unit a, (ref<int<4>>)->unit b)->unit {"
				"		if (x < 3) {"
				"			a(x);"
				"		} else {"
				"			b(x);"
				"		};"
				"	};"

				// apply them
				"	decl ref<int<4>> x = var(1);"
				"	*x;"							// should be 1
				"	f(x,a,b);"
				"	*x;"							// should be 2
				"	f(x,a,b);"
				"	*x;"							// should be 3
				"	f(x,a,b);"
				"	*x;"							// should be 6
				"	f(x,a,b);"
				"	*x;"							// should be 12
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[8].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[10].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{12}", toString(analysis.getValuesOf(code[12].as<ExpressionAddress>(), A)));

		EXPECT_LE(analysis.getNumSets(), 750);
		EXPECT_LE(analysis.getNumConstraints(), 800); 	// TODO: reduce this value back to 750 - there should not be more constraints than sets (duplicates)

//		std::cout << "Num Sets:  " << analysis.getNumSets() << "\n";
//		std::cout << "Num Const: " << analysis.getNumConstraints() << "\n";
//		createDotDump(analysis);
	}


	TEST(CBA, BooleanConnectors) {

		// call a higher-order function causing some effect
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"

				// prepare some functions
				"	decl auto a = true;"
				"	decl auto b = false;"

				// apply them
				"	true;"
				"	false;"
				"	!true;"
				"	!false;"

				"	1<2;"
				"	!(1<2);"

				"	true && false;"
				"	true && true;"

				"	false || true;"
				"	false || false;"

				// some combined stuff
				"	decl ref<int<4>> x = var(1);"
				"	decl ref<int<4>> y = var(2);"

				"	(0 < x) && (x < 2);"
				"	((0 < x) && (x < 2)) || ( y < x );"
				"	((0 < x) && (x < 2)) && ( y < x );"
				"	( y < x ) || ((0 < x) && (x < 2));"

				"	false || (true && true);"

				// TODO: check side-effects
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		int i = 0;
		i++; i++;
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		i++;i++;
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[i++].as<ExpressionAddress>(), B)));
//		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[17].as<ExpressionAddress>(), B)));

		EXPECT_LE(analysis.getNumSets(), 2058);
		EXPECT_LE(analysis.getNumConstraints(), 2322);

//		std::cout << "Num Sets:  " << analysis.getNumSets() << "\n";
//		std::cout << "Num Const: " << analysis.getNumConstraints() << "\n";
//		createDotDump(analysis);
	}

	TEST(CBA, BindExpressionsSimple) {

		// call a higher-order function causing some effect
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"

				// create a counter
				"	decl int<4> x = 2;"
				"	let inc = lambda (int<4> z) => z + x;"

				// create some bindings
				"	inc(3);"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));

	}

	TEST(CBA, BindExpressions_101) {

			// first test for bind expressions - no captured context
			NodeManager mgr;
			IRBuilder builder(mgr);

			auto in = builder.parseStmt(
					"{"
					"	decl auto inc = lambda (int<4> x) => x + 1;"
					"	inc(2);"
					"	inc(4);"
					"}"
			).as<CompoundStmtPtr>();

			ASSERT_TRUE(in);
			CompoundStmtAddress code(in);

			CBA analysis(code);

			EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));
			EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A)));
//			createDotDump(analysis);
	}

	TEST(CBA, BindExpressions_102) {

			// a bind expression capturing a reference
			NodeManager mgr;
			IRBuilder builder(mgr);

			auto in = builder.parseStmt(R"(
					{
						decl ref<int<4>> o = var(1);
                        let f = lambda (int<4> a, ref<int<4>> b)->int<4> { return a + b; };
						decl auto inc = lambda (int<4> x) => f(x,o);
						inc(2);
						o = 2;
						inc(2);
						o = 5;
						inc(3);
					}
			)").as<CompoundStmtPtr>();

			ASSERT_TRUE(in);
			CompoundStmtAddress code(in);

			CBA analysis(code);

			EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A)));
			EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), A)));
			EXPECT_EQ("{8}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), A)));
//			createDotDump(analysis);
	}

	TEST(CBA, BindExpressions_103) {

		// a bind expression causing some side effect
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"

				// create a counter
				"	decl ref<int<4>> c = var(0);"
				"	decl auto inc  = lambda () => { c = c+1; };"
				"	decl auto dec  = lambda () => { c = c-1; };"
				"	decl auto inc2 = lambda (int<4> z) => { c = c+z; };"

				// create some bindings
				"	*c;"
				"	inc();"
				"	*c;"
				"	inc();"
				"	*c;"
				"	dec();"
				"	*c;"
				"	lambda (int<4> z) => { c = c+z; }(4);"
				"	*c;"
				"	inc2(2);"
				"	*c;"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[8].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[10].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[12].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{7}", toString(analysis.getValuesOf(code[14].as<ExpressionAddress>(), A)));
//		createDotDump(analysis);

	}

	TEST(CBA, ContextConstraints) {

		// create a code fragment with dynamic contexts
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
			"{"
			"	decl auto f = lambda (()->unit a)->unit { a(); };"
			"	decl auto g = lambda (()->unit a)->unit { a(); };"
			"	f(lambda ()->unit {});"
			"	f(lambda ()->unit {});"
			"	g(lambda ()->unit {});"
			"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("[0,1,2,3,4,5]", toString(analysis.getDynamicCallLabels()));

		EXPECT_EQ("{3,4}", toString(analysis.getValuesOf(code[0].as<DeclarationStmtAddress>()->getInitialization().as<LambdaExprAddress>()->getBody()[0].as<ExpressionAddress>(), pred)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[1].as<DeclarationStmtAddress>()->getInitialization().as<LambdaExprAddress>()->getBody()[0].as<ExpressionAddress>(), pred)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), pred)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), pred)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), pred)));
//		createDotDump(analysis);
	}

	TEST(CBA, RecursiveCallContexts_1) {

		// create a simple test case containing recursion
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let fac = lambda (int<4> x)->int<4> {"
				"		if (x == 0) return 1;"
				"		return x * fac(x-1);"
				"	};"
				"	"
				"	fac(0);"
				"	fac(1);"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		auto contexts = toString(analysis.getValidContexts<DefaultContext>());

		EXPECT_PRED2(containsSubString, contexts, "[0,0]");
		EXPECT_PRED2(containsSubString, contexts, "[1,1]");
		EXPECT_PRED2(containsSubString, contexts, "[2,2]");

		EXPECT_PRED2(containsSubString, contexts, "[0,1]");
		EXPECT_PRED2(containsSubString, contexts, "[0,2]");

		EXPECT_PRED2(notContainsSubString, contexts, "[1,2]");
		EXPECT_PRED2(notContainsSubString, contexts, "[2,1]");

		EXPECT_PRED2(notContainsSubString, contexts, "[1,0]");
		EXPECT_PRED2(notContainsSubString, contexts, "[2,0]");
	}

	TEST(CBA, RecursiveCallContexts_2) {

		// create a simple test case containing recursion
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let even, odd = "
				"		lambda (int<4> x)->bool {"
				"			if (x == 0) return true;"
				"			return odd(x-1);"
				"		},"
				"		lambda (int<4> x)->bool {"
				"			if (x == 0) return false;"
				"			return even(x-1);"
				"		};"
				"	"
				"	even(0);"
				"	odd(0);"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		auto contexts = toString(analysis.getValidContexts<DefaultContext>());


		EXPECT_PRED2(   containsSubString, contexts, "[0,0]");

		// --- the first call --

		EXPECT_PRED2(notContainsSubString, contexts, "[1,1]");
		EXPECT_PRED2(notContainsSubString, contexts, "[2,2]");

		EXPECT_PRED2(   containsSubString, contexts, "[0,1]");
		EXPECT_PRED2(notContainsSubString, contexts, "[0,2]");

		EXPECT_PRED2(   containsSubString, contexts, "[1,2]");
		EXPECT_PRED2(   containsSubString, contexts, "[2,1]");

		EXPECT_PRED2(notContainsSubString, contexts, "[1,0]");
		EXPECT_PRED2(notContainsSubString, contexts, "[2,0]");

		EXPECT_PRED2(   containsSubString, contexts, "[0,0]");

		// --- and the second call --

		EXPECT_PRED2(notContainsSubString, contexts, "[3,3]");
		EXPECT_PRED2(notContainsSubString, contexts, "[4,4]");

		EXPECT_PRED2(notContainsSubString, contexts, "[0,3]");
		EXPECT_PRED2(   containsSubString, contexts, "[0,4]");

		EXPECT_PRED2(   containsSubString, contexts, "[3,4]");
		EXPECT_PRED2(   containsSubString, contexts, "[4,3]");

		EXPECT_PRED2(notContainsSubString, contexts, "[3,0]");
		EXPECT_PRED2(notContainsSubString, contexts, "[4,0]");
	}

	TEST(CBA, Recursion_1) {

		// create a simple test case containing recursion
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let fac = lambda (int<4> x)->int<4> {"
				"		if (x == 0) return 1;"
				"		return x * fac(x-1);"
				"	};"
				"	"
				"	fac(0);"
				"	fac(1);"
				"	fac(2);"
				"	fac(3);"
				"	fac(4);"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[0].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A)));

		// the analysis of code[3] should contain the unknown value
		lattice<decltype(A)>::type::less_op_type less_op;
		EXPECT_PRED2(less_op, Formula(), analysis.getValuesOf(code[3].as<ExpressionAddress>(), A));

		// but if the accuracy is increased by using a longer call-string it should work
		EXPECT_EQ("{6}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A, Context<3,0>())));

		// yet still not for larger recursive functions
		EXPECT_PRED2(less_op, Formula(), analysis.getValuesOf(code[4].as<ExpressionAddress>(), A, Context<3,0>()));

//		createDotDump(analysis);
	}

	TEST(CBA, Recursion_2) {

		// let's try some mutual recursion
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let even, odd = "
				"		lambda (int<4> x)->bool {"
				"			if (x == 0) return true;"
				"			return odd(x-1);"
				"		},"
				"		lambda (int<4> x)->bool {"
				"			if (x == 0) return false;"
				"			return even(x-1);"
				"		};"
				"	"
				"	even(0);"
				"	even(1);"
				"	even(2);"
				"	even(3);"
				"	odd(0);"
				"	odd(1);"
				"	odd(2);"
				"	odd(3);"
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[0].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), B)));

		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[4].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{0}", toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), B)));
		EXPECT_EQ("{1}", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), B)));

//		createDotDump(analysis);
	}

	TEST(CBA, LocationContext) {

		// some code where the context of a memory allocation is relevant
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	decl auto f = lambda ()->ref<int> { return new(0); };"
				"	"
				"	decl ref<int> a = f();"
				"	decl ref<int> b = f();"
				"	"
				"	a = 4;"
				"	b = 5;"
				"	a;"				// should be a location @ context A
				"	b;"				// should be a location @ context B != A
				"	*a;"			// should be 4
				"	*b;"			// should be 5  - if locations have no context, both would be {5}
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ(
				"{(0-0-1-2-0-1-2-0-0-1-2-0-1-2-0-1,[[0,1],[<0,[0,0],0>,<0,[0,0],0>]],#)}",
				toString(analysis.getValuesOf(code[5].as<ExpressionAddress>(), R))
		);
		EXPECT_EQ(
				"{(0-0-1-2-0-1-2-0-0-1-2-0-1-2-0-1,[[0,2],[<0,[0,0],0>,<0,[0,0],0>]],#)}",
				toString(analysis.getValuesOf(code[6].as<ExpressionAddress>(), R))
		);

		EXPECT_EQ("{4}", toString(analysis.getValuesOf(code[7].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{5}", toString(analysis.getValuesOf(code[8].as<ExpressionAddress>(), A)));

	}


	TEST(CBA, ArgumentSideEffects) {

		// some code where the context of a memory allocation is relevant
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto in = builder.parseStmt(
				"{"
				"	let int = int<4>;"
				"	let inc = lambda (ref<int> a)->int { a = a + 1; return *a; };"
				"	"
				"	decl ref<int> x = new(0);"
				"	"
				"	inc(x) + inc(x);"			// this should be 3 (1 + 2 in any evaluation order)
				"	*x;"						// this should be 2 now
				"}"
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("{3}", toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));
		EXPECT_EQ("{2}", toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}



	TEST(CBA, InitExpressionTests) {

		// some code where the context of a memory allocation is relevant
		NodeManager mgr;
		IRBuilder builder(mgr);

		std::map<string, NodePtr> symbols;
		symbols["c"] = builder.literal("c", builder.getLangBasic().getBool());


		auto in = builder.parseStmt(
				"{"
				"	vector_init_undefined(lit(int<4>),param(10));"
				"	"
				"	vector_init_uniform(10,param(5));"
				"	vector_init_uniform((c?4:5),param(5));"
				"	"
				"	array_create_1D(lit(int<4>),20u);"
				"}", symbols
		).as<CompoundStmtPtr>();

		ASSERT_TRUE(in);
		CompoundStmtAddress code(in);

		CBA analysis(code);

		EXPECT_EQ("[*={-unknown-}]", 	toString(analysis.getValuesOf(code[0].as<ExpressionAddress>(), A)));
		EXPECT_EQ("[*={10}]", 			toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));

		auto value = toString(analysis.getValuesOf(code[2].as<ExpressionAddress>(), A));
		EXPECT_TRUE("[*={4,5}]" == value || "[*={5,4}]" == value);

		EXPECT_EQ("[*={-unknown-}]", 	toString(analysis.getValuesOf(code[3].as<ExpressionAddress>(), A)));

//		createDotDump(analysis);
	}

//	TEST(CBA, IntegerParameter) {
//
//		// some code where the context of a memory allocation is relevant
//		NodeManager mgr;
//		IRBuilder builder(mgr);
//
//		std::map<string, NodePtr> symbols;
//
//		auto in = builder.parseStmt(
//				"{"
//            //    "   let getInt = (intTypeParam<#a> param) -> int<4> { return to.uint(param(#a)); }"
//                "   getInt(6); "
//				"}"
//		).as<CompoundStmtPtr>();
//
//		ASSERT_TRUE(in);
//		CompoundStmtAddress code(in);
//
//        dumpColor(code);
//
//		CBA analysis(code);
//
//
//		EXPECT_EQ("[]", 	toString(analysis.getValuesOf(code[1].as<ExpressionAddress>(), A)));
//
////		createDotDump(analysis);
//	}


} // end namespace cba
} // end namespace analysis
} // end namespace insieme
