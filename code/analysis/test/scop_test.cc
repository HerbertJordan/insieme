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

#include "insieme/analysis/scop.h"
#include "insieme/analysis/polyhedral.h"

#include "insieme/core/program.h"
#include "insieme/core/ast_builder.h"
#include "insieme/core/statements.h"

#include "insieme/core/parser/ir_parse.h"

using namespace insieme::core;
using namespace insieme::analysis;

TEST(ScopRegion, CompoundStmt) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);

    auto compStmt = parser.parseStatement(
		"{\
			decl int<4>:b = 20; \
			(op<array.subscript.1D>(array<int<4>,1>:v, (int<4>:a+b)));\
		}"
	);
	// Mark scops in this code snippet
	scop::mark(compStmt);

	EXPECT_FALSE(compStmt->hasAnnotation(scop::ScopRegion::KEY));
}

TEST(ScopRegion, IfStmt) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);

    auto ifStmt = static_pointer_cast<const IfStmt>( parser.parseStatement("\
		if((int<4>:c <= int<4>:d)){ \
			(op<array.subscript.1D>(array<int<4>,1>:v, (int<4>:a-int<4>:b))); \
		} else { \
			(op<array.subscript.1D>(array<int<4>,1>:v, (int<4>:a+int<4>:b))); \
		}") );
	// std::cout << *ifStmt << std::endl;
	// Mark scops in this code snippet
	scop::mark(ifStmt);

	EXPECT_TRUE(ifStmt->getThenBody()->hasAnnotation(scop::ScopRegion::KEY));
	scop::ScopRegion& ann = *ifStmt->getThenBody()->getAnnotation(scop::ScopRegion::KEY);

	poly::IterationVector iterVec = ann.getIterationVector();
	EXPECT_EQ(static_cast<size_t>(5), iterVec.size());

	EXPECT_EQ(static_cast<size_t>(0), iterVec.getIteratorNum());
	EXPECT_EQ(static_cast<size_t>(4), iterVec.getParameterNum());
	{	
		std::ostringstream ss;
		ss << ann.getIterationVector();
		EXPECT_EQ("(|v7,v8,v4,v5|1)", ss.str());
	}
	{ 
		std::ostringstream ss;
		ss << *ann.getConstraints();
		EXPECT_EQ("(0*v7 + 0*v8 + 1*v4 + -1*v5 + 0*1 <= 0)", ss.str());
	}
	EXPECT_TRUE(ifStmt->getElseBody()->hasAnnotation(scop::ScopRegion::KEY));
	ann = *ifStmt->getElseBody()->getAnnotation(scop::ScopRegion::KEY);
	iterVec = ann.getIterationVector();
	EXPECT_EQ(static_cast<size_t>(5), iterVec.size());

	EXPECT_EQ(static_cast<size_t>(0), iterVec.getIteratorNum());
	EXPECT_EQ(static_cast<size_t>(4), iterVec.getParameterNum());
 	{	
		std::ostringstream ss;
		ss << ann.getIterationVector();
		EXPECT_EQ("(|v7,v8,v4,v5|1)", ss.str());
	}
	{ 
		std::ostringstream ss;
		ss << *ann.getConstraints();
		EXPECT_EQ("NOT(0*v7 + 0*v8 + 1*v4 + -1*v5 + 0*1 <= 0)", ss.str());
	}


}

TEST(ScopRegion, SimpleForStmt) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);

    auto forStmt = static_pointer_cast<const ForStmt>( parser.parseStatement("\
		for(decl int<4>:i = 10 .. 50 : -1) { \
			(op<array.subscript.1D>(array<int<4>,1>:v, (i+int<4>:b))); \
		}") );
	// std::cout << *forStmt << std::endl;
	scop::mark(forStmt);

	EXPECT_TRUE(forStmt->hasAnnotation(scop::ScopRegion::KEY));
	scop::ScopRegion& ann = *forStmt->getAnnotation(scop::ScopRegion::KEY);

	poly::IterationVector iterVec = ann.getIterationVector();
	EXPECT_EQ(static_cast<size_t>(3), iterVec.size());
	EXPECT_EQ(static_cast<size_t>(1), iterVec.getIteratorNum());
	EXPECT_EQ(static_cast<size_t>(1), iterVec.getParameterNum());

	{	
		std::ostringstream ss;
		ss << iterVec;
		EXPECT_EQ("(v9|v11|1)", ss.str());
	}
	{ 
		std::ostringstream ss;
		ss << *ann.getConstraints();
		EXPECT_EQ("(1*v9 + 0*v11 + -10*1 >= 0) AND (1*v9 + 0*v11 + -50*1 < 0)", ss.str());
	}
	EXPECT_FALSE(forStmt->getBody()->hasAnnotation(scop::ScopRegion::KEY));
}

TEST(ScopRegion, ForStmt) {
	
	NodeManager mgr;
	parse::IRParser parser(mgr);

    auto forStmt = static_pointer_cast<const ForStmt>( parser.parseStatement("\
		for(decl int<4>:i = 10 .. 50 : 1) { \
			(op<array.subscript.1D>(array<int<4>,1>:v, (i*int<4>:b))); \
			if ((i > 25)) { \
				(int<4>:h = (op<array.subscript.1D>(array<int<4>,1>:v, ((int<4>:n+i)-1))));\
			};\
		}") );
	// std::cout << *forStmt << std::endl;
	scop::mark(forStmt);

	EXPECT_FALSE( forStmt->hasAnnotation(scop::ScopRegion::KEY) );
	IfStmtPtr ifStmt = static_pointer_cast<const IfStmt>(
		static_pointer_cast<const CompoundStmt>(forStmt->getBody())->getStatements().back());

	EXPECT_FALSE(ifStmt->hasAnnotation(scop::ScopRegion::KEY));
	EXPECT_TRUE(ifStmt->getThenBody()->hasAnnotation(scop::ScopRegion::KEY));

	// check the then body
	scop::ScopRegion& ann = *ifStmt->getThenBody()->getAnnotation(scop::ScopRegion::KEY);
	poly::IterationVector iterVec = ann.getIterationVector(); 

	EXPECT_EQ(static_cast<size_t>(3), iterVec.size());
	EXPECT_EQ(static_cast<size_t>(0), iterVec.getIteratorNum());
	EXPECT_EQ(static_cast<size_t>(2), iterVec.getParameterNum());
	
	{
		std::ostringstream ss;
		ss << iterVec;
		EXPECT_EQ("(|v12,v16|1)", ss.str());
	}

	{
		std::ostringstream ss;
		ss << *ann.getConstraints();
		EXPECT_EQ("(1*v12 + 0*v16 + -25*1 > 0)", ss.str());
	}



}

