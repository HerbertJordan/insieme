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

#include "insieme/core/program.h"
#include "insieme/core/ast_visitor.h"

using namespace insieme::core;

class SimpleVisitor : public ASTVisitor<void> {

public:
	int countGenericTypes;
	int countArrayTypes;
	int countExpressions;
	int countRefTypes;

	SimpleVisitor() : ASTVisitor<void>(true), countGenericTypes(0), countArrayTypes(0), countExpressions(0), countRefTypes(0) {};

public:
	void visitGenericType(const GenericTypePtr& cur) {
		countGenericTypes++;
	}

	void visitExpression(const ExpressionPtr& cur) {
		countExpressions++;
	}

	void visitArrayType(const ArrayTypePtr& cur) {
		countArrayTypes++;
	}

	void visitRefType(const RefTypePtr& cur) {
		countRefTypes++;

		// forward processing
		visitGenericType(cur);
	}
};

TEST(ASTVisitor, DispatcherTest) {

	NodeManager manager;
	SimpleVisitor visitor;

	ProgramPtr program = Program::create(manager);

	EXPECT_EQ ( 0, visitor.countArrayTypes );
	EXPECT_EQ ( 0, visitor.countExpressions );
	EXPECT_EQ ( 0, visitor.countGenericTypes );
	EXPECT_EQ ( 0, visitor.countRefTypes );

	visitor.visit(program);

	EXPECT_EQ ( 0, visitor.countArrayTypes );
	EXPECT_EQ ( 0, visitor.countExpressions );
	EXPECT_EQ ( 0, visitor.countGenericTypes );
	EXPECT_EQ ( 0, visitor.countRefTypes );


	GenericTypePtr type = GenericType::get(manager, "int");
	visitor.visit(type);

	EXPECT_EQ ( 0, visitor.countArrayTypes );
	EXPECT_EQ ( 0, visitor.countExpressions );
	EXPECT_EQ ( 1, visitor.countGenericTypes );
	EXPECT_EQ ( 0, visitor.countRefTypes );

	auto intType = manager.basic.getInt16();
	visitor.visit(intType);

	EXPECT_EQ ( 0, visitor.countArrayTypes );
	EXPECT_EQ ( 0, visitor.countExpressions );
	EXPECT_EQ ( 2, visitor.countGenericTypes );
	EXPECT_EQ ( 0, visitor.countRefTypes );

	LiteralPtr literal = Literal::get(manager, type, "3");
	visitor.visit(literal);

	EXPECT_EQ ( 0, visitor.countArrayTypes );
	EXPECT_EQ ( 1, visitor.countExpressions );
	EXPECT_EQ ( 2, visitor.countGenericTypes );
	EXPECT_EQ ( 0, visitor.countRefTypes );

	ArrayTypePtr arrayType = ArrayType::get(manager, type);
	visitor.visit(arrayType);

	EXPECT_EQ ( 1, visitor.countArrayTypes );
	EXPECT_EQ ( 1, visitor.countExpressions );
	EXPECT_EQ ( 2, visitor.countGenericTypes );
	EXPECT_EQ ( 0, visitor.countRefTypes );

	RefTypePtr refType = RefType::get(manager, type);
	visitor.visit(refType);

	EXPECT_EQ ( 1, visitor.countArrayTypes );
	EXPECT_EQ ( 1, visitor.countExpressions );
	EXPECT_EQ ( 3, visitor.countGenericTypes );
	EXPECT_EQ ( 1, visitor.countRefTypes );
}


class CountingVisitor : public ASTVisitor<int> {
public:

	int counter;

	CountingVisitor(bool countTypes=true)
		: ASTVisitor<int>(countTypes), counter(0) {};

	int visitNode(const NodePtr& node) {
		// std::cout << *node << std::endl;
		return ++counter;
	};

	void reset() {
		counter = 0;
	}

};

class CountingAddressVisitor : public ASTVisitor<int, Address> {
public:

	int counter;

	CountingAddressVisitor(bool countTypes=true)
		: ASTVisitor<int, Address>(countTypes), counter(0) {};

	int visitNode(const NodeAddress& address) {
		return ++counter;
	};

	void reset() {
		counter = 0;
	}

};


