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

#include "insieme/analysis/polyhedral/polyhedral.h"
#include "insieme/analysis/polyhedral/backend.h"
#include "insieme/analysis/polyhedral/backends/isl_backend.h"
#include "insieme/core/ir_expressions.h"
#include "insieme/core/ir_builder.h"

#include "isl/map.h"
#include "isl/union_map.h"

using namespace insieme;
using namespace insieme::core;
using namespace insieme::analysis;
using namespace insieme::analysis::polyhedral;

using insieme::utils::ConstraintType;

#define CREATE_ITER_VECTOR \
	VariablePtr iter1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1); \
	VariablePtr param = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3); \
	\
	IterationVector iterVec; \
	\
	iterVec.add( Iterator(iter1) ); \
	EXPECT_EQ(static_cast<size_t>(2), iterVec.size()); \
	iterVec.add( Parameter(param) ); \
	EXPECT_EQ(static_cast<size_t>(3), iterVec.size()); \

typedef std::vector<int> CoeffVect;

TEST(IslBackend, SetCreation) {
	
	NodeManager mgr;	
	CREATE_ITER_VECTOR; 

	auto&& ctx = makeCtx<ISL>();
	auto&& set = makeSet(ctx, IterationDomain(iterVec));

	std::ostringstream ss;
	ss << *set;
	EXPECT_EQ("[v3] -> { [v1] }", ss.str());

	IterationVector iv2;
	set->toConstraint(mgr, iv2);
	EXPECT_EQ(iv2, iterVec);
}

TEST(IslBackend, SetConstraint) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	AffineFunction af(iterVec, CoeffVect({0,3,10}) );
	AffineConstraint c(af, ConstraintType::LT);

	auto&& ctx = makeCtx<ISL>();
	auto&& set = makeSet(ctx, IterationDomain(c));
	set->simplify();

	std::ostringstream ss;
	ss << *set;
	EXPECT_EQ("[v3] -> { [v1] : v3 <= -4 }", ss.str());

	IterationVector iv2;
	set->toConstraint(mgr, iv2);

	EXPECT_EQ(iv2, iterVec);

	// Build directly the ISL set
	//isl_union_set* refSet = isl_union_set_read_from_str(ctx->getRawContext(), 
	//		"[v3] -> { [v1] : 3v3 + 10 < 0}"
	//	);
	//isl_union_set* diff = isl_union_set_subtract(refSet, isl_union_set_copy(const_cast<isl_union_set*>(set->getAsIslSet())));
	
	// check for equality
	//EXPECT_EQ( isl_union_set_is_empty(diff) );
	//isl_union_set_free(diff);
}

TEST(IslBackend, SetConstraintNormalized) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	AffineFunction af(iterVec, CoeffVect({1, 0, 10}) );
	
	// 1*v1 + 0*v2  + 10 != 0
	AffineConstraint c(af, ConstraintType::NE);

	// 1*v1 + 10 > 0 && 1*v1 +10 < 0
	// 1*v1 + 9 >= 0 & -1*v1 -11 >= 0
	auto&& ctx = makeCtx<ISL>();
	auto&& set = makeSet(ctx, IterationDomain(c));

	std::ostringstream ss;
	ss << *set;
	EXPECT_EQ("[v3] -> { [v1] : v1 <= -11 or v1 >= -9 }", ss.str());

	// Build directly the ISL set
	//isl_union_set* refSet = isl_union_set_read_from_str(ctx->getRawContext(), 
	//		"[v3] -> {[v1] : v1 + 10 < 0 or v1 + 10 > 0}"
	//	);
	
	//isl_union_set* diff = isl_union_set_subtract(refSet, isl_union_set_copy(const_cast<isl_union_set*>(set->getAsIslSet())));
	// check for equality
	//EXPECT_TRUE( isl_union_set_is_empty(diff) );
	//isl_union_set_free(diff);
}

