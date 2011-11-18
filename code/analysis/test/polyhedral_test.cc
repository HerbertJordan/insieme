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

#include "insieme/analysis/polyhedral/iter_vec.h"
#include "insieme/analysis/polyhedral/affine_func.h"
#include "insieme/analysis/polyhedral/constraint.h"
#include "insieme/analysis/polyhedral/polyhedral.h"

#include "insieme/core/ir_program.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_statements.h"
#include "insieme/core/ir_builder.h"

using namespace insieme::core;
using namespace insieme::analysis;

#define CREATE_ITER_VECTOR \
	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); \
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2); \
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); \
	\
	poly::IterationVector iterVec; \
	\
	iterVec.add( poly::Iterator(iter1) ); \
	EXPECT_EQ(static_cast<size_t>(2), iterVec.size()); \
	iterVec.add( poly::Parameter(param) ); \
	EXPECT_EQ(static_cast<size_t>(3), iterVec.size()); \
	iterVec.add( poly::Iterator(iter2) ); \
	EXPECT_EQ(static_cast<size_t>(4), iterVec.size()); \

TEST(IterationVector, Creation) {
	
	NodeManager mgr;
	CREATE_ITER_VECTOR;
	EXPECT_EQ(static_cast<size_t>(4), iterVec.size());
	{
		std::ostringstream ss;
		iterVec.printTo(ss);
		EXPECT_EQ("(v1,v2|v3|1)", ss.str());
	}
	EXPECT_TRUE( iterVec[0] == poly::Iterator(iter1) );
	EXPECT_FALSE( iterVec[0] == poly::Parameter(iter1) );

	poly::IterationVector iterVec2;

	iterVec2.add( poly::Parameter(param) );
	iterVec2.add( poly::Iterator(iter1) ); 
	iterVec2.add( poly::Iterator(iter2) );

	for (size_t it = 0; it < iterVec.size(); ++it ) {
		EXPECT_TRUE( iterVec[it] == iterVec2[it]);
	}
	{
		std::ostringstream ss;
		ss << iterVec2;
		EXPECT_EQ("(v1,v2|v3|1)", ss.str());
	}
}

TEST(IterationVector, Iterator) {
	
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::IterationVector::iterator it = iterVec.begin();
	EXPECT_EQ(*(it+1), poly::Iterator(iter2));
	EXPECT_EQ(*(it+2), poly::Parameter(param));
	EXPECT_EQ(*(it+3), poly::Constant());

}

TEST(IterationVector, MergeEmpty) {
	NodeManager mgr;

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2); 
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); 
	
	poly::IterationVector iterVec1( { iter1, iter2 } ) ; 
//	iterVec1.add( poly::Parameter(param) ); 

	poly::IterationVector iterVec2; 

	poly::IterationVector itv = poly::merge(iterVec1, iterVec2);
	
	poly::IterationVector::iterator it = itv.begin();
	EXPECT_EQ(poly::Iterator(iter1), *(it++));
	EXPECT_EQ(poly::Iterator(iter2), *(it++));
//	EXPECT_EQ(poly::Parameter(param), *(it++));
	EXPECT_EQ(poly::Constant(), *it);

	poly::IterationVector itv2 = poly::merge(iterVec2, itv);
	// std::cout << itv;
	poly::IterationVector::iterator it2 = itv2.begin();
	EXPECT_EQ(poly::Iterator(iter1), *(it2++));
	EXPECT_EQ(poly::Iterator(iter2), *(it2++));
// 	EXPECT_EQ(poly::Parameter(param), *(it2++));
	EXPECT_EQ(poly::Constant(), *it2);

}

