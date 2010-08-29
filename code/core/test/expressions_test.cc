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

#include <sstream>

#include <gtest/gtest.h>
#include "statements.h"
#include "expressions.h"

using namespace insieme::core;

TEST(ExpressionsTest, IntLiterals) {
	TypeManager typeMan;
	StatementManager manager(typeMan);
	IntLiteralPtr i5 = IntLiteral::get(manager, 5);
	IntLiteralPtr i7 = IntLiteral::get(manager, 7);
	IntLiteralPtr i5long = IntLiteral::get(manager, 5, 8);
	
	EXPECT_EQ( *i5, *IntLiteral::get(manager, 5) );
	EXPECT_NE( *i5, *i5long );
	EXPECT_NE( *i5, *i7 );
	EXPECT_EQ( i5->getValue(), 5 );
}

TEST(ExpressionsTest, FloatLiterals) {
	TypeManager typeMan;
	StatementManager manager(typeMan);
	FloatLiteralPtr f5_s = FloatLiteral::get(manager, "5.0");
	FloatLiteralPtr f5 = FloatLiteral::get(manager, 5.0);
	
	// EXPECT_EQ( *f5, *f5_s ); //-- this is not necessarily true
	std::stringstream ss;
	ss << *f5_s;
	EXPECT_EQ( ss.str(), "5.0" );
	EXPECT_EQ( f5->getValue(), f5_s->getValue() );
}
