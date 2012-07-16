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

#include "insieme/analysis/dfa/solver.h"
#include "insieme/analysis/dfa/problem.h"
#include "insieme/analysis/dfa/entity.h"

#include "insieme/core/ir_program.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_statements.h"
#include "insieme/analysis/cfg.h"

#include "insieme/core/parser/ir_parse.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/utils/set_utils.h"
#include "insieme/utils/logging.h"

using namespace insieme;
using namespace insieme::core;
using namespace insieme::analysis;
using namespace insieme::analysis::dfa;

TEST(Problem, Variable) {

	NodeManager mgr;
	parse::IRParser parser(mgr);

	typedef utils::set::PointerSet<VariablePtr> VarSet; 

    auto code = parser.parseStatement(
		"{"
		"	decl ref<int<4>>:a = 0;"
		"	for(decl int<4>:i = 10 .. 50 : 1) { "
		"		(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, (i+int<4>:b))); "
		"	};"
		"	decl int<4>:c = (op<ref.deref>(a));"
		"}"
    );

    EXPECT_TRUE(code);

	CFGPtr cfg = CFG::buildCFG(code);

	WorklistQueue q;
	std::vector<unsigned> pushed_order;

	std::function<void (const cfg::BlockPtr&)> f = 
		[&q, &pushed_order] (const cfg::BlockPtr& block) { 
			q.enqueue(block); 
			pushed_order.push_back( block->getBlockID() );
		};

	cfg->visitDFS( f );

	EXPECT_FALSE(q.empty());
	EXPECT_EQ(11u, q.size());

	for (auto bid : pushed_order) {
		EXPECT_EQ(bid, q.dequeue()->getBlockID());
	}

	cfg->visitDFS( f );
	cfg->visitDFS( f );

	EXPECT_FALSE(q.empty());
	EXPECT_EQ(11u, q.size());
}

TEST(Problem, LiveVariables) {

	NodeManager mgr;
	parse::IRParser parser(mgr);

	typedef utils::set::PointerSet<VariablePtr> VarSet; 

    auto code = parser.parseStatement(
		"{"
		"	decl ref<int<4>>:a = 0;"
		"	for(decl int<4>:i = 10 .. 50 : 1) { "
		"		(op<array.ref.elem.1D>(ref<array<int<4>,1>>:v, (i+int<4>:b))); "
		"	};"
		"	decl int<4>:c = (op<ref.deref>(a));"
		"}"
    );

	std::cout << *code << std::endl;

    EXPECT_TRUE(code);
	CFGPtr cfg = CFG::buildCFG(code);

	Solver<LiveVariables> s(*cfg);
	s.solve();

}