TEST(IterationVector, MergeNotEmpty) {
	NodeManager mgr;

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2); 
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); 
	
	poly::IterationVector iterVec1; 
	iterVec1.add( poly::Iterator(iter1) ); 
	iterVec1.add( poly::Parameter(param) ); 
	std::cout << iterVec1 << std::endl;

	poly::IterationVector iterVec2; 
	iterVec2.add( poly::Parameter(param) ); 
	iterVec2.add( poly::Iterator(iter2) ); 
	std::cout << iterVec2 << std::endl; 

	poly::IterationVector itv = poly::merge(iterVec1, iterVec2);
	std::cout << itv << std::endl;
	poly::IterationVector::iterator it = itv.begin();
	EXPECT_EQ(poly::Iterator(iter1), *(it++));
	EXPECT_EQ(poly::Iterator(iter2), *(it++));
	EXPECT_EQ(poly::Parameter(param), *(it++));
	EXPECT_EQ(poly::Constant(), *it);
}

TEST(IterVec, Transform) {
	NodeManager mgr;

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2); 
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); 
	
	poly::IterationVector iterVec1( { iter1 }, { param } ); 
	iterVec1.add( poly::Iterator(iter1) ); 
	iterVec1.add( poly::Parameter(param) ); 
	// std::cout << iterVec1 << std::endl;
	
	VariablePtr iter3 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 4); 
	VariablePtr param2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 5); 
	
	poly::IterationVector iterVec2( { iter3, iter1, iter2 }, { param2, param } ); 
	// std::cout << iterVec2 << std::endl;
	
	const poly::IndexTransMap&& transMap = poly::transform(iterVec2, iterVec1);
	EXPECT_EQ(transMap, poly::IndexTransMap({1,4,5}));
	
}

//==== AffineFunction =========================================================

TEST(AffineFunction, Creation) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec);
	af.setCoeff(poly::Iterator(iter1), 0);
	af.setCoeff(poly::Parameter(param),2);
	af.setCoeff(poly::Iterator(iter2), 1);
	af.setCoeff(poly::Constant(), 10);

	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("1*v2 + 2*v3 + 10*1", ss.str());
	}

	EXPECT_EQ(0, af.getCoeff(iter1));
	EXPECT_EQ(2, af.getCoeff(param));
	EXPECT_EQ(1, af.getCoeff(iter2));
	EXPECT_EQ(10, af.getCoeff(poly::Constant()));

	VariablePtr param2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 4); 	
	iterVec.add(poly::Parameter(param2));

	EXPECT_EQ(0, af.getCoeff(param2));
	EXPECT_EQ(0, af.getCoeff(iter1));
	EXPECT_EQ(2, af.getCoeff(param));
	EXPECT_EQ(1, af.getCoeff(iter2));
	EXPECT_EQ(10, af.getCoeff(poly::Constant()));

	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("1*v2 + 2*v3 + 10*1", ss.str());
	}

	// convertion to IR 
	ExpressionPtr expr = poly::toIR(mgr, af);
	EXPECT_EQ(expr->toString(), "int.add(int.add(v2, int.mul(2, v3)), 10)");
}

TEST(AffineFunction, CreationFromExpr) {
	NodeManager mgr;
   
   	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1);
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2);
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3);

	CallExprPtr sum = CallExpr::get(mgr, mgr.getLangBasic().getInt4(), mgr.getLangBasic().getSignedIntAdd(), 
			toVector<ExpressionPtr>(iter1, param) 
		);
			
	poly::IterationVector iterVec; 
	iterVec.add( poly::Iterator(iter1) );

	poly::AffineFunction af(iterVec, sum);

	EXPECT_EQ(1, af.getCoeff(iter1));
	EXPECT_EQ(1, af.getCoeff(param));
	EXPECT_EQ(0, af.getCoeff(poly::Constant()));

	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("1*v1 + 1*v3", ss.str());
	}

	iterVec.add( poly::Iterator(iter2) );
	VariablePtr param2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 4); 
	iterVec.add(poly::Parameter(param2));

	EXPECT_EQ(1, af.getCoeff(iter1));
	EXPECT_EQ(0, af.getCoeff(iter2));
	EXPECT_EQ(1, af.getCoeff(param));
	EXPECT_EQ(0, af.getCoeff(param2));
	EXPECT_EQ(0, af.getCoeff(poly::Constant()));

	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("1*v1 + 1*v3", ss.str());
	}
}

