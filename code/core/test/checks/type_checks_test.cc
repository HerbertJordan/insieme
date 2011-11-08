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

#include "insieme/core/ir_builder.h"
#include "insieme/core/checks/typechecks.h"

namespace insieme {
namespace core {
namespace checks {

bool containsMSG(const MessageList& list, const Message& msg) {
	return contains(list.getAll(), msg);
}


TEST(CallExprTypeCheck, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);
	const lang::BasicGenerator& basic = manager.getLangBasic();

	// OK ... create some types
	TypePtr int2 = basic.getInt2();
	TypePtr int4 = basic.getInt4();
	TypePtr intA = basic.getIntGen();

	// ... define some types
	TupleTypePtr empty = builder.tupleType(toVector<TypePtr>());
	EXPECT_EQ("()", toString(*empty));
	FunctionTypePtr nullary = builder.functionType(empty->getElementTypes(), intA);
	EXPECT_EQ("(()->int<#a>)", toString(*nullary));
	TupleTypePtr single = builder.tupleType(toVector(intA));
	EXPECT_EQ("(int<#a>)", toString(*single));
	FunctionTypePtr unary = builder.functionType(single->getElementTypes(), intA);
	EXPECT_EQ("((int<#a>)->int<#a>)", toString(*unary));
	TupleTypePtr pair = builder.tupleType(toVector(intA, intA));
	EXPECT_EQ("(int<#a>,int<#a>)", toString(*pair));
	FunctionTypePtr binary = builder.functionType(pair->getElementTypes(), intA);
	EXPECT_EQ("((int<#a>,int<#a>)->int<#a>)", toString(*binary));

	// define literals
	LiteralPtr x = builder.literal(int2, "1");
	EXPECT_EQ("1", toString(*x));

	LiteralPtr y = builder.literal(int4, "2");
	EXPECT_EQ("2", toString(*y));

	LiteralPtr constFun = builder.literal(nullary, "zero");
	LiteralPtr unaryFun = builder.literal(unary, "succ");
	LiteralPtr binaryFun = builder.literal(binary, "sum");


	ExpressionPtr expr = constFun;

	CheckPtr typeCheck = make_check<CallExprTypeCheck>();

	// correct examples ...
	expr = builder.callExpr(intA, constFun, toVector<ExpressionPtr>());
	EXPECT_EQ("[]", toString(check(expr, typeCheck)));

	expr = builder.callExpr(int2, unaryFun, toVector<ExpressionPtr>(x));
	EXPECT_EQ("[]", toString(check(expr, typeCheck)));

	expr = builder.callExpr(int4, unaryFun, toVector<ExpressionPtr>(y));
	EXPECT_EQ("[]", toString(check(expr, typeCheck)));

	expr = builder.callExpr(int2, binaryFun, toVector<ExpressionPtr>(x,x));
	EXPECT_EQ("[]", toString(check(expr, typeCheck)));

	expr = builder.callExpr(int4, binaryFun, toVector<ExpressionPtr>(x,y));
	EXPECT_EQ("[]", toString(check(expr, typeCheck)));

	expr = builder.callExpr(int4, binaryFun, toVector<ExpressionPtr>(y,y));
	EXPECT_EQ("[]", toString(check(expr, typeCheck)));

	// invalid return type
	MessageList issues;
	expr = builder.callExpr(int2, unaryFun, toVector<ExpressionPtr>(y));
	issues = check(expr, typeCheck);
	EXPECT_EQ((std::size_t)1, issues.size());
	EXPECT_PRED2(containsMSG, issues, Message(NodeAddress(expr), EC_TYPE_INVALID_RETURN_TYPE, "", Message::ERROR));

	expr = builder.callExpr(intA, binaryFun, toVector<ExpressionPtr>(x,y));
	issues = check(expr, typeCheck);
	EXPECT_EQ((std::size_t)1, issues.size());
	EXPECT_PRED2(containsMSG, issues, Message(NodeAddress(expr), EC_TYPE_INVALID_RETURN_TYPE, "", Message::ERROR));

	expr = builder.callExpr(intA, binaryFun, toVector<ExpressionPtr>(y,y));
	issues = check(expr, typeCheck);
	EXPECT_EQ((std::size_t)1, issues.size());
	EXPECT_PRED2(containsMSG, issues, Message(NodeAddress(expr), EC_TYPE_INVALID_RETURN_TYPE, "", Message::ERROR));

