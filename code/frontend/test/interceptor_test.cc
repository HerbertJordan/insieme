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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include "clang/AST/Decl.h"
#pragma GCC diagnostic pop
// DON'T MOVE THIS!

#include <gtest/gtest.h>

#include "insieme/frontend/stmt_converter.h"
#include "insieme/frontend/expr_converter.h"
#include "insieme/frontend/type_converter.h"

#include "insieme/core/ir_program.h"
#include "insieme/core/checks/full_check.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/utils/logging.h"

#include "insieme/utils/config.h"
#include "insieme/frontend/convert.h"
#include "insieme/frontend/pragma/insieme.h"
#include "insieme/frontend/extensions/interceptor_extension.h"
#include "insieme/frontend/tu/ir_translation_unit.h"

#include "insieme/utils/test/test_utils.h"
#include "test_utils.inc"

using namespace insieme::core;
using namespace insieme::core::checks;
using namespace insieme::utils::log;
namespace fe = insieme::frontend;

void checkSemanticErrors(const NodePtr& node) {
	auto msgList = check( node, checks::getFullCheck() ).getAll();
	EXPECT_EQ(static_cast<unsigned int>(0), msgList.size());
	std::sort(msgList.begin(), msgList.end());
	std::for_each(msgList.begin(), msgList.end(), [&node](const Message& cur) {
		LOG(INFO) << *node;
		LOG(INFO) << cur << std::endl;
	});
}

std::string getPrettyPrinted(const NodePtr& node) {
	std::ostringstream ss;
	ss << insieme::core::printer::PrettyPrinter(node,
			insieme::core::printer::PrettyPrinter::OPTIONS_DETAIL |
			insieme::core::printer::PrettyPrinter::NO_LET_BINDINGS
	);

	// Remove new lines and leading spaces
	std::vector<char> res;
	std::string prettyPrint = ss.str();
	for(auto it = prettyPrint.begin(), end = prettyPrint.end(); it != end; ++it)
		if(!(*it == '\n' || (it + 1 != end && *it == ' ' && *(it+1) == ' ')))
			res.push_back(*it);

	return std::string(res.begin(), res.end());
}

TEST(Interception, SimpleInterception) {
	fe::Source src(
		R"(
			#include "interceptor_header.h"
			void intercept_simpleFunc() {
				int a = 0;
				ns::simpleFunc(1);
				ns::simpleFunc(a);
			}

			void intercept_memFunc1() {
				int a = 0;
				ns::S s;
				s.memberFunc(a);
			}
			void intercept_memFunc2() {
				using namespace ns;
				int a = 0;
				S s;
				s.memberFunc(a);
			}

			// only for manual compilation
			int main() {
				intercept_simpleFunc();
				intercept_memFunc1();
				intercept_memFunc2();
			};
		)"
	,fe::CPP);

	NodeManager mgr;
	IRBuilder builder(mgr);
	fe::ConversionJob job(src);
    job.addIncludeDirectory(CLANG_SRC_DIR "inputs/interceptor/");
	job.addInterceptedNameSpacePattern( "ns::.*" );
	job.registerFrontendPlugin<fe::extensions::InterceptorPlugin>(job.getInterceptedNameSpacePatterns());
	auto tu = job.toIRTranslationUnit(mgr);
	//LOG(INFO) << tu;

	auto retTy = builder.getLangBasic().getUnit(); 
	auto funcTy = builder.functionType( TypeList(), retTy);

	//intercept_simpleFunc
	auto sf = builder.literal("intercept_simpleFunc", funcTy);
	auto sf_expected = "fun() -> unit { decl ref<int<4>> v1 = ( var(0)); ns::simpleFunc(1); ns::simpleFunc(( *v1));}";

	auto res = analysis::normalize(tu[sf]);
	auto code = toString(getPrettyPrinted(res));
	EXPECT_EQ(sf_expected, code);

	//intercept_memFunc1
	auto mf1 = builder.literal("intercept_memFunc1", funcTy);
	auto mf1_expected = "fun() -> unit { decl ref<int<4>> v1 = ( var(0)); decl ref<ns::S> v2 = ( var(zero(type<ns::S>))); memberFunc(v2, ( *v1));}";

	res = analysis::normalize(tu[mf1]);
	code = toString(getPrettyPrinted(res));

	EXPECT_EQ(mf1_expected, code);
	//intercept_memFunc2
	auto mf2 = builder.literal("intercept_memFunc2", funcTy);
	auto mf2_expected = "fun() -> unit { decl ref<int<4>> v1 = ( var(0)); decl ref<ns::S> v2 = ( var(zero(type<ns::S>))); memberFunc(v2, ( *v1));}";

	res = analysis::normalize(tu[mf2]);
	code = toString(getPrettyPrinted(res));
	EXPECT_EQ(mf2_expected, code);
}