TEST(AffineFunction, ToExpr) {
	NodeManager mgr;
   
   	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1);
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2);
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3);

	CallExprPtr sum = CallExpr::get(mgr, mgr.getLangBasic().getInt4(), mgr.getLangBasic().getSignedIntAdd(), 
			toVector<ExpressionPtr>(iter1, param) 
		);
			
	poly::IterationVector iterVec; 
	iterVec.add( poly::Iterator(iter1) );
	poly::AffineFunction af(iterVec, sum);

	ExpressionPtr expr = poly::toIR(mgr, af);

	EXPECT_EQ(expr->toString(), "int.add(v1, v3)");

	poly::AffineFunction af2(iterVec, expr);
	EXPECT_EQ(*expr, *toIR(mgr,af2));
}

TEST(AffineFunction, Equality) {
	NodeManager mgr;

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2); 
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); 
	
	poly::IterationVector iterVec1; 
	iterVec1.add( poly::Iterator(iter1) ); 
	iterVec1.add( poly::Parameter(iter2) ); 
	
	poly::AffineFunction af1(iterVec1, {1,1,0});
	
	poly::IterationVector iterVec2; 
	iterVec2.add( poly::Iterator(iter2) ); 
	iterVec2.add( poly::Parameter(iter1) ); 
		
	poly::AffineFunction af2(iterVec2, {1,1,0});
	
	EXPECT_NE(af1, af2);
}

TEST(AffineFunction, AddIter) {
	NodeManager mgr;

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2); 
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); 
	
	poly::IterationVector iterVec1( { iter1 }, { param } ); 
	
	poly::AffineFunction af1(iterVec1, {1,1,0});
	iterVec1.add( poly::Iterator(iter2) );
	
	af1.setCoeff(iter2, 2);
	
	EXPECT_EQ( af1.getCoeff(iter2), 2 );
}

TEST(AffineFunction, AddParam) {
	NodeManager mgr;

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); 
	VariablePtr param1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2); 
	VariablePtr param2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); 
	
	poly::IterationVector iterVec1( { iter1 }, { param1 }); 
	
	poly::AffineFunction af1(iterVec1, {1,1,0});
	iterVec1.add( poly::Parameter(param2) );
	
	af1.setCoeff(param2, 2);
	
	EXPECT_EQ( af1.getCoeff(param2), 2 );
}


TEST(AffineFunction, AFChangeBase) {
	NodeManager mgr;

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2); 
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); 
	
	poly::IterationVector iterVec1( { iter2, iter1 }, { param }); 
	// std::cout << iterVec1 << std::endl;
	
	poly::AffineFunction af1(iterVec1, { 0,1,1,9 } );
	
	VariablePtr iter3 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 4); 
	VariablePtr param2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 5); 
	
	poly::IterationVector iterVec2( { iter3, iter1, iter2 }, { param2, param } ) ; 
	// std::cout << iterVec2 << std::endl;
	
	const poly::IndexTransMap&& transMap = poly::transform(iterVec2, iterVec1);
	EXPECT_EQ(transMap, poly::IndexTransMap( { 2,1,4,5 } ));
	
	poly::AffineFunction aft = af1.toBase(iterVec2, transMap);
	EXPECT_EQ(af1, aft);
	
	iterVec1.add( poly::Iterator(iter3) );
	poly::AffineFunction af2(iterVec1, {0,1,0,1,9} );
	EXPECT_EQ(af2, aft);
	
	af2.setCoeff(poly::Iterator(iter3), 3);
	EXPECT_NE(af2, aft);
}

// Constraints ======================================================================================

TEST(Constraint, Creation) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec, {0,1,2,10});
	poly::Constraint<poly::AffineFunction> c(af, poly::Constraint<poly::AffineFunction>::EQ);
	{
		std::ostringstream ss;
		c.printTo(ss);
		EXPECT_EQ("1*v2 + 2*v3 + 10*1 == 0", ss.str());
	}
}