	// invalid argument types
	TypePtr concreteType2 = builder.genericType("int", toVector<TypePtr>(), toVector<IntTypeParamPtr>(ConcreteIntTypeParam::get(manager, 2)));
	LiteralPtr z = builder.literal(concreteType2, "3");
	EXPECT_EQ("3", toString(*z));

	TypePtr boolType = builder.genericType("bool");
	LiteralPtr w = builder.literal(boolType, "true");
	EXPECT_EQ("true", toString(*w));

	// => not unifyable arguments (but forming a sub-type) ...
	expr = builder.callExpr(int4, binaryFun, toVector<ExpressionPtr>(y,z));
	issues = check(expr, typeCheck);
	EXPECT_TRUE(issues.empty()) << issues;

	// => not unifyable arguments
	expr = builder.callExpr(intA, binaryFun, toVector<ExpressionPtr>(y,w));
	issues = check(expr, typeCheck);
	EXPECT_EQ((std::size_t)1, issues.size());
	EXPECT_PRED2(containsMSG, issues, Message(NodeAddress(expr), EC_TYPE_INVALID_ARGUMENT_TYPE, "", Message::ERROR));

	// => wrong number of arguments
	expr = builder.callExpr(intA, binaryFun, toVector<ExpressionPtr>(x));
	issues = check(expr, typeCheck);
	EXPECT_EQ((std::size_t)1, issues.size());
	EXPECT_PRED2(containsMSG, issues, Message(NodeAddress(expr), EC_TYPE_INVALID_NUMBER_OF_ARGUMENTS, "", Message::ERROR));

	expr = builder.callExpr(intA, binaryFun, toVector<ExpressionPtr>(x,y,z));
	issues = check(expr, typeCheck);
	EXPECT_EQ((std::size_t)1, issues.size());
	EXPECT_PRED2(containsMSG, issues, Message(NodeAddress(expr), EC_TYPE_INVALID_NUMBER_OF_ARGUMENTS, "", Message::ERROR));
}

TEST(StructExprTypeCheck, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// some preparations
	core::StringValuePtr name = builder.stringValue("x");
	core::TypePtr typeA = builder.genericType("A");
	core::TypePtr typeB = builder.genericType("B");

	core::ExpressionPtr valueA = builder.literal(typeA, "a");
	core::ExpressionPtr valueB = builder.literal(typeB, "b");

	NamedTypeList entries;
	entries.push_back(builder.namedType(name, typeA));
	core::StructTypePtr structType = builder.structType(entries);


	// create struct expression
	NamedValueList members;
	members.push_back(builder.namedValue(name, valueA));
	core::StructExprPtr ok = builder.structExpr(structType, members);

	members.clear();
	members.push_back(builder.namedValue(name, valueB));
	core::StructExprPtr err = builder.structExpr(structType, members);

	// conduct checks
	CheckPtr typeCheck = make_check<StructExprTypeCheck>();

	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_INVALID_INITIALIZATION_EXPR, "", Message::ERROR));
}

TEST(MemberAccessElementTypeCheck, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);
	const lang::BasicGenerator& basic = builder.getLangBasic();

	// get function to be tested
	LiteralPtr fun = basic.getCompositeMemberAccess();

	// Create a example expressions
	TypePtr typeA = builder.genericType("typeA");
	TypePtr typeB = builder.genericType("typeB");
	TypePtr typeC = builder.genericType("typeC");

	StringValuePtr identA = builder.stringValue("a");
	StringValuePtr identB = builder.stringValue("b");
	StringValuePtr identC = builder.stringValue("c");

	NamedTypeList entries;
	entries.push_back(builder.namedType(identA, typeA));
	entries.push_back(builder.namedType(identB, typeB));

	TypePtr structType = builder.structType(entries);
	VariablePtr var = builder.variable(structType);
	VariablePtr var2 = builder.variable(typeA);

	ExpressionPtr ok = builder.callExpr(fun, var, builder.getIdentifierLiteral(identA), builder.getTypeLiteral(typeA));
	ExpressionPtr err1 = builder.callExpr(fun, var, builder.getIdentifierLiteral(identA), builder.getTypeLiteral(typeB));
	ExpressionPtr err2 = builder.callExpr(fun, var, builder.getIdentifierLiteral(identC), builder.getTypeLiteral(typeB));
	ExpressionPtr err3 = builder.callExpr(fun, var2, builder.getIdentifierLiteral(identA), builder.getTypeLiteral(typeA));
	ExpressionPtr err4 = builder.callExpr(fun, var2, var, builder.getTypeLiteral(typeA));


	CheckPtr typeCheck = make_check<MemberAccessElementTypeCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err1, typeCheck).empty());
	ASSERT_FALSE(check(err2, typeCheck).empty());
	ASSERT_FALSE(check(err3, typeCheck).empty());
	ASSERT_FALSE(check(err4, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err1,typeCheck), Message(NodeAddress(err1), EC_TYPE_INVALID_TYPE_OF_MEMBER, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err2,typeCheck), Message(NodeAddress(err2), EC_TYPE_NO_SUCH_MEMBER, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err3,typeCheck), Message(NodeAddress(err3), EC_TYPE_ACCESSING_MEMBER_OF_NON_NAMED_COMPOSITE_TYPE, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err4,typeCheck), Message(NodeAddress(err4), EC_TYPE_INVALID_IDENTIFIER, "", Message::ERROR));
}