TEST(ASTVisitor, RecursiveVisitorTest) {

	// TODO: run recursive visitor test

	NodeManager manager;
	CountingVisitor visitor(true);
	auto recVisitor = makeDepthFirstVisitor(visitor);

	ProgramPtr program = Program::create(manager);

	visitor.reset();
	visitor.visit(program);
	EXPECT_EQ ( 1, visitor.counter );

	visitor.reset();
	recVisitor.visit(program);
	EXPECT_EQ ( 1, visitor.counter );


	GenericTypePtr type = GenericType::get(manager, "int");
	visitor.visit(type);

	visitor.reset();
	visitor.visit(program);
	EXPECT_EQ ( 1, visitor.counter );

	visitor.reset();
	recVisitor.visit(program);
	EXPECT_EQ ( 1, visitor.counter );

	GenericTypePtr type2 = GenericType::get(manager, "int", toVector<TypePtr>(type, type), toVector<IntTypeParamPtr>(VariableIntTypeParam::get(manager, 'p')), type);

	visitor.reset();
	visitor.visit(type2);
	EXPECT_EQ ( 1, visitor.counter );

	visitor.reset();
	recVisitor.visit(type2);
	EXPECT_EQ ( 5, visitor.counter );

	IfStmtPtr ifStmt = IfStmt::get(manager,
		Literal::get(manager, type, "12"),
		Literal::get(manager, type, "14"),
		CompoundStmt::get(manager)
	);

	visitor.reset();
	visitor.visit(ifStmt);
	EXPECT_EQ ( 1, visitor.counter );

	visitor.reset();
	recVisitor.visit(ifStmt);
	EXPECT_EQ ( 7, visitor.counter );


	// ------ test for addresses ----
	CountingAddressVisitor adrVisitor(true);
	auto recAdrVisitor = makeDepthFirstVisitor(adrVisitor);

	adrVisitor.reset();
	adrVisitor.visit(NodeAddress(ifStmt));
	EXPECT_EQ ( 1, adrVisitor.counter );

	adrVisitor.reset();
	recAdrVisitor.visit(NodeAddress(ifStmt));
	EXPECT_EQ ( 7, adrVisitor.counter );


	// test without types
	CountingVisitor noTypePtrVisitor(false);
	auto recNoTypeVisitor = makeDepthFirstVisitor(noTypePtrVisitor);

	noTypePtrVisitor.reset();
	noTypePtrVisitor.visit(ifStmt);
	EXPECT_EQ ( 1, noTypePtrVisitor.counter );

	noTypePtrVisitor.reset();
	recNoTypeVisitor.visit(ifStmt);
	EXPECT_EQ ( 5, noTypePtrVisitor.counter );

	CountingAddressVisitor noTypeAdrVisitor(false);
	auto recNoTypeAdrVisitor = makeDepthFirstVisitor(noTypeAdrVisitor);

	noTypeAdrVisitor.reset();
	noTypeAdrVisitor.visit(NodeAddress(ifStmt));
	EXPECT_EQ ( 1, noTypeAdrVisitor.counter );

	noTypeAdrVisitor.reset();
	recNoTypeAdrVisitor.visit(NodeAddress(ifStmt));
	EXPECT_EQ ( 5, noTypeAdrVisitor.counter );


}

TEST(ASTVisitor, BreadthFirstASTVisitorTest) {

	// TODO: run recursive visitor test

	NodeManager manager;
	CountingVisitor visitor;

	// create a Test CASE
	TypePtr typeD = GenericType::get(manager, "D");
	TypePtr typeE = GenericType::get(manager, "E");
	TypePtr typeF = GenericType::get(manager, "F");

	TypePtr typeB = GenericType::get(manager, "B", toVector(typeD, typeE));
	TypePtr typeC = GenericType::get(manager, "C", toVector(typeF));

	TypePtr typeA = GenericType::get(manager, "A", toVector(typeB, typeC));


	// create a resulting list
	vector<NodePtr> res;

	// create a visitor collecting all nodes
	auto collector = makeLambdaVisitor([&res](const NodePtr& cur) {
		res.push_back(cur);
	}, true);

	auto breadthVisitor = makeBreadthFirstVisitor(collector);

	breadthVisitor.visit(typeA);
	vector<NodePtr> expected;
	expected.push_back(typeA);
	expected.push_back(typeB);
	expected.push_back(typeC);
	expected.push_back(typeD);
	expected.push_back(typeE);
	expected.push_back(typeF);

	EXPECT_EQ ( toString(expected), toString(res));
	EXPECT_TRUE ( equals(expected, res));

	res.clear();
	EXPECT_TRUE ( equals(vector<NodePtr>(), res) );
	visitBreadthFirst(typeA, collector);
	EXPECT_TRUE ( equals(expected, res));

	res.clear();
	EXPECT_TRUE ( equals(vector<NodePtr>(), res) );
	visitBreadthFirst(typeA, [&res](const NodePtr& cur) {
		res.push_back(cur);
	}, true);
	EXPECT_TRUE ( equals(expected, res));

}