TEST(IslBackend, FromCombiner) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	// 0*v1 + 2*v2 + 10
	AffineFunction af(iterVec, CoeffVect({0,2,10}) );

	// 0*v1 + 2*v3 + 10 == 0
	AffineConstraint c1(af, ConstraintType::EQ);

	// 2*v1 + 3*v3 +10 
	AffineFunction af2(iterVec, CoeffVect({2,3,10}) );
	
	// 2*v1 + 3*v3 +10 < 0
	AffineConstraint c2(af2, ConstraintType::LT);

	// 2v3+10 == 0 OR !(2v1 + 3v3 +10 < 0)
	
	auto&& ctx = makeCtx<ISL>();
	AffineConstraintPtr cons1 = c1 or (not_(c2));
	auto&& set = makeSet(ctx, IterationDomain( cons1 ));

	std::ostringstream ss;
	ss << *set;
	EXPECT_EQ("[v3] -> { [v1] : v3 = -5 or 2v1 >= -10 - 3v3 }", ss.str());

	IterationVector iv;
	AffineConstraintPtr cons2 = set->toConstraint(mgr, iv);

	// normalize the cons1 as it contains negations 

	// Build directly the ISL set
	isl_union_set* refSet = isl_union_set_read_from_str(ctx->getRawContext(), 
			"[v3] -> {[v1] : 2*v3 + 10 = 0 or 2*v1 +3*v3 +10 >= 0}"
		);
	
	SetPtr<> set2(*ctx, refSet);
	
	// we cannot check equality on sets because 1 sets contains ids and the one read from the string doesnt
	std::ostringstream ss1;
	ss1 << *set;
	std::ostringstream ss2;
	ss2 << *set2;

	EXPECT_EQ( ss1.str(), ss2.str() );
}

TEST(IslBackend, SetUnion) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;
	AffineConstraint c1(AffineFunction(iterVec, CoeffVect({0,3,10})), ConstraintType::LT);
	AffineConstraint c2(AffineFunction(iterVec, CoeffVect({1,-1,0})), ConstraintType::EQ);

	auto&& ctx = makeCtx<ISL>();
	auto&& set1 = makeSet(ctx, IterationDomain(c1) );
	auto&& set2 = makeSet(ctx, IterationDomain(c2) );

	auto&& set = set1 + set2;

	IterationVector iv;
	AffineConstraintPtr cons = set->toConstraint(mgr, iv);
	AffineConstraintPtr orig = normalize(c1 or c2);
	
	//std::ostringstream ss;
	//set->printTo(ss);
	
	//isl_union_set* refMap = isl_union_set_read_from_str(ctx->getRawContext(), 
	//		"[v3] -> { [v1] : 3*v3 + 10 < 0; [v1] : 1*v1 -1*v3 = 0}"
	//	);

	//printIslSet(std::cout, ctx->getRawContext(), refMap);
	//EXPECT_EQ("", ss.str());
}

TEST(IslBackend, SetIntersect) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;
	AffineConstraint c1(AffineFunction(iterVec, CoeffVect({0,3,10})), ConstraintType::LT);
	AffineConstraint c2(AffineFunction(iterVec, CoeffVect({1,-1,0})), ConstraintType::EQ);

	auto&& ctx = makeCtx<ISL>();
	auto&& set1 = makeSet(ctx, IterationDomain(c1) );
	auto&& set2 = makeSet(ctx, IterationDomain(c2) );

	auto&& set = set1 * set2;

	IterationVector iv;
	AffineConstraintPtr cons = set->toConstraint(mgr, iv);
	
	//std::cout << *cons << std::endl;
	AffineConstraintPtr orig = normalize(c1 and c2);

	//std::cout << *orig << std::endl;

	
	//std::ostringstream ss;
	//set->printTo(ss);
	
	//isl_union_set* refMap = isl_union_set_read_from_str(ctx->getRawContext(), 
	//		"[v3] -> { [v1] : 3*v3 + 10 < 0; [v1] : 1*v1 -1*v3 = 0}"
	//	);

	//printIslSet(std::cout, ctx->getRawContext(), refMap);
	//EXPECT_EQ("", ss.str());
}

TEST(IslBackend, SimpleMap) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	AffineSystem affSys(iterVec, { { 0, 2, 10 }, 
		  							     { 1, 1,  0 },
									     { 1,-1,  8 } } );
	// 0*v1 + 2*v2 + 10
	
	auto&& ctx = makeCtx<ISL>();
	auto&& map = makeMap(ctx, affSys);

	std::ostringstream ss;
	ss << *map;
	EXPECT_EQ("[v3] -> { [v1] -> [10 + 2v3, v3 + v1, 8 - v3 + v1] }", ss.str());