TEST(Constraint, Normalization) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec, {0,1,2,10});
	poly::Constraint<poly::AffineFunction> c(af, poly::Constraint<poly::AffineFunction>::LT);
	{
		std::ostringstream ss;
		c.printTo(ss);
		EXPECT_EQ("1*v2 + 2*v3 + 10*1 < 0", ss.str());
	}
	poly::ConstraintCombinerPtr<poly::AffineFunction> nc = normalize(c);
	{
		std::ostringstream ss;
		nc->printTo(ss);
		EXPECT_EQ("(-1*v2 + -2*v3 + -11*1 >= 0)", ss.str());
	}
}

TEST(Constraint, Combiner) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec, {0,1,2,10});
	poly::Constraint<poly::AffineFunction> c1(af, poly::Constraint<poly::AffineFunction>::EQ);
	EXPECT_EQ(toIR(mgr,c1)->toString(), 
			"int.le(int.add(int.add(v2, int.mul(2, v3)), 10), 0)"
		);

	poly::AffineFunction af2(iterVec, {2,3,0,10});
	poly::Constraint<poly::AffineFunction> c2(af2, poly::Constraint<poly::AffineFunction>::LT);
	EXPECT_EQ(toIR(mgr,c2)->toString(), 
			"int.le(int.add(int.add(int.mul(2, v1), int.mul(3, v2)), 10), 0)"
		);

	poly::ConstraintCombinerPtr<poly::AffineFunction> ptr = c1 or not_(c2);

	ExpressionPtr expr = toIR(mgr, ptr);
	EXPECT_EQ(expr->toString(), 
		"bool.or(int.le(int.add(int.add(v2, int.mul(2, v3)), 10), 0), bind(){rec v3.{v3=fun(int<4> v1, int<4> v2) {return bool.not(int.le(int.add(int.add(int.mul(2, v1), int.mul(3, v2)), 10), 0));}}(v1, v2)})");
	
}

TEST(IterationDomain, Creation) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec,  { 0, 1, 2, 10 });
	poly::AffineFunction af2(iterVec, { 1, 1, 0,  7 });
	poly::AffineFunction af3(iterVec, { 1, 0, 1,  0 });

	poly::ConstraintCombinerPtr<poly::AffineFunction> cl = 
		poly::Constraint<poly::AffineFunction>(af, poly::Constraint<poly::AffineFunction>::LT) and 
		poly::Constraint<poly::AffineFunction>(af2, poly::Constraint<poly::AffineFunction>::LT) and 
		poly::Constraint<poly::AffineFunction>(af3, poly::Constraint<poly::AffineFunction>::NE);
	{
		std::ostringstream ss;
		ss << iterVec;
		EXPECT_EQ("(v1,v2|v3|1)", ss.str());
	}

	poly::IterationDomain it(cl);
	VariablePtr param2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 4); 
	iterVec.add(poly::Parameter(param2));
	EXPECT_EQ(static_cast<size_t>(5), iterVec.size());

	{
		std::ostringstream ss;
		ss << iterVec;
		EXPECT_EQ("(v1,v2|v3,v4|1)", ss.str());
	}

	// check weather these 2 affine functions are the same... even thought the
	// underlying iteration vector has been changed
	// EXPECT_EQ(af, (*it.begin()).getAffineFunction());
}


TEST(AffineFunction, ChangeBase) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec, { 0, 1, 2, 10 } );
	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("1*v2 + 2*v3 + 10*1", ss.str());
	}

	poly::IterationVector iterVec1( { iter1, param, iter2 } ); 
	// std::cout << iterVec1 << std::endl;

	const poly::IndexTransMap&& map = poly::transform(iterVec1, iterVec);
	EXPECT_EQ(map, poly::IndexTransMap( {0,2,1,3} ));

	poly::AffineFunction&& converted = af.toBase(iterVec1, map);
	{
		std::ostringstream ss;
		converted.printTo(ss);
		EXPECT_EQ("2*v3 + 1*v2 + 10*1", ss.str());
	}

	poly::AffineFunction&& converted2 = af.toBase(iterVec1);
	EXPECT_EQ(converted, converted2);

}