TEST(MemberAccessElementTypeCheck, References) {
	NodeManager manager;
	IRBuilder builder(manager);
	const lang::BasicGenerator& basic = builder.getLangBasic();

	// get function to be tested
	LiteralPtr fun = basic.getCompositeRefElem();

	// Create a example expressions
	TypePtr typeA = builder.genericType("typeA");
	TypePtr typeB = builder.genericType("typeB");
	TypePtr typeC = builder.genericType("typeC");

	TypePtr typeRefA = builder.refType(typeA);
	TypePtr typeRefB = builder.refType(typeB);
	TypePtr typeRefC = builder.refType(typeC);

	StringValuePtr identA = builder.stringValue("a");
	StringValuePtr identB = builder.stringValue("b");
	StringValuePtr identC = builder.stringValue("c");

	NamedCompositeType::Entries entries;
	entries.push_back(builder.namedType(identA, typeA));
	entries.push_back(builder.namedType(identB, typeB));

	TypePtr structType = builder.structType(entries);
	TypePtr structRefType = builder.refType(structType);

	VariablePtr var = builder.variable(structRefType);
	VariablePtr var2 = builder.variable(typeRefA);

	ExpressionPtr ok = builder.callExpr(fun, var, builder.getIdentifierLiteral(identA), builder.getTypeLiteral(typeA));
	ExpressionPtr err1 = builder.callExpr(fun, var, builder.getIdentifierLiteral(identA), builder.getTypeLiteral(typeB));
	ExpressionPtr err2 = builder.callExpr(fun, var, builder.getIdentifierLiteral(identC), builder.getTypeLiteral(typeB));
	ExpressionPtr err3 = builder.callExpr(fun, var2, builder.getIdentifierLiteral(identA), builder.getTypeLiteral(typeA));
	ExpressionPtr err4 = builder.callExpr(fun, var2, var, builder.getTypeLiteral(typeA));


	CheckPtr typeCheck = make_check<MemberAccessElementTypeCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err1, typeCheck).empty());
	ASSERT_FALSE(check(err2, typeCheck).empty());
	ASSERT_FALSE(check(err3, typeCheck).empty());
	ASSERT_FALSE(check(err4, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err1,typeCheck), Message(NodeAddress(err1), EC_TYPE_INVALID_TYPE_OF_MEMBER, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err2,typeCheck), Message(NodeAddress(err2), EC_TYPE_NO_SUCH_MEMBER, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err3,typeCheck), Message(NodeAddress(err3), EC_TYPE_ACCESSING_MEMBER_OF_NON_NAMED_COMPOSITE_TYPE, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err4,typeCheck), Message(NodeAddress(err4), EC_TYPE_INVALID_IDENTIFIER, "", Message::ERROR));
}

TEST(ReturnTypeCheck, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);
	const lang::BasicGenerator& basic = manager.getLangBasic();

	// create a function type (for all those functions)
	TypePtr resultType = basic.getInt4();
	FunctionTypePtr funType = builder.functionType(TypeList(), resultType);

	// create a function where everything is correct
	StatementPtr body = builder.returnStmt(builder.literal(resultType, "1"));
	LambdaPtr ok = builder.lambda(funType, VariableList(), body);

	// create a function where return type is wrong
	body = builder.returnStmt(builder.literal(basic.getInt2(), "1"));
	LambdaPtr err = builder.lambda(funType, VariableList(), body);

	// create a function where return type is wrong - and nested
	body = builder.returnStmt(builder.literal(basic.getInt2(), "1"));
	body = builder.compoundStmt(body);
	LambdaPtr err2 = builder.lambda(funType, VariableList(), body);


	CheckPtr returnTypeCheck = make_check<ReturnTypeCheck>();
	EXPECT_TRUE(check(ok, returnTypeCheck).empty());
	ASSERT_FALSE(check(err, returnTypeCheck).empty());
	ASSERT_FALSE(check(err2, returnTypeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err,returnTypeCheck), Message(NodeAddress(err).getAddressOfChild(2,0), EC_TYPE_INVALID_RETURN_VALUE_TYPE, "", Message::ERROR));
}