//	isl_union_map* refMap = isl_union_map_read_from_str(ctx->getRawContext(), 
//			"[v3] -> {[v1] -> [10 + 2v3, v3 + v1, 8 - v3 + v1] }"
//		);

//	EXPECT_TRUE( isl_union_map_is_equal(refMap, const_cast<isl_union_map*>(map->getAsIslMap())) );

//	isl_union_map_free(refMap);
}

TEST(IslBackend, MapUnion) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	AffineSystem affSys(iterVec, { { 0, 2, 10 }, 
		  							     { 1, 1,  0 }, 
									     { 1,-1,  8 } } );
	// 0*v1 + 2*v2 + 10
	auto&& ctx = makeCtx<ISL>();
	auto&& map = makeMap(ctx, affSys);

	std::ostringstream ss;
	ss << *map;
	EXPECT_EQ("[v3] -> { [v1] -> [10 + 2v3, v3 + v1, 8 - v3 + v1] }", ss.str());
	
	AffineSystem affSys2(iterVec, { { 1,-2, 0 }, 
		 								  { 1, 8, 4 }, 
										  {-5,-1, 4 } } );
	// 0*v1 + 2*v2 + 10
	auto&& map2 = makeMap( ctx, affSys2 );
	std::ostringstream ss2;
	ss2 << *map2;
	EXPECT_EQ("[v3] -> { [v1] -> [-2v3 + v1, 4 + 8v3 + v1, 4 - v3 - 5v1] }", ss2.str());

	auto&& mmap = *map + *map2;
	std::ostringstream  ss3;
	ss3 << *mmap;
	EXPECT_EQ("[v3] -> { [v1] -> [10 + 2v3, v3 + v1, 8 - v3 + v1]; [v1] -> [-2v3 + v1, 4 + 8v3 + v1, 4 - v3 - 5v1] }", ss3.str());
}

TEST(Domain, SimpleStrided) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	VariablePtr stride = Variable::get(mgr, mgr.getLangBasic().getInt4(), 8);
	iterVec.add( Iterator(stride, true) );

	AffineFunction af(iterVec,  { 0, 1, 0, 10 });
	AffineFunction af2(iterVec, { 1, 2, 5,  7 });
	AffineFunction af3(iterVec, { 1, 3, 0,  0 });

	AffineConstraintPtr&& cl = 
		AffineConstraint(af, 	ConstraintType::LT) and 
		AffineConstraint(af2, ConstraintType::EQ) and 
		AffineConstraint(af3, ConstraintType::NE);
	{
		std::ostringstream ss;
		ss << iterVec;
		EXPECT_EQ("(v1,v8|v3|1)", ss.str());
	}

	auto&& ctx = makeCtx<ISL>();
	auto&& set = makeSet(ctx, IterationDomain(cl));

	std::ostringstream ss;
	ss << *set;
	EXPECT_EQ("[v3] -> { [v1] : (exists (e0 = [(-1 - v3 + v1)/2]: 2e0 = -1 - v3 + v1 and v1 >= 15 - 5v3 and v1 >= -19 - 15v3)) or (exists (e0 = [(-1 - v3 + v1)/2]: 2e0 = -1 - v3 + v1 and v1 >= 15 - 5v3 and v1 <= -23 - 15v3)) }", ss.str());

}

TEST(Domain, Strided) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	VariablePtr stride1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 8);
	VariablePtr stride2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 9);
	iterVec.add( Iterator(stride1, true) );
	iterVec.add( Iterator(stride2, true) );


	AffineFunction af(iterVec,  { 1, 0, 0, 0, -3 });
	AffineFunction af2(iterVec, {-1, 0, 0, 0, 9 });
	AffineFunction af3(iterVec, { -1, 2, 0, 0, 0 });
	AffineFunction af4(iterVec, { -1, 0, 3, 0, 0 });


	AffineConstraintPtr&& cl = 
		AffineConstraint(af) and 
		AffineConstraint(af2) and 
		AffineConstraint(af3, ConstraintType::EQ) and 
		AffineConstraint(af4, ConstraintType::EQ);
	{
		std::ostringstream ss;
		ss << iterVec;
		EXPECT_EQ("(v1,v8,v9|v3|1)", ss.str());
	}

	auto&& ctx = makeCtx<ISL>();
	auto&& set = makeSet(ctx, IterationDomain(cl));

	std::ostringstream ss;
	ss << *set;
	EXPECT_EQ("[v3] -> { [6] }", ss.str());

}

