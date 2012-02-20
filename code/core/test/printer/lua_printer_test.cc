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

#include "insieme/core/printer/lua_printer.h"
#include "insieme/core/ir_builder.h"

#include "insieme/core/parser/ir_parse.h"
#include "insieme/utils/lua/lua.h"
#include "insieme/utils/test/test_utils.h"

namespace insieme {
namespace core {
namespace printer {

	TEST(LuaPrinter, SimpleExpr) {
	
		// sum up two values
		NodeManager manager;
		IRBuilder builder(manager);

		TypePtr intType = manager.getLangBasic().getInt4();
		VariablePtr a = builder.variable(intType, 1);
		VariablePtr b = builder.variable(intType, 2);
		ExpressionPtr t = builder.add(a, b);

		EXPECT_EQ("int.add(v1, v2)", toString(*t));
		EXPECT_EQ("v1 + v2", toLuaScript(t));

		t = builder.mul(t,builder.sub(t,a));
		EXPECT_EQ("int.mul(int.add(v1, v2), int.sub(int.add(v1, v2), v1))", toString(*t));
		EXPECT_EQ("(v1 + v2) * ((v1 + v2) - v1)", toLuaScript(t));

		// test whether script is syntactically correct
		utils::lua::Lua lua;
		lua.run("v1 = 1 ; v2 = 2");
		EXPECT_EQ(6, lua.eval<int>(toLuaScript(t)));
	}

	TEST(LuaPrinter, ControlFlow) {
		NodeManager mgr;
		parse::IRParser parser(mgr);

		auto forStmt = static_pointer_cast<const ForStmt>( parser.parseStatement(
			"for(decl uint<4>:k = 0 .. 10 : 1) {"
			"	for(decl uint<4>:i = 0 .. 20 : 1) {"
			"		decl ref<uint<4>>:m = (op<ref.var>(10));"
			"		for(decl uint<4>:j = 0 .. 30 : 1) {"
			"			(op<ref.assign>((op<vector.ref.elem>(ref<vector<uint<4>,10>>:v, i)), (op<ref.deref>(m))));"
			"           (op<ref.assign>(ref<uint<4>>:x, (x + 1)));"
			"		};"
			"	};"
			"}") );


		string script = toLuaScript(forStmt);
		EXPECT_PRED2(containsSubString, script, "(10 - 1)");
		EXPECT_PRED2(containsSubString, script, "v5[v2] = (v3)");
		EXPECT_PRED2(containsSubString, script, "v6 = (v6) + 1");

		// test whether script is syntactically correct
		utils::lua::Lua lua;
		lua.run("v5 = {}; v6 = 0");
		lua.run(toLuaScript(forStmt));
		EXPECT_EQ(10*20*30, lua.eval<int>("v6"));
	}


} // end namespace printer
} // end namespace core
} // end namespace insieme