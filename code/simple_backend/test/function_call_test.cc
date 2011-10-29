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

#include <fstream>

#include "insieme/utils/compiler/compiler.h"
#include "insieme/utils/test/test_config.h"

#include "insieme/simple_backend/backend_convert.h"

#include "insieme/core/program.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/parser/ir_parse.h"

#include "insieme/utils/logging.h"
#include "insieme/utils/compiler/compiler.h"
#include "insieme/utils/test/test_utils.h"

namespace insieme {
namespace backend {

TEST(FunctionCall, templates) {
    core::NodeManager manager;
    core::parse::IRParser parser(manager);

    core::ProgramPtr program = parser.parseProgram("main: fun ()->int<4>:\
            mainfct in { ()->int<4>:mainfct = ()->int<4>{ { \
                (fun(type<'a>:dtype, uint<4>:size) -> array<'a, 1> {{ \
                    return (op<array.create.1D>( dtype, size )); \
                }}(lit<type<real<4> >, type(real(4)) >, lit<uint<4>, 7>) ); \
                \
                \
                return 0; \
            } } }");


    LOG(INFO) << "Printing the IR: " << core::printer::PrettyPrinter(program);

    LOG(INFO) << "Converting IR to C...";
    auto converted = simple_backend::SimpleBackend::getDefault()->convert(program);
    LOG(INFO) << "Printing converted code: " << *converted;

    string code = toString(*converted);

    EXPECT_FALSE(code.find("<?>") != string::npos);
//    EXPECT_FALSE(code.find("[[unhandled_simple_type") != string::npos);
}


TEST(FunctionCall, VectorReduction) {
    core::NodeManager manager;
    core::parse::IRParser parser(manager);

    // Operation: vector.reduction
    // Type: (vector<'elem,#l>, 'res, ('elem, 'res) -> 'res) -> 'res

    core::ProgramPtr program = parser.parseProgram("main: fun ()->unit:\
            mainfct in { ()->unit:mainfct = ()->unit{ { \
                (fun() -> unit {{ \
                    return (op<vector.reduction>( vector<int<4>,4>(1,2,3,4), 0, op<int.add> )); \
                }}() ); \
                return 0; \
            } } }");


    LOG(INFO) << "Printing the IR: " << core::printer::PrettyPrinter(program);

    LOG(INFO) << "Converting IR to C...";
    auto converted = simple_backend::SimpleBackend::getDefault()->convert(program);
    LOG(INFO) << "Printing converted code: " << *converted;

    string code = toString(*converted);

    EXPECT_FALSE(code.find("<?>") != string::npos);

    // test contracted form
    EXPECT_PRED2(containsSubString, code, "return (((((0+1)+2)+3)+4));");
}



TEST(FunctionCall, Pointwise) {
    core::NodeManager manager;
    core::parse::IRParser parser(manager);

    // Operation: vector.pointwise
    // Type: (('elem, 'elem) -> 'res) -> (vector<'elem,#l>, vector<'elem,#l>) -> vector<'res, #l>

    core::ProgramPtr program = parser.parseProgram("main: fun ()->unit:\
            mainfct in { ()->unit:mainfct = ()->unit{ { \
    			((op<vector.pointwise>(op<int.add>))(vector<int<4>,4>(1,2,3,4), vector<int<4>,4>(5,6,7,8))); \
            } } }");



    auto converted = simple_backend::SimpleBackend::getDefault()->convert(program);

    string code = toString(*converted);
    EXPECT_FALSE(code.find("<?>") != string::npos);

    // try compiling the code fragment
    utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-I " SRC_ROOT_DIR "simple_backend/include/insieme/simple_backend/runtime/");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}


TEST(FunctionCall, TypeLiterals) {
    core::NodeManager manager;
    core::parse::IRParser parser(manager);

    // create a function accepting a type literal

    core::ProgramPtr program = parser.parseProgram("main: fun ()->int<4>:\
                mainfct in { ()->int<4>:mainfct = ()->int<4>{ { \
                    (fun(type<'a>:dtype) -> int<4> {{ \
                        return 0; \
                    }}(lit<type<real<4> >, type(real(4)) >) ); \
                    return 0; \
                } } }");

    std::cout << "Program: " << *program << std::endl;

    auto converted = simple_backend::SimpleBackend::getDefault()->convert(program);

    std::cout << "Converted: \n" << *converted << std::endl;

    string code = toString(*converted);
    EXPECT_FALSE(code.find("<?>") != string::npos);

    // try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-I " SRC_ROOT_DIR "simple_backend/include/insieme/simple_backend/runtime/");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}


} // namespace backend
} // namespace insieme