TEST(DeclarationStmtTypeCheck, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create a function literal
	TypePtr type = builder.genericType("int");
	TypePtr type2 = builder.genericType("uint");
	ExpressionPtr init = builder.literal(type, "4");
	DeclarationStmtPtr ok = builder.declarationStmt(builder.variable(type), init);
	DeclarationStmtPtr err = builder.declarationStmt(builder.variable(type2), init);

	CheckPtr typeCheck = make_check<DeclarationStmtTypeCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_INVALID_INITIALIZATION_EXPR, "", Message::ERROR));
}

TEST(IfCondition, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create a function literal
	TypePtr intType = builder.genericType("int");
	TypePtr boolType = builder.genericType("bool");
	ExpressionPtr intLit = builder.literal(intType, "4");
	ExpressionPtr boolLit = builder.literal(boolType, "true");
	NodePtr ok = builder.ifStmt(boolLit, builder.getNoOp());
	NodePtr err = builder.ifStmt(intLit, builder.getNoOp());

	CheckPtr typeCheck = make_check<IfConditionTypeCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_INVALID_CONDITION_EXPR, "", Message::ERROR));
}

TEST(WhileCondition, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create a function literal
	TypePtr intType = builder.genericType("int");
	TypePtr boolType = builder.genericType("bool");
	ExpressionPtr intLit = builder.literal(intType, "4");
	ExpressionPtr boolLit = builder.literal(boolType, "true");
	NodePtr ok = builder.whileStmt(boolLit, builder.getNoOp());
	NodePtr err = builder.whileStmt(intLit, builder.getNoOp());

	CheckPtr typeCheck = make_check<WhileConditionTypeCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_INVALID_CONDITION_EXPR, "", Message::ERROR));
}

TEST(Switch, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create a function literal
	TypePtr intType = builder.getLangBasic().getInt1();
	TypePtr boolType = builder.genericType("bool");
	ExpressionPtr intLit = builder.literal(intType, "4");
	ExpressionPtr boolLit = builder.literal(boolType, "true");
	SwitchStmtPtr ok = builder.switchStmt(intLit, vector<SwitchCasePtr>());
	SwitchStmtPtr err = builder.switchStmt(boolLit, vector<SwitchCasePtr>());

	CheckPtr typeCheck = make_check<SwitchExpressionTypeCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_INVALID_SWITCH_EXPR, "", Message::ERROR));
}

TEST(BuildInLiterals, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create a function literal

	LiteralPtr ok = builder.getLangBasic().getFalse();
	LiteralPtr err = builder.literal(builder.genericType("strangeType"), ok->getValue());

	CheckPtr typeCheck = make_check<BuiltInLiteralCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_INVALID_TYPE_OF_LITERAL, "", Message::WARNING));
}

TEST(RefCastExpr, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create a function literal

	TypePtr type = builder.genericType("int");
	TypePtr ref = builder.refType(type);

	CastExprPtr ok = builder.castExpr(type, builder.literal(type, "1"));
	CastExprPtr err1 = builder.castExpr(ref, builder.literal(type, "1"));
	CastExprPtr err2 = builder.castExpr(type, builder.literal(ref, "1"));
	CastExprPtr err3 = builder.castExpr(builder.refType(type), builder.literal(builder.refType(ref), "1"));

	CheckPtr typeCheck = make_check<RefCastCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err1, typeCheck).empty());
	ASSERT_FALSE(check(err2, typeCheck).empty());
	ASSERT_FALSE(check(err3, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err1,typeCheck), Message(NodeAddress(err1), EC_TYPE_NON_REF_TO_REF_CAST, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err2,typeCheck), Message(NodeAddress(err2), EC_TYPE_REF_TO_NON_REF_CAST, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err3,typeCheck), Message(NodeAddress(err3), EC_TYPE_REF_TO_NON_REF_CAST, "", Message::ERROR));
}