TEST(ASTVisitor, VisitOnceASTVisitorTest) {

	NodeManager manager;


	// build a simple type sharing nodes
	TypePtr shared = GenericType::get(manager, "shared");
	NodePtr type = TupleType::get(manager, toVector(shared, shared));

	// create a resulting list
	vector<NodePtr> res;

	// create a visitor collecting all nodes
	auto collector = makeLambdaVisitor([&res](const NodePtr& cur) {
		res.push_back(cur);
	}, true);

	// visit all recursively
	res.clear();
	auto recursive = makeDepthFirstVisitor(collector);
	recursive.visit(type);

	EXPECT_TRUE ( equals(toVector<NodePtr>(type, shared, shared), res));

	// visit all, only once
	res.clear();
	auto prefix = makeDepthFirstOnceVisitor(collector);
	prefix.visit(type);
	EXPECT_TRUE ( equals(toVector<NodePtr>(type, shared), res));

	res.clear();
	auto postfix = makeDepthFirstOnceVisitor(collector, false);
	postfix.visit(type);

	EXPECT_TRUE ( equals(toVector<NodePtr>(shared, type), res));

}

TEST(ASTVisitor, UtilitiesTest) {

	NodeManager manager;


	// build a simple type sharing nodes
	TypePtr shared = GenericType::get(manager, "shared");
	NodePtr type = TupleType::get(manager, toVector(shared, shared));

	// create a resulting list
	vector<NodePtr> res;

	// create a visitor collecting all nodes
	auto collector = makeLambdaVisitor([&res](const NodePtr& cur) {
		res.push_back(cur);
	}, true);

	// visit all recursively
	res.clear();
	visitDepthFirst(type, collector);
	EXPECT_TRUE ( equals(toVector<NodePtr>(type, shared, shared), res));

	// visit all, only once
	res.clear();
	visitDepthFirstOnce(type, collector);
	EXPECT_TRUE ( equals(toVector<NodePtr>(type, shared), res));

	res.clear();
	visitDepthFirstOnce(type, collector, false);
	EXPECT_TRUE ( equals(toVector<NodePtr>(shared, type), res));


	auto fun = [&res](const NodePtr& cur) {
		res.push_back(cur);
	};

	// visit all recursively
	res.clear();
	visitDepthFirst(type, fun, true, false);
	EXPECT_TRUE ( equals(toVector<NodePtr>(), res));

	res.clear();
	visitDepthFirst(type, fun, true, true);
	EXPECT_TRUE ( equals(toVector<NodePtr>(type, shared, shared), res));

	res.clear();
	visitDepthFirst(type, fun, false, true);
	EXPECT_TRUE ( equals(toVector<NodePtr>(shared, shared, type), res));

	// visit all, only once
	res.clear();
	visitDepthFirstOnce(type, fun, true, true);
	EXPECT_TRUE ( equals(toVector<NodePtr>(type, shared), res));

	res.clear();
	visitDepthFirstOnce(type, fun, true, true);
	EXPECT_TRUE ( equals(toVector<NodePtr>(type, shared), res));

	res.clear();
	visitDepthFirstOnce(type, fun, false, true);
	EXPECT_TRUE ( equals(toVector<NodePtr>(shared, type), res));

}

template<template<class Target> class Ptr>
class InterruptingVisitor : public ASTVisitor<bool,Ptr> {
public:

	int counter;
	int limit;

	InterruptingVisitor(int limit) : ASTVisitor<bool,Ptr>(true), counter(0), limit(limit) {};

	bool visitNode(const Ptr<const Node>& node) {
		return !(++counter < limit);
	};

	void reset() {
		counter = 0;
	}

};