TEST(Transformations, Interchange) {
	NodeManager mgr;
	IRBuilder builder(mgr);

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4()); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4()); 
	
	VariablePtr var = Variable::get(mgr, mgr.getLangBasic().getInt4());
	ExpressionPtr arrAcc = builder.callExpr( 
		mgr.getLangBasic().getArrayRefElem1D(), 
		builder.callExpr( 
			mgr.getLangBasic().getArrayRefElem1D(), 
			builder.variable( builder.arrayType(builder.arrayType(mgr.getLangBasic().getInt4())) ),
			iter1
		),
		iter2
	);

	StatementPtr stmt = builder.callExpr( mgr.getLangBasic().getRefAssign(), var, arrAcc );
	
	// std::cout << "STMT: " << *stmt << std::endl;

	// Build the Iteration Vector
	poly::IterationVector iterVec( { iter1, iter2 } );  // (i,j,1)

	// DOMAIN
	// v1 >= 0 && v1 <= 100
	// v2 >= 0 && v2 <= 100
	poly::IterationDomain domain( iterVec, { {  1, 0,   0 },     	// v1 >= 0
								  			 { -1, 0, 100 }, 		// -v1 + 100 >= 0
										     {  0, 1,   0 },		// v2 >= 0
											 {  0,-1, 100 } } );	// -v2 + 100 >= 0

	// std::cout << "DOM: " << domain << std::endl;
	poly::AffineSystem sched(iterVec, { {1, 0, 0}, 
									    {0, 1, 0} } );

	poly::Scop scop(iterVec);
	scop.push_back( poly::Stmt( 0, StatementAddress(stmt), domain, sched ) );

	NodePtr ir = scop.toIR(mgr);
	
	EXPECT_EQ( ir->toString(), "{for(int<4> v5 = 0 .. int.add(100, 1) : 1) {for(int<4> v6 = 0 .. int.add(100, 1) : 1) {ref.assign(v3, array.ref.elem.1D(array.ref.elem.1D(v4, v5), v6));};};}");

	// perform interchange 
	poly::AffineSystem& schedule = scop[0].getSchedule();
	schedule.set( { { 0, 1, 0}, {1, 0, 0} } );

	ir = scop.toIR(mgr);

	EXPECT_EQ( ir->toString(), "{for(int<4> v7 = 0 .. int.add(100, 1) : 1) {for(int<4> v8 = 0 .. int.add(100, 1) : 1) {ref.assign(v3, array.ref.elem.1D(array.ref.elem.1D(v4, v8), v7));};};}");
}

