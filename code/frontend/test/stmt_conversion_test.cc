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

// defines which are needed by LLVM
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include "clang/AST/Decl.h"
// DON'T MOVE THIS!

#include <gtest/gtest.h>

#include "insieme/core/program.h"
#include "insieme/core/ast_check.h"
#include "insieme/core/checks/typechecks.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/utils/logging.h"

#include "insieme/frontend/program.h"
#include "insieme/frontend/clang_config.h"
#include "insieme/frontend/conversion.h"
#include "insieme/frontend/insieme_pragma.h"

#include "clang/Index/Indexer.h"
#include "clang/Index/Program.h"

using namespace insieme::core;
using namespace insieme::frontend;
using namespace insieme::frontend::conversion;
using namespace clang;

struct VariableResetHack {
	static void reset() { Variable::counter = 0; }
};

TEST(StmtConversion, FileTest) {

	insieme::utils::InitLogger("ut_stmt_conversion_test", INFO, true);
	CommandLineOptions::Verbosity = 0;

	SharedNodeManager shared = std::make_shared<NodeManager>();
	insieme::frontend::Program prog(shared);
	prog.addTranslationUnit( std::string(SRC_DIR) + "/inputs/stmt.c" );

	const PragmaList& pl = (*prog.getTranslationUnits().begin())->getPragmaList();
	const ClangCompiler& comp = (*prog.getTranslationUnits().begin())->getCompiler();

	std::for_each(pl.begin(), pl.end(),
		[ & ](const PragmaPtr curr) {
			const TestPragma* tp = static_cast<const TestPragma*>(&*curr);
			// we reset the counter for variables so we can write independent tests
			VariableResetHack::reset();

			clang::idx::Program p;
			clang::idx::Indexer idx(p);

			ConversionFactory convFactory( shared, comp, prog.getClangIndexer(), prog.getClangProgram(), pl );

			if(tp->isStatement()) {
				StatementPtr&& stmt = convFactory.convertStmt( tp->getStatement() );
				std::ostringstream ss;
				ss << insieme::core::printer::PrettyPrinter(stmt, insieme::core::printer::PrettyPrinter::OPTIONS_DETAIL);

				std::vector<char> res;
				std::string prettyPrint = ss.str();
				for(auto it = prettyPrint.begin(), end = prettyPrint.end(); it != end; ++it)
					if(!(*it == '\n' || (it + 1 != end && *it == ' ' && *(it+1) == ' ')))
						res.push_back(*it);

				EXPECT_EQ(tp->getExpected(), '\"' + std::string(res.begin(), res.end()) + '\"' );
				// std::cout << ss.str();
				// do type checking
				MessageList&& msgList = check( stmt, checks::getFullCheck() );
				EXPECT_EQ(static_cast<unsigned int>(0), msgList.size());
				std::sort(msgList.begin(), msgList.end());
				std::for_each(msgList.begin(), msgList.end(), [&stmt](const Message& cur) {
					LOG(INFO) << *stmt;
					LOG(INFO) << cur << std::endl;
				});

			} else {
				if(const clang::TypeDecl* td = dyn_cast<const clang::TypeDecl>(tp->getDecl())) {
					EXPECT_EQ(tp->getExpected(), '\"' + convFactory.convertType( td->getTypeForDecl() )->toString() + '\"' );
				}else if(const clang::FunctionDecl* fd = dyn_cast<const clang::FunctionDecl>(tp->getDecl())) {
					EXPECT_EQ(tp->getExpected(), '\"' + convFactory.convertFunctionDecl( fd )->toString() + '\"' );
				}
			}


	});

}