TEST(ASTVisitor, RecursiveInterruptableVisitorTest) {

	// TODO: run recursive visitor test

	NodeManager manager;
	InterruptingVisitor<Pointer> limit3(3);
	InterruptingVisitor<Pointer> limit10(10);

	GenericTypePtr type = GenericType::get(manager, "int");

	IfStmtPtr ifStmt = IfStmt::get(manager,
		Literal::get(manager, type, "12"),
		Literal::get(manager, type, "14"),
		CompoundStmt::get(manager)
	);

	limit3.reset();
	EXPECT_TRUE(visitDepthFirstInterruptable(ifStmt, limit3));
	EXPECT_EQ ( 3, limit3.counter );

	limit10.reset();
	EXPECT_FALSE(visitDepthFirstInterruptable(ifStmt, limit10));
	EXPECT_EQ ( 7, limit10.counter );

	// ------ test for addresses ----
	InterruptingVisitor<Address> limitA3(3);
	InterruptingVisitor<Address> limitA10(10);

	limitA3.reset();
	EXPECT_TRUE(visitDepthFirstInterruptable(NodeAddress(ifStmt), limitA3));
	EXPECT_EQ ( 3, limitA3.counter );

	limitA10.reset();
	visitDepthFirstInterruptable(ifStmt, limit10);
	EXPECT_FALSE(visitDepthFirstInterruptable(NodeAddress(ifStmt), limitA10));
	EXPECT_EQ ( 7, limitA10.counter );
}


TEST(ASTVisitor, VisitOnceInterruptableVisitorTest) {

	// TODO: run recursive visitor test

	NodeManager manager;
	InterruptingVisitor<Pointer> limit3(3);
	InterruptingVisitor<Pointer> limit10(10);

	GenericTypePtr type = GenericType::get(manager, "int");

	IfStmtPtr ifStmt = IfStmt::get(manager,
		Literal::get(manager, type, "12"),
		Literal::get(manager, type, "14"),
		CompoundStmt::get(manager)
	);

	limit3.reset();
	EXPECT_TRUE(visitDepthFirstOnceInterruptable(ifStmt, limit3));
	EXPECT_EQ ( 3, limit3.counter );

	limit10.reset();
	EXPECT_FALSE(visitDepthFirstOnceInterruptable(ifStmt, limit10));
	EXPECT_EQ ( 6, limit10.counter );

	// check number of nodes when visiting all nodes
	limit10.reset();
	visitDepthFirstOnce(ifStmt, limit10);
	EXPECT_EQ( 6, limit10.counter);

	// ------ test for addresses ----
	InterruptingVisitor<Address> limitA3(3);
	InterruptingVisitor<Address> limitA10(10);

	limitA3.reset();
	EXPECT_TRUE(visitDepthFirstOnceInterruptable(NodeAddress(ifStmt), limitA3));
	EXPECT_EQ ( 3, limitA3.counter );

	limitA10.reset();
	EXPECT_FALSE(visitDepthFirstOnceInterruptable(NodeAddress(ifStmt), limitA10));
	EXPECT_EQ ( 6, limitA10.counter );

	// check number of nodes when visiting all nodes
	limitA10.reset();
	visitDepthFirstOnce(NodeAddress(ifStmt), limitA10);
	EXPECT_EQ( 6, limitA10.counter);
}


class PruningVisitor : public ASTVisitor<bool,Address> {
public:

	int counter;
	int depthLimit;

	PruningVisitor(int depthLimit) : ASTVisitor<bool,Address>(true), counter(0), depthLimit(depthLimit) {};

	bool visitNode(const NodeAddress& node) {
		counter++;
		return ! (node.getDepth() < (std::size_t)depthLimit);
	};

	void reset() {
		counter = 0;
	}

};

TEST(ASTVisitor, RecursivePrunableVisitorTest) {

	NodeManager manager;
	GenericTypePtr type = GenericType::get(manager, "int");

	IfStmtPtr ifStmt = IfStmt::get(manager,
		Literal::get(manager, type, "12"),
		Literal::get(manager, type, "14"),
		CompoundStmt::get(manager)
	);

	// ------ test for addresses ----
	PruningVisitor limitA(1);
	PruningVisitor limitB(2);

	limitA.reset();
	visitDepthFirstPrunable(NodeAddress(ifStmt), limitA);
	EXPECT_EQ ( 1, limitA.counter );

	limitB.reset();
	visitDepthFirstPrunable(NodeAddress(ifStmt), limitB);
	EXPECT_EQ ( 4, limitB.counter );
}