TEST(Transformations, Tiling) {
	NodeManager mgr;
	IRBuilder builder(mgr);

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4()); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4()); 

	VariablePtr iterTile = Variable::get(mgr, mgr.getLangBasic().getInt4()); 
	
	VariablePtr var = Variable::get(mgr, mgr.getLangBasic().getInt4());
	ExpressionPtr arrAcc = builder.callExpr( 
		mgr.getLangBasic().getArrayRefElem1D(), 
		builder.callExpr( 
			mgr.getLangBasic().getArrayRefElem1D(), 
			builder.variable( builder.arrayType(builder.arrayType(mgr.getLangBasic().getInt4())) ),
			iter1
		),
		iter2
	);

	StatementPtr stmt = builder.callExpr( 
					mgr.getLangBasic().getRefAssign(), 
					var, 
					arrAcc
				);
	
	// std::cout << "STMT: " << *stmt << std::endl;

	poly::IterationVector iterVec( { iter1, iter2 } );  // (i,j,1)

	// DOMAIN
	// v1 >= 0 && v1 <= 100
	// v2 >= 0 && v2 <= 100
	poly::IterationDomain domain( iterVec, { { 1, 0,   0 }, 
										     {-1, 0, 100 }, 
										     { 0, 1,   0 }, 
										     { 0,-1, 100 } } );

	poly::AffineSystem sched( iterVec, { { 1,0,0 }, { 0,1,0} } );

	poly::Scop scop(iterVec);
	scop.push_back( poly::Stmt( 0, StatementAddress(stmt), domain, sched ) );

	NodePtr ir = scop.toIR(mgr);

	EXPECT_EQ( ir->toString(), "{for(int<4> v6 = 0 .. int.add(100, 1) : 1) {for(int<4> v7 = 0 .. int.add(100, 1) : 1) {ref.assign(v4, array.ref.elem.1D(array.ref.elem.1D(v5, v6), v7));};};}");

	// perform interchange 
	poly::AffineSystem& schedule = scop[0].getSchedule();

	// Add an outer loop 
	scop.getIterationVector().add( poly::Iterator(iterTile) );

	VariablePtr&& existenceVar = builder.variable( mgr.getLangBasic().getInt4() );
	scop.getIterationVector().add( poly::Iterator( existenceVar, true ) );

	// update the domain
	// v3 >= 0 && v3 <= 100
	// v1 >= v3 && v1 <= v3+T
	scop[0].getDomain() &= poly::IterationDomain(scop.getIterationVector(),
			{ { 0, 0,  1, 0,   0 }, 
			  { 0, 0, -1, 0, 100 }, 
			  { 1, 0, -1, 0,   0 },
			  {-1, 0,  1, 0,  25 } } );
	
	// exist e0: e0*T == v3
	scop[0].getDomain() &= poly::IterationDomain( 
		makeCombiner( 
			poly::AffineConstraint( 
				poly::AffineFunction( scop.getIterationVector(), { 0, 0,  1, -25, 0 } ), 
				poly::AffineConstraint::EQ 
			) 
		) 
	);

	// std::cout << "DOM: " << scop[0].getDomain() << std::endl;

	// add a new row to the scheduling matrix 
	schedule.append( { 0, 1, 0, 0, 0 } );

	// change the scheduling function by scheduling this loop as first 
	schedule[0].setCoeff(iterTile, 1);
	schedule[0].setCoeff(iter1, 0);
	schedule[1].setCoeff(iter1, 1);
	schedule[1].setCoeff(iter2, 0);

	// std::cout << schedule << std::endl;

	ir = scop.toIR(mgr);

	EXPECT_EQ( ir->toString(), "{for(int<4> v9 = 0 .. int.add(100, 1) : 25) {for(int<4> v10 = v9 .. int.add(ite(int.lt(100, int.add(v9, 25)), bind(){rec v13.{v13=fun() {return 100;}}()}, bind(){rec v12.{v12=fun(int<4> v11) {return int.add(v11, 25);}}(v9)}), 1) : 1) {for(int<4> v14 = 0 .. int.add(100, 1) : 1) {ref.assign(v4, array.ref.elem.1D(array.ref.elem.1D(v5, v10), v14));};};};}");

}