TEST(CastExpr, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create a function literal

	TypePtr type = builder.genericType("int");
	TypePtr ref = builder.refType(type);
	TypePtr fun = builder.functionType(toVector(type), ref);

	CastExprPtr ok = builder.castExpr(type, builder.literal(type, "1"));
	CastExprPtr err1 = builder.castExpr(fun, builder.literal(type, "1"));
	CastExprPtr err2 = builder.castExpr(type, builder.literal(fun, "1"));
	CastExprPtr err3 = builder.castExpr(builder.refType(fun), builder.literal(builder.refType(ref), "1"));

	CheckPtr typeCheck = make_check<CastCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err1, typeCheck).empty());
	ASSERT_FALSE(check(err2, typeCheck).empty());
	ASSERT_FALSE(check(err3, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err1,typeCheck), Message(NodeAddress(err1), EC_TYPE_ILLEGAL_CAST, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err2,typeCheck), Message(NodeAddress(err2), EC_TYPE_ILLEGAL_CAST, "", Message::ERROR));
	EXPECT_PRED2(containsMSG, check(err3,typeCheck), Message(NodeAddress(err3), EC_TYPE_ILLEGAL_CAST, "", Message::ERROR));
}

TEST(KeywordCheck, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create correct and wrong instances

	TypePtr element = builder.genericType("A");
	IntTypeParamPtr param = builder.concreteIntTypeParam(8);

	CheckPtr typeCheck = make_check<KeywordCheck>();

	// test vector
	{
		TypePtr ok = builder.vectorType(element, param);
		TypePtr err = builder.genericType("vector", toVector<TypePtr>(element), toVector<IntTypeParamPtr>(param));

		EXPECT_FALSE(*ok == *err);

		EXPECT_TRUE(check(ok, typeCheck).empty());
		EXPECT_FALSE(check(err, typeCheck).empty());

		EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_ILLEGAL_USE_OF_TYPE_KEYWORD, "", Message::WARNING));
	}

	// test array
	{
		TypePtr ok = builder.arrayType(element, param);
		TypePtr err = builder.genericType("array", toVector<TypePtr>(element), toVector<IntTypeParamPtr>(param));

		EXPECT_FALSE(*ok == *err);

		EXPECT_TRUE(check(ok, typeCheck).empty());
		EXPECT_FALSE(check(err, typeCheck).empty());

		EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_ILLEGAL_USE_OF_TYPE_KEYWORD, "", Message::WARNING));
	}

	// test references
	{
		TypePtr ok = builder.refType(element);
		TypePtr err = builder.genericType("ref", toVector<TypePtr>(element));

		EXPECT_TRUE(check(ok, typeCheck).empty());
		EXPECT_FALSE(check(err, typeCheck).empty());

		EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_ILLEGAL_USE_OF_TYPE_KEYWORD, "", Message::WARNING));
	}

	// test channel
	{
		TypePtr ok = builder.channelType(element, param);
		TypePtr err = builder.genericType("channel", toVector<TypePtr>(element), toVector<IntTypeParamPtr>(param));

		EXPECT_FALSE(*ok == *err);

		EXPECT_TRUE(check(ok, typeCheck).empty());
		EXPECT_FALSE(check(err, typeCheck).empty());

		EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_ILLEGAL_USE_OF_TYPE_KEYWORD, "", Message::WARNING));
	}
}

TEST(ExternalFunctionType, Basic) {
	NodeManager manager;
	IRBuilder builder(manager);

	// OK ... create a function literal
	TypePtr intType = builder.genericType("int");
	TypePtr boolType = builder.genericType("bool");
	FunctionTypePtr funTypeOK  = builder.functionType(toVector(intType), boolType, true);
	FunctionTypePtr funTypeERR = builder.functionType(toVector(intType), boolType, false);

	NodePtr ok = builder.literal(funTypeOK, "fun");
	NodePtr err = builder.literal(funTypeERR, "fun");

	CheckPtr typeCheck = make_check<ExternalFunctionTypeCheck>();
	EXPECT_TRUE(check(ok, typeCheck).empty());
	ASSERT_FALSE(check(err, typeCheck).empty());

	EXPECT_PRED2(containsMSG, check(err,typeCheck), Message(NodeAddress(err), EC_TYPE_INVALID_FUNCTION_TYPE, "", Message::ERROR));
}


} // end namespace checks
} // end namespace core
} // end namespace insieme