TEST(ASTVisitor, VisitOncePrunableVisitorTest) {

	NodeManager manager;

	GenericTypePtr type = GenericType::get(manager, "int");

	IfStmtPtr ifStmt = IfStmt::get(manager,
		Literal::get(manager, type, "12"),
		Literal::get(manager, type, "14"),
		CompoundStmt::get(manager)
	);

	// ------ test for addresses ----
	PruningVisitor limitA(1);
	PruningVisitor limitB(2);

	limitA.reset();
	visitDepthFirstOncePrunable(NodeAddress(ifStmt), limitA);
	EXPECT_EQ ( 1, limitA.counter );

	limitB.reset();
	visitDepthFirstOncePrunable(NodeAddress(ifStmt), limitB);
	EXPECT_EQ ( 4, limitB.counter );

	// check number of nodes when visiting all nodes
	limitB.reset();
	visitDepthFirstOnce(NodeAddress(ifStmt), limitB);
	EXPECT_EQ( 6, limitB.counter);
}


TEST(ASTVisitor, SingleTypeLambdaVisitor) {

	NodeManager manager;

	GenericTypePtr type = GenericType::get(manager, "int");

	IfStmtPtr ifStmt = IfStmt::get(manager,
		Literal::get(manager, type, "12"),
		Literal::get(manager, type, "14"),
		CompoundStmt::get(manager)
	);

	// ------ test for addresses ----

	{
		int counter = 0;
		auto visitor = makeLambdaVisitor([&counter](const LiteralPtr& cur) {
			counter++;
		});
		visitDepthFirst(ifStmt, visitor);
		EXPECT_EQ(counter, 2);
	}

	{
		int counter = 0;
		auto visitor = makeLambdaVisitor([&counter](const CompoundStmtPtr& cur) {
			counter++;
		});
		visitDepthFirst(ifStmt, visitor);
		EXPECT_EQ(counter, 2);
	}

}


bool filterLiteral(const LiteralPtr& cur) {
	return cur->getValue() == "14";
}

TEST(ASTVisitor, FilteredLambdaVisitor) {

	NodeManager manager;

	GenericTypePtr type = GenericType::get(manager, "int");

	IfStmtPtr ifStmt = IfStmt::get(manager,
		Literal::get(manager, type, "12"),
		Literal::get(manager, type, "14"),
		CompoundStmt::get(manager)
	);

	// ------ test for addresses ----

	auto filter =[](const LiteralPtr& cur) {
		return cur->getValue() == "12";
	};

	{
		int counter = 0;
		auto visitor = makeLambdaVisitor(filter,[&counter](const LiteralPtr& cur) {
			counter++;
		});
		visitDepthFirst(ifStmt, visitor);
		EXPECT_EQ(counter, 1);
	}

	{
		// without filter
		int counter = 0;
		auto visitor = makeLambdaVisitor([&counter](const LiteralPtr& cur) {
			counter++;
		});
		visitDepthFirst(ifStmt, visitor);
		EXPECT_EQ(counter, 2);
	}

	{
		// with reject-all filter
		int counter = 0;
		auto visitor = makeLambdaVisitor(RejectAll<const LiteralPtr&>(), [&counter](const LiteralPtr& cur) {
			counter++;
		});
		visitDepthFirst(ifStmt, visitor);
		EXPECT_EQ(counter, 0);
	}
}


TEST(ASTVisitor, ParameterTest) {

	NodeManager manager;

	GenericTypePtr type = GenericType::get(manager, "int");

	IfStmtPtr ifStmt = IfStmt::get(manager,
		Literal::get(manager, type, "12"),
		Literal::get(manager, type, "14"),
		CompoundStmt::get(manager)
	);

	auto visitor = makeLambdaVisitor([](const NodePtr& cur, int& a, int& b){
		a++;
		b--;
	}, true);


	int n = 0;
	int m = 1;
	visitor.visit(ifStmt, n, m);
	EXPECT_EQ(1, n);
	EXPECT_EQ(0, m);

	n = 0;
	m = 0;
	auto recVisitor = makeDepthFirstVisitor(visitor);
	recVisitor.visit(ifStmt, n, m);
	EXPECT_EQ(7, n);
	EXPECT_EQ(-7, m);

	// this should work - but it does not ...
//	visitAllP(type, visitor, false, n, m);


}