namespace {

IterationDomain createFloor( IterationVector& iterVec, int N, int D ) {

	return IterationDomain(
		AffineConstraint(
			// p - exist1*D - exist2 == 0
			AffineFunction( iterVec, CoeffVect({ -D, -1,  1,  0 }) ), ConstraintType::EQ) and 
		AffineConstraint(
			// exist2 - D < 0
			AffineFunction( iterVec, CoeffVect({  0,  1,  0, -D }) ), utils::ConstraintType::LT) and
		AffineConstraint(
			// exist2 >= 0
			AffineFunction( iterVec, CoeffVect({  0,  1,  0,  0 }) ), utils::ConstraintType::GE) and
		AffineConstraint(
			// N == N
			AffineFunction( iterVec, CoeffVect({  0,  0,  1, -N }) ), utils::ConstraintType::EQ)
	);

}

IterationDomain createCeil( IterationVector& iterVec, int N, int D ) {

	return IterationDomain(
		AffineConstraint(
			// p - exist1*D + exist2 == 0
			AffineFunction( iterVec, CoeffVect({ -D, +1,  1,  0 }) ), ConstraintType::EQ) and 
		AffineConstraint(
			// exist2 - D < 0
			AffineFunction( iterVec, CoeffVect({  0,  1,  0, -D }) ), utils::ConstraintType::LT) and
		AffineConstraint(
			// exist2 >= 0
			AffineFunction( iterVec, CoeffVect({  0,  1,  0,  0 }) ), utils::ConstraintType::GE) and
		AffineConstraint(
			// N == N
			AffineFunction( iterVec, CoeffVect({  0,  0,  1, -N })), utils::ConstraintType::EQ)
	);

}
} // end anonymous namespace

TEST(IslBackend, Floor) {
	NodeManager mgr;

	VariablePtr exist1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1);
	VariablePtr exist2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2);
	VariablePtr p = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3);

	IterationVector iterVec;
	iterVec.add( Iterator(exist1) );
	iterVec.add( Iterator(exist2, true) );
	iterVec.add( Parameter(p) );

	{
		IterationDomain dom = createFloor(iterVec, 24, 5);
		auto&& ctx = makeCtx<ISL>();
		auto&& set = makeSet(ctx, dom);

		std::ostringstream ss;
		ss << *set;
		EXPECT_EQ(ss.str(), "[v3] -> { [4] : v3 = 24 }");
	}
	{
		IterationDomain dom = createFloor(iterVec, 40, 5);
		auto&& ctx = makeCtx<ISL>();
		auto&& set = makeSet(ctx, dom);

		std::ostringstream ss;
		ss << *set;
		EXPECT_EQ(ss.str(), "[v3] -> { [8] : v3 = 40 }");
	}

	{
		IterationDomain dom = createFloor(iterVec, -12, 5);
		auto&& ctx = makeCtx<ISL>();
		auto&& set = makeSet(ctx, dom);

		std::ostringstream ss;
		ss << *set;
		EXPECT_EQ(ss.str(), "[v3] -> { [-3] : v3 = -12 }");
		
	}
}

TEST(IslBackend, Ceil) {
	NodeManager mgr;

	VariablePtr exist1 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 1);
	VariablePtr exist2 = Variable::get(mgr, mgr.getLangBasic().getInt4(), 2);
	VariablePtr p = Variable::get(mgr, mgr.getLangBasic().getInt4(), 3);

	IterationVector iterVec;
	iterVec.add( Iterator(exist1) );
	iterVec.add( Iterator(exist2, true) );
	iterVec.add( Parameter(p) );

	{
		IterationDomain dom = createCeil(iterVec, 24, 5);
		auto&& ctx = makeCtx<ISL>();
		auto&& set = makeSet(ctx, dom);

		std::ostringstream ss;
		ss << *set;
		EXPECT_EQ(ss.str(), "[v3] -> { [5] : v3 = 24 }");
	}
	{
		IterationDomain dom = createCeil(iterVec, 40, 5);
		auto&& ctx = makeCtx<ISL>();
		auto&& set = makeSet(ctx, dom);

		std::ostringstream ss;
		ss << *set;
		EXPECT_EQ(ss.str(), "[v3] -> { [8] : v3 = 40 }");
	}
}