TEST(Transformations, Fusion) {
	NodeManager mgr;
	IRBuilder builder(mgr);

	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4()); 
	VariablePtr iter2 = Variable::get(mgr, mgr.getLangBasic().getInt4()); 
	
	VariablePtr fusedIter = Variable::get(mgr, mgr.getLangBasic().getInt4()); 

	VariablePtr var = Variable::get(mgr, mgr.getLangBasic().getInt4());
	ExpressionPtr arrAcc1 = builder.callExpr( 
		mgr.getLangBasic().getArrayRefElem1D(), 
		builder.callExpr( 
			mgr.getLangBasic().getArrayRefElem1D(), 
			builder.variable( builder.arrayType(builder.arrayType(mgr.getLangBasic().getInt4())) ),
			iter1
		),
		iter2
	);

	ExpressionPtr arrAcc2 = builder.callExpr( 
		mgr.getLangBasic().getArrayRefElem1D(), 
		builder.callExpr( 
			mgr.getLangBasic().getArrayRefElem1D(), 
			builder.variable( builder.arrayType(builder.arrayType(mgr.getLangBasic().getInt4())) ),
			iter2
		),
		iter1
	);

	StatementPtr stmt1 = builder.callExpr( 
					mgr.getLangBasic().getRefAssign(), 
					var, 
					arrAcc1
				);

	StatementPtr stmt2 = builder.callExpr( 
					mgr.getLangBasic().getRefAssign(), 
					var,
					builder.callExpr( mgr.getLangBasic().getSignedIntAdd(), var, arrAcc2)
				);

	
	// std::cout << "STMT: " << *stmt << std::endl;

	poly::IterationVector iterVec;  // (i,j,1)
	iterVec.add( poly::Iterator(iter1) ); 
	iterVec.add( poly::Iterator(iter2) ); 

	// STMT 1
	// DOMAIN
	// v1 >= 0 && v1 <= 100
	poly::IterationDomain domain1( iterVec, { {  1, 0,  0 },
										 	  { -1, 0, 90 } } );
	
	domain1 &= poly::IterationDomain( 
		makeCombiner( 
			poly::AffineConstraint( 
				poly::AffineFunction( iterVec, { 0, 1,  0 } ), 
				poly::AffineConstraint::EQ 
			) 
		) 
	);

	poly::AffineSystem sched1(iterVec, { {0, 0, 0},
										 {1, 0, 0}, 
										 {0, 0, 0} } );

	poly::Scop scop(iterVec);
	scop.push_back( poly::Stmt( 0, StatementAddress(stmt1), domain1, sched1 ) );

	// STMT2
	poly::AffineSystem sched2(iterVec, { {0, 0, 1}, 
									     {0, 1, 0}, 
										 {0, 0, 0} } );


	poly::IterationDomain domain2( iterVec, { {  0, 1,   0 },
							  				  {  0,-1, 100 } } );

	domain2 &= poly::IterationDomain( 
		makeCombiner( 
			poly::AffineConstraint( 
				poly::AffineFunction( iterVec, { 1, 0,  0 } ), 
				poly::AffineConstraint::EQ 
			) 
		) 
	);

	scop.push_back( poly::Stmt( 1, StatementAddress(stmt2), domain2, sched2 ) );

	NodePtr ir = scop.toIR(mgr);
	
	EXPECT_EQ( ir->toString(), "{for(int<4> v7 = 0 .. int.add(90, 1) : 1) {ref.assign(v4, array.ref.elem.1D(array.ref.elem.1D(v5, v7), 0));}; for(int<4> v8 = 0 .. int.add(100, 1) : 1) {ref.assign(v4, int.add(v4, array.ref.elem.1D(array.ref.elem.1D(v6, v8), 0)));};}");


	// Add an outer loop 
	scop.getIterationVector().add( poly::Iterator(fusedIter) );

	scop[0].getDomain() &= poly::IterationDomain( 
		makeCombiner( 
			poly::AffineConstraint( 
				poly::AffineFunction( scop.getIterationVector(), { 1, 0, -1, 0 } ), 
				poly::AffineConstraint::EQ 
			) 
		) 
	);

	scop[1].getDomain() &= poly::IterationDomain( 
		makeCombiner( 
			poly::AffineConstraint( 
				poly::AffineFunction( scop.getIterationVector(), { 0, 1, -1, 0 } ), 
				poly::AffineConstraint::EQ 
			) 
		) 
	);

	// perform interchange 
	scop[0].getSchedule().set( { {0,0,1,0}, 
								 {0,0,0,0} } );

	scop[1].getSchedule().set( { {0,0,1,0}, 
								 {0,0,0,1} } );

	ir = scop.toIR(mgr);

	EXPECT_EQ( ir->toString(), "{for(int<4> v9 = 0 .. int.add(90, 1) : 1) {ref.assign(v4, array.ref.elem.1D(array.ref.elem.1D(v5, v9), 0)); ref.assign(v4, int.add(v4, array.ref.elem.1D(array.ref.elem.1D(v6, v9), 0)));}; for(int<4> v10 = 91 .. int.add(100, 1) : 1) {ref.assign(v4, int.add(v4, array.ref.elem.1D(array.ref.elem.1D(v6, v10), 0)));};}");
}
 
