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

#include "insieme/transform/pattern/ir_pattern.h"
#include "insieme/core/parser/ir_parse.h"

#include "insieme/utils/logging.h"

#ifndef TEST
// avoiding warnings in eclipse, enabling auto completions
#define TEST void fun
#endif

using namespace insieme::utils::log;

namespace insieme {
using namespace core;
	
namespace transform {
namespace pattern {

bool isMatch(const TreePatternPtr& pattern, const NodePtr& node) {
	return details::match(pattern, node);
}

bool noMatch(const TreePatternPtr& pattern, const NodePtr& node) {
	return !isMatch(pattern, node);
}

TEST(IRConvert, Basic) {
	NodeManager manager;
	auto t = [&manager](string typespec) { return parse::parseType(manager, typespec); };
	
	TypePtr tupleA = t("(int<4>,float<8>,uint<1>)");
	TypePtr tupleB = t("(int<4>,float<8>)");


	TreePatternPtr patternA = aT(atom(t("float<8>")));
	EXPECT_PRED2(isMatch, patternA, tupleA);
	TreePatternPtr patternB = aT(atom(t("uint<8>")));
	EXPECT_PRED2(noMatch, patternB, tupleA);
	TreePatternPtr patternC = irp::tupleType(any << atom(t("float<8>")) << any);
	EXPECT_PRED2(isMatch, patternC, tupleA);
	EXPECT_PRED2(noMatch, patternC, tupleB);

	TreePatternPtr patternD = irp::tupleType(*any << atom(t("float<8>")) << *any);
	EXPECT_PRED2(isMatch, patternD, tupleA);
	EXPECT_PRED2(isMatch, patternD, tupleB);

	TreePatternPtr patternE = irp::tupleType(*any << atom(t("uint<1>")) << *any);
	EXPECT_PRED2(isMatch, patternE, tupleA);
	EXPECT_PRED2(noMatch, patternE, tupleB);
}


TEST(IRPattern, Types) {
	NodeManager manager;
	auto pt = [&manager](const string& str) { return parse::parseType(manager, str); };
	
	TypePtr int8Type = pt("int<8>");
	TypePtr genericA = pt("megatype<ultratype<int<8>,666>>");
	TypePtr genericB = pt("megatype<ultratype<int<8>,B>>");
	
	TreePatternPtr patternA = irp::genericType("megatype", single(any));
	EXPECT_PRED2(noMatch, patternA, int8Type);
	EXPECT_PRED2(isMatch, patternA, genericA);

	TreePatternPtr patternB = irp::genericType("ultratype", any << any);
	EXPECT_PRED2(noMatch, patternB, int8Type);
	EXPECT_PRED2(noMatch, patternB, genericA);
	EXPECT_PRED2(noMatch, aT(patternB), int8Type);
	EXPECT_PRED2(noMatch, aT(patternB), genericA);
	EXPECT_PRED2(isMatch, aT(patternB), genericB);

	TreePatternPtr patternC = irp::genericType("ultratype", single(any), single(any));
	EXPECT_PRED2(noMatch, patternC, int8Type);
	EXPECT_PRED2(noMatch, patternC, genericA);
	EXPECT_PRED2(noMatch, aT(patternC), int8Type);
	EXPECT_PRED2(isMatch, aT(patternC), genericA);
	EXPECT_PRED2(noMatch, aT(patternC), genericB);
}


TEST(IRPattern, literal) {
	NodeManager manager;
	auto pe = [&manager](string str) { return parse::parseExpression(manager, str); };

	ExpressionPtr exp1 = pe("(4.2 + 3.1)");
	ExpressionPtr exp2 = pe("(4 - 2)");
	ExpressionPtr exp3 = pe("((4.2 + 3.1) * (4 - 2))");
	
	TreePatternPtr patternA = irp::callExpr(manager.getLangBasic().getRealAdd(), *any);
	EXPECT_PRED2(isMatch, patternA, exp1);
	EXPECT_PRED2(noMatch, patternA, exp2);
	EXPECT_PRED2(noMatch, patternA, exp3);
	
	TreePatternPtr patternB = node(any << irp::literal("int.sub") << *any);
	EXPECT_PRED2(noMatch, patternB, exp1);
	EXPECT_PRED2(isMatch, patternB, exp2);
	EXPECT_PRED2(noMatch, patternB, exp3);
}

TEST(IRPattern, variable) {
	NodeManager manager;
	auto at = [&manager](string str) { return irp::atom(manager, str); };

	StatementPtr var1 = parse::parseExpression(manager, "int<8>:x");

	TreePatternPtr pattern1 = irp::variable(at("int<8>"), any);
	EXPECT_PRED2(isMatch, pattern1, var1);
}

TEST(IRPattern, lambdaExpr) {
	NodeManager manager;
	auto at = [&manager](string str) { return irp::atom(manager, str); };

	ExpressionPtr exp1 = parse::parseExpression(manager, "fun (int<4>:i, int<8>:v) -> int<4> { return i }");

	TreePatternPtr pattern1 = irp::lambdaExpr(any, irp::lambdaDefinition(*any));
	EXPECT_PRED2(isMatch, pattern1, exp1);
}

TEST(IRPattern, lambda) {
	NodeManager manager;
	auto at = [&manager](string str) { return irp::atom(manager, str); };

	NodePtr node = parse::parseIR(manager, "(int<8>:i, int<8>:v) -> int<4> { return i }");

	TreePatternPtr pattern1 = irp::lambda(any, irp::variable(var("x"), any) << irp::variable(var("x"), any), any);
	EXPECT_PRED2(isMatch, pattern1, node);
}

TEST(IRPattern, callExpr) {
	NodeManager manager;
	auto at = [&manager](string str) { return irp::atom(manager, str); };

	ExpressionPtr exp1 = parse::parseExpression(manager, "( 4 + 5 )");

	TreePatternPtr pattern1 = irp::callExpr(any, irp::literal(any, any) , *any);
	EXPECT_PRED2(isMatch, pattern1, exp1);
}

TEST(IRPattern, declarationStmt) {
	NodeManager manager;
	auto at = [&manager](string str) { return irp::atom(manager, str); };

	StatementPtr stmt1 = parse::parseStatement(manager, "decl int<4>:i = 3");

	TreePatternPtr pattern1 = irp::declarationStmt(any, any);
	TreePatternPtr pattern2 = irp::declarationStmt(irp::variable(at("int<4>"), any), at("3"));
	EXPECT_PRED2(isMatch, pattern1, stmt1);
	EXPECT_PRED2(isMatch, pattern2, stmt1);
}

TEST(IRPattern, ifStmt) {
	NodeManager manager;
	auto at = [&manager](string str) { return irp::atom(manager, str); };
	auto ps = [&manager](string str) { return parse::parseStatement(manager, str); };

	StatementPtr stmt1 = ps("if(0) { return 0; } else { return (1+2); }");
	StatementPtr stmt2 = ps("if(0) { return 0; }");
	StatementPtr stmt3 = ps("if((1 != 0)) { return 0; }");

	TreePatternPtr pattern1 = irp::ifStmt(any, at("{ return 0; }"), irp::returnStmt(any));
	TreePatternPtr pattern2 = irp::ifStmt(any, irp::returnStmt(any), irp::returnStmt(any));
	TreePatternPtr pattern3 = irp::ifStmt(any, irp::returnStmt(at("1")|at("0")), irp::returnStmt(any));
	TreePatternPtr pattern4 = irp::ifStmt(any, at("{ return 0; }"), any);
	TreePatternPtr pattern5 = irp::ifStmt(any, at("{ return 0; }"), at("{ }"));
	TreePatternPtr pattern6 = irp::ifStmt(at("(1 != 0)"), at("{ return 0; }"), at("{ }"));

	EXPECT_PRED2(isMatch, pattern1, stmt1);
	EXPECT_PRED2(isMatch, pattern2, stmt1);
	EXPECT_PRED2(isMatch, pattern3, stmt1);
	EXPECT_PRED2(isMatch, pattern4, stmt1);
	EXPECT_PRED2(noMatch, pattern5, stmt1);
	EXPECT_PRED2(noMatch, pattern6, stmt1);

	EXPECT_PRED2(noMatch, pattern1, stmt2);
	EXPECT_PRED2(noMatch, pattern2, stmt2);
	EXPECT_PRED2(noMatch, pattern3, stmt2);
	EXPECT_PRED2(isMatch, pattern4, stmt2);
	EXPECT_PRED2(isMatch, pattern5, stmt2);
	EXPECT_PRED2(noMatch, pattern6, stmt2);

	EXPECT_PRED2(noMatch, pattern1, stmt3);
	EXPECT_PRED2(noMatch, pattern2, stmt3);
	EXPECT_PRED2(noMatch, pattern3, stmt3);
	EXPECT_PRED2(isMatch, pattern4, stmt3);
	EXPECT_PRED2(isMatch, pattern5, stmt3);
	EXPECT_PRED2(isMatch, pattern6, stmt3);
}

TEST(IRPattern, forStmt) {
	NodeManager manager;
	auto at = [&manager](string str) { return irp::atom(manager, str); };
	auto ps = [&manager](string str) { return parse::parseStatement(manager, str); };

	StatementPtr stmt1 = ps("for(decl int<4>:i = 30 .. 5 : -5) { decl int<4>:i = 3;}");
	StatementPtr stmt2 = ps("for(decl int<4>:i = 0 .. 10 : 2) { return 0; }");
	StatementPtr stmt3 = ps("for(decl int<4>:i = 0 .. 5 : 1) { 7; 6; continue; 8; }");
	StatementPtr stmt4 = ps("for(decl int<4>:i = 0 .. 2 : 1) { for(decl int<4>:i = 0 .. 2 : 1){ return 0; }; }");

	TreePatternPtr pattern1 = irp::forStmt(var("x"), any, at("5"), at("-5"), irp::declarationStmt(var("x"), at("3")));
	TreePatternPtr pattern2 = irp::forStmt(any, any, at("10"), at("2"), irp::returnStmt(at("0")));
	TreePatternPtr pattern3 = irp::forStmt(any, any, at("5"), at("1"), irp::compoundStmt(*any << irp::continueStmt << any));
	TreePatternPtr pattern4 = irp::forStmt(var("i"), var("x"), var("y"), var("z"), irp::compoundStmt(irp::forStmt(var("i"), var("x"), var("y"), var("z"), irp::compoundStmt(*any)) << *any));

	EXPECT_PRED2(isMatch, pattern1, stmt1);
	EXPECT_PRED2(isMatch, pattern2, stmt2);
	EXPECT_PRED2(isMatch, pattern3, stmt3);
	EXPECT_PRED2(isMatch, pattern4, stmt4);
}

TEST(IRPattern, Addresses) {
	NodeManager manager;
	auto at = [&manager](string str) { return irp::atom(manager, str); };
	auto ps = [&manager](string str) { return StatementAddress(parse::parseStatement(manager, str)); };

	StatementAddress stmt1 = ps("for(decl int<4>:i = 30 .. 5 : -5) { decl int<4>:i = 3;}");
	StatementAddress stmt2 = ps("for(decl int<4>:i = 0 .. 2 : 1) { for(decl int<4>:i = 0 .. 2 : 1){ 7; return 0; }; }");

	TreePatternPtr pattern1 = irp::forStmt(var("x"), any, at("5"), at("-5"), irp::declarationStmt(var("x"), at("3")));
	TreePatternPtr pattern2 = irp::forStmt(var("i"), var("x"), var("y"), var("z"), irp::compoundStmt(irp::forStmt(var("i"), var("x"), var("y"), var("z"), irp::compoundStmt(*var("b", any))) << *any));

	// addresses always point to first encounter
	auto match = pattern1->matchAddress(stmt1);
	EXPECT_TRUE(match);
	EXPECT_EQ(stmt1.getAddressOfChild(0,0), match->getVarBinding("x").getValue());

	match = pattern2->matchAddress(stmt2);
	EXPECT_TRUE(match);
	EXPECT_EQ(stmt2.getAddressOfChild(0,0), match->getVarBinding("i").getValue());
	EXPECT_EQ(stmt2.getAddressOfChild(0,1), match->getVarBinding("x").getValue());
	EXPECT_EQ(stmt2.getAddressOfChild(1), match->getVarBinding("y").getValue());
	EXPECT_EQ(stmt2.getAddressOfChild(2), match->getVarBinding("z").getValue());

	EXPECT_EQ(
			toString(toVector<NodeAddress>(
					stmt2.getAddressOfChild(3,0,3,0),
					stmt2.getAddressOfChild(3,0,3,1)
			)),
			toString(match->getVarBinding("b").getList())
	);
}

//TEST(IRPattern, whileStmt) {
//	NodeManager manager;
//	auto at = [&manager](string str) { return irp::atom(manager, str); };
//	auto ps = [&manager](string str) { return parse::parseStatement(manager, str); };
//
//	StatementPtr stmt1 = ps(" { decl ref<int<4>>:i = 30; while((i < 40)){ (i++); }; }");
//	StatementPtr stmt2 = ps(" while((ref<int<4>>:i < 40)){ { }; { decl int<4>:b = (i++);}; }");
//
//	auto tree1 = toTree(stmt1);
//	auto tree2 = toTree(stmt2);
//
//	TreePatternPtr pattern1 = irp::compoundStmt(irp::declarationStmt(var("x"), at("30")) << irp::whileStmt(irp::callExpr(irp::literal("int.lt", any),
//								irp::callExpr(irp::literal("ref.deref", any), var("x") << *any) << *any), irp::compoundStmt(*any << irp::callExpr(any, var("x") << *any))));
//	TreePatternPtr pattern2 = irp::whileStmt(irp::callExpr(irp::literal("int.lt", any), irp::callExpr(irp::literal("ref.deref", any), var("x") << *any) << *any),
//											 aT(irp::declarationStmt(any, aT(var("x")))));
//
//	/*MatchOpt mo = pattern1->isMatch(tree1);
//	if (mo){
//		MatchValue mv = mo->getVarBinding("x");
//		std::cout << mv.getTree()->getAttachedValue<NodePtr>();
//	}*/
//
//	EXPECT_PRED2(isMatch, pattern1, tree1) << *stmt1;
//	EXPECT_PRED2(isMatch, pattern2, tree2) << *stmt2;
//}


TEST(IRPattern, AbitrarilyNestedLoop) {

	// create a pattern for an arbitrarily nested loop
	auto pattern = rT(var("loops", irp::forStmt(var("iter",any), any, any, any, aT(recurse) | aT(!irp::forStmt()))));

	//EXPECT_EQ("", toString(pattern));

	// create loops and test the pattern
	NodeManager mgr;

	// not perfectly nested loop


	auto match = pattern->matchPointer(core::parse::parseStatement(mgr, "\
			for(decl uint<4>:i = 10 .. 50 : 1) { \
				(op<array.ref.elem.1D>(ref<array<uint<4>,1>>:v, i)); \
				for(decl uint<4>:j = 5 .. 25 : 1) { \
					(op<array.ref.elem.1D>(ref<array<uint<4>,1>>:v, (i+j))); \
				}; \
			}"));
    EXPECT_TRUE(match);
    EXPECT_EQ(2u, match->getVarBinding("loops").getList().size());
    EXPECT_EQ(2u, match->getVarBinding("iter").getList().size());


    match = pattern->matchPointer(core::parse::parseStatement(mgr, "\
    		for(decl uint<4>:k = 1 .. 6 : 1) { \
    		    (op<array.ref.elem.1D>(ref<array<uint<4>,1>>:v, k)); \
				for(decl uint<4>:i = 10 .. 50 : 1) { \
					(op<array.ref.elem.1D>(ref<array<uint<4>,1>>:v, i)); \
					for(decl uint<4>:j = 5 .. 25 : 1) { \
						(op<array.ref.elem.1D>(ref<array<uint<4>,1>>:v, (i+j))); \
					}; \
					(op<array.ref.elem.1D>(ref<array<uint<4>,1>>:v, i)); \
			    }; \
    		}"));
    EXPECT_TRUE(match);
    EXPECT_EQ(3u, match->getVarBinding("loops").getList().size());
    EXPECT_EQ(3u, match->getVarBinding("iter").getList().size());

}

} // end namespace pattern
} // end namespace transform
} // end namespace insieme