TEST(IslBackend, Cardinality) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;
	// 0*v1 + 2*v2 + 10
	AffineFunction af(iterVec, {1,2,10} );

	// 0*v1 + 2*v3 + 10 == 0
	AffineConstraint c1(af, ConstraintType::GT);

	// 2*v1 + 3*v3 +10 
	AffineFunction af2(iterVec, {1,3,10} );
	
	// 2*v1 + 3*v3 +10 < 0
	AffineConstraint c2(af2, ConstraintType::LT);

	// 2v3+10 == 0 OR !(2v1 + 3v3 +10 < 0)
	
	auto&& ctx = makeCtx<ISL>();
	AffineConstraintPtr cons1 = c1 and c2;
	auto&& set = makeSet(ctx, IterationDomain( cons1 ));

	PiecewisePtr<> pw = set->getCard();

	{
		std::ostringstream ss;
		ss << *pw;
		EXPECT_EQ("[v3] -> { (-1 - v3) : v3 <= -2 }", ss.str());
	}

	IterationVector cardDom = pw->getIterationVector(mgr);
	IterationDomain dom = makeVarRange(cardDom, param, IRBuilder(mgr).intLit(-4));

	// intersect with v3 == 4
	pw *= makeSet(ctx, dom);

	{
		std::ostringstream ss;
		ss << *pw;
		EXPECT_EQ("[v3] -> { 3 : v3 = -4 }", ss.str());
	}
	pw->simplify();
	std::cout << *pw << std::endl;

	// arithmetic::Piecewise apw = pw->toPiecewise(mgr);
	// std::cout << apw;
	// EXPECT_TRUE( arithmetic::isFormula(apw) );
	// EXPECT_EQ(arithmetic::Formula(-5), arithmetic::toFormula(apw));
}

TEST(IslBackend, Cardinality2) {
	NodeManager mgr;
	core::IRBuilder builder(mgr);

	CREATE_ITER_VECTOR;
	core::VariablePtr it2 = builder.variable( mgr.getLangBasic().getInt4(), 2 );

	iterVec.add( Iterator(it2) );

	// 1*v1 + 2*v3
	AffineFunction af(iterVec, CoeffVect({1,0,2,0}) );
	// 1*v1 + 2*v3 > 0
	AffineConstraint c1(af, ConstraintType::GT);

	// 1*v1 + 2*v3 + 10 
	AffineFunction af2(iterVec, CoeffVect({1,0,2,-10}) );
	// 1*v1 + 2*v3 +10 < 0
	AffineConstraint c2(af2, ConstraintType::LT);


	// 1*v2 + 3*v3 + 0
	AffineFunction af3(iterVec, CoeffVect({0,1,3,0}) );
	// 1*v2 + 3*v3 + 10 > 0
	AffineConstraint c3(af3, ConstraintType::GT);

	// 1*v2 + 3*v3 +10 
	AffineFunction af4(iterVec, CoeffVect({0,1,3,-10}) );
	
	// 2*v1 + 3*v3 +10 < 0
	AffineConstraint c4(af4, ConstraintType::LT);

	auto&& ctx = makeCtx<ISL>();
	AffineConstraintPtr cons1 = (c1 and c2) and (c3 and c4);
	auto&& set = makeSet(ctx, IterationDomain( cons1 ));

	PiecewisePtr<> pw = set->getCard();
	{
		std::ostringstream ss;
		ss << *pw;
		EXPECT_EQ("[v3] -> { 81 }", ss.str());
	}
	
	utils::Piecewise<arithmetic::Formula> apw = pw->toPiecewise(mgr);
	//EXPECT_TRUE( utils::isFormula(apw) );
	//EXPECT_EQ(arithmetic::Formula(81), arithmetic::toFormula(apw));

}
