/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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
#include "insieme/utils/config.h"

#include "insieme/backend/sequential/sequential_backend.h"
#include "insieme/backend/runtime/runtime_backend.h"

#include "insieme/core/ir_program.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/lang/basic.h"
#include "insieme/core/lang/extension.h"
#include "insieme/core/checks/full_check.h"

#include "insieme/utils/logging.h"
#include "insieme/utils/compiler/compiler.h"
#include "insieme/utils/test/test_utils.h"

namespace insieme {
namespace backend {

TEST(FunctionCall, templates) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    core::ProgramPtr program = builder.parseProgram(
    	"int<4> main() {"
    	"	lambda (type<'a> dtype, uint<4> size)->ref<array<'a,1>> {"
    	"		return ref_new(array_create_1D(dtype, size));"
    	"	} (lit(real<4>), 7u);"
    	"	return 0;"
    	"}"
    );

    ASSERT_TRUE(program);

    LOG(INFO) << "Printing the IR: " << core::printer::PrettyPrinter(program);

    LOG(INFO) << "Converting IR to C...";
    auto converted = sequential::SequentialBackend::getDefault()->convert(program);
    LOG(INFO) << "Printing converted code: " << *converted;

    string code = toString(*converted);

    EXPECT_PRED2(notContainsSubString, code, "<?>");
    EXPECT_PRED2(notContainsSubString, code, "<a>");
    EXPECT_PRED2(notContainsSubString, code, "UNSUPPORTED");

    // try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}


TEST(FunctionCall, VectorReduction) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    // Operation: vector.reduction
    // Type: (vector<'elem,#l>, 'res, ('elem, 'res) -> 'res) -> 'res

    std::map<string, core::NodePtr> symbols;
    symbols["v"] = builder.vectorExpr(toVector<core::ExpressionPtr>(builder.intLit(1), builder.intLit(2),  builder.intLit(3),  builder.intLit(4)));

    core::ProgramPtr program = builder.parseProgram(
    		"unit main() {"
    		"	lambda ()->int<4> {"
    		"		return vector_reduction(v, 0, int_add);"
    		"	}();"
    		"}",
    		symbols
    );
    ASSERT_TRUE(program);

    LOG(INFO) << "Printing the IR: " << core::printer::PrettyPrinter(program);

    LOG(INFO) << "Converting IR to C...";
    auto converted = sequential::SequentialBackend::getDefault()->convert(program);
    LOG(INFO) << "Printing converted code: " << *converted;

    string code = toString(*converted);

    EXPECT_FALSE(code.find("<?>") != string::npos);

    // test contracted form
    EXPECT_PRED2(containsSubString, code, "0 + 1 + 2 + 3 + 4;");

    // try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}



TEST(FunctionCall, Pointwise) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    // Operation: vector.pointwise
    // Type: (('elem, 'elem) -> 'res) -> (vector<'elem,#l>, vector<'elem,#l>) -> vector<'res, #l>

    std::map<string, core::NodePtr> symbols;
	symbols["v1"] = builder.vectorExpr(toVector<core::ExpressionPtr>(builder.intLit(1), builder.intLit(2),  builder.intLit(3),  builder.intLit(4)));
	symbols["v2"] = builder.vectorExpr(toVector<core::ExpressionPtr>(builder.intLit(5), builder.intLit(6),  builder.intLit(7),  builder.intLit(8)));

    core::ProgramPtr program = builder.parseProgram(
    		"unit main() {"
    		"	vector_pointwise(int_add)(v1,v2);"
    		"}",
    		symbols
    );
    ASSERT_TRUE(program);


    auto converted = sequential::SequentialBackend::getDefault()->convert(program);

    string code = toString(*converted);
    EXPECT_FALSE(code.find("<?>") != string::npos);

    // try compiling the code fragment
    utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-I " SRC_ROOT_DIR "simple_backend/include/insieme/simple_backend/runtime/");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler)) << "Code: \n" << *converted;
}


TEST(FunctionCall, TypeLiterals) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    // create a function accepting a type literal

    core::ProgramPtr program = builder.parseProgram(
    		"int<4> main() {"
    		"	lambda (type<'a> dtype)->int<4> {"
    		"	} (lit(real<4>));"
    		"	return 0;"
    		"}"
    );
    ASSERT_TRUE(program);

    std::cout << "Program: " << *program << std::endl;

    auto converted = sequential::SequentialBackend::getDefault()->convert(program);

    std::cout << "Converted: \n" << *converted << std::endl;

    string code = toString(*converted);
    EXPECT_PRED2(notContainsSubString, code, "<?>");
    EXPECT_PRED2(containsSubString, code, "fun_1()");

    // try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}


TEST(FunctionCall, GenericFunctionAndTypeLiteral) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    // create a function accepting a type literal

    core::ProgramPtr program = builder.parseProgram(R"(
    		int<4> main() {
    			lambda (ref<array<'a,1>> data)->uint<8> {
    				return sizeof(lit('a));
    			} (ref_reinterpret(ref_null,lit(array<real<4>,1>)));
    			return 0;
    		}
    )");
    ASSERT_TRUE(program);

    std::cout << "Program: " << *program << std::endl;

    auto converted = sequential::SequentialBackend::getDefault()->convert(program);

    std::cout << "Converted: \n" << *converted << std::endl;

    string code = toString(*converted);
    EXPECT_PRED2(notContainsSubString, code, "<?>");
    EXPECT_PRED2(containsSubString, code, "sizeof(float)");

    // try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}


TEST(FunctionCall, RefNewCalls) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    // create a function accepting a type literal
    core::TypePtr intType = manager.getLangBasic().getInt4();
    core::TypePtr type = builder.refType(intType);
    core::VariablePtr var = builder.variable(type, 1);

    core::ExpressionPtr init = builder.refNew(builder.undefinedNew(intType));
    core::DeclarationStmtPtr decl = builder.declarationStmt(var, init);


    auto converted = sequential::SequentialBackend::getDefault()->convert(decl);

    std::cout << "Converted: \n" << *converted << std::endl;

    string code = toString(*converted);
    EXPECT_PRED2(notContainsSubString, code, "<?>");
    EXPECT_PRED2(containsSubString, code, "int32_t* var_1 = (int32_t*)malloc(sizeof(int32_t))");

}

TEST(FunctionCall, VectorExpression) {
	core::NodeManager manager;
	core::IRBuilder builder(manager);

	core::ExpressionPtr zero = builder.literal(manager.getLangBasic().getUInt8(), "0");
	core::ExpressionPtr offset = builder.refVar(builder.vectorExpr(toVector(zero, zero, zero)));
	core::ExpressionPtr extFun = core::lang::getLiteral(manager, "(ref<vector<uint<8>,3>>)->unit", "call_vector");
	core::ExpressionPtr call = builder.callExpr(manager.getLangBasic().getUnit(), extFun, toVector(offset));

	auto converted = sequential::SequentialBackend::getDefault()->convert(call);
	string code = toString(*converted);

	EXPECT_PRED2(containsSubString, code, "call_vector((uint64_t*)(&(__insieme_type_1){{(uint64_t)0, (uint64_t)0, (uint64_t)0}}))");
}


TEST(Literals, BoolLiterals) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    // create a code fragment including the boolean constants
    core::ProgramPtr program = builder.parseProgram(
    		"int<4> main() {"
    		"	true;"
    		"	false;"
    		"	return 0;"
    		"}"
    );
    ASSERT_TRUE(program);

    std::cout << "Program: " << *program << std::endl;

    auto converted = sequential::SequentialBackend::getDefault()->convert(program);

    std::cout << "Converted: \n" << *converted << std::endl;

    string code = toString(*converted);
    EXPECT_PRED2(containsSubString, code, "true");
    EXPECT_PRED2(containsSubString, code, "false");

    // try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}

TEST(Parallel, Whatever) {
       core::NodeManager mgr;
       core::IRBuilder builder(mgr);

        // create a code fragment allocating an array on the stack and using it
        core::ProgramPtr program = builder.parseProgram(
                R"(
                let int = int<4>;
                let uint = uint<4>;

                let differentbla = lambda ('b x) -> unit {
                    decl auto m = x;
                    decl auto l = m;
                };

                let bla = lambda ('a f) -> unit {
                    let anotherbla = lambda ('a x) -> unit {
                        decl auto m = x;
                    };
                    anotherbla(f);
                    differentbla(f);
                    parallel(job { decl auto l = f; });
                };

                int main() {
                    // some bla
                    decl int x = 10;
                    bla(x);
                    return 0;
                }
                )"
        );
        std::cout << "Program: " << *program << std::endl;

        auto converted = sequential::SequentialBackend::getDefault()->convert(program);
        std::cout << "Converted Seq: \n" << *converted << std::endl;
        auto converted_rt = runtime::RuntimeBackend::getDefault()->convert(program);
        std::cout << "Converted Run: \n" << *converted_rt << std::endl;
}


TEST(Arrays, Allocation) {
	core::NodeManager manager;
	core::IRBuilder builder(manager);

	// create a code fragment allocating an array on the stack and using it
	core::ProgramPtr program = builder.parseProgram(
			R"(
			let int = int<4>;
			let uint = uint<4>;

			int main() {
				// determine array size
				decl uint size = 10u;

				// create two arrays on the stack and the heap
				decl ref<array<int,1>> a = var(array_create_1D(lit(int), size));
				decl ref<array<int,1>> b = new(array_create_1D(lit(int), size));

				// use the two arrays
				a[2] = 123;
				b[4] = 321;
			}
			)"
	);

	ASSERT_TRUE(program);

	std::cout << "Program: " << std::endl; dump(program); std::cout << std::endl;

	auto converted = sequential::SequentialBackend::getDefault()->convert(program);

	std::cout << "Converted: \n" << *converted << std::endl;

	string code = toString(*converted);
	EXPECT_PRED2(containsSubString, code, "int32_t a[size];");
	EXPECT_PRED2(containsSubString, code, "int32_t* b = malloc(sizeof(int32_t) * size)");

	// try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}

TEST(PrimitiveType, LongLong) {
	core::NodeManager manager;
	core::IRBuilder builder(manager);

	// create a code fragment allocating an array on the stack and using it
	core::ProgramPtr program = builder.parseProgram(
			R"(
			let longlong = int<16>;

			int<4> main() {
				// just create a long-long variable
				decl int<16> a = 10l;
				// just create a long-long variable
				decl uint<16> b = 10ul;
			}
			)"
	);

	ASSERT_TRUE(program);

	std::cout << "Program: " << std::endl; dump(program); std::cout << std::endl;

	auto converted = sequential::SequentialBackend::getDefault()->convert(program);

	std::cout << "Converted: \n" << *converted << std::endl;

	string code = toString(*converted);
	EXPECT_PRED2(containsSubString, code, "long long a = (int64_t)10l;");
	EXPECT_PRED2(containsSubString, code, "unsigned long long b = (uint64_t)10ul;");

	// try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}


TEST(References, RefAny) {
	core::NodeManager manager;
	core::IRBuilder builder(manager);

	// create a code fragment allocating an array on the stack and using it
	core::ProgramPtr program = builder.parseProgram(
			R"(
			int<4> main() {
				decl ref<any> x;
				decl ref<ref<any>> y = var(x);
				decl ref<ref<any>> z = new(x);

				lit("w":ref<ref<any>>);
			}
			)"
	);

	ASSERT_TRUE(program);

	std::cout << "Program: " << std::endl; dump(program); std::cout << std::endl;

	auto converted = sequential::SequentialBackend::getDefault()->convert(program);

	std::cout << "Converted: \n" << *converted << std::endl;

	string code = toString(*converted);
	EXPECT_PRED2(containsSubString, code, "void* x;");
	EXPECT_PRED2(containsSubString, code, "void* y = &x;");
	EXPECT_PRED2(containsSubString, code, "void** z = _ref_new___insieme_type_2(&x);");
	EXPECT_PRED2(containsSubString, code, "void* w;");
	EXPECT_PRED2(containsSubString, code, "&w;");

	// try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}


TEST(FunctionCall, GenericFunctionsWithLazy) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    core::ProgramPtr program = builder.parseProgram(R"(
    	int<4> main() {
    		
    		let f = lambda (ref<'a> a, ('a)=>bool c, ()=>'a v)->ref<'a> {
    			if(c(*a)) {
    				a = v();
    			}
    		};
    		
    		decl ref<int<4>> a = var(1);
    		decl ref<real<4>> b = var(2.0f);
    		
    		f(a, lambda (int<4> a)=> true, lambda ()=>3);
			f(b, lambda (real<4> b)=> true, lambda ()=>4.0f);
			
    		return 0;
    	}
    )");

    ASSERT_TRUE(program);

    // check for semantic errors
    EXPECT_TRUE(core::checks::check(program).empty()) << core::checks::check(program);

    LOG(INFO) << "Converting IR to C...";
    auto converted = sequential::SequentialBackend::getDefault()->convert(program);
    LOG(INFO) << "Printing converted code: " << *converted;

    string code = toString(*converted);

    EXPECT_PRED2(notContainsSubString, code, "<?>");
    EXPECT_PRED2(notContainsSubString, code, "<a>");
    EXPECT_PRED2(notContainsSubString, code, "UNSUPPORTED");

    // try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}

TEST(FunctionCall, PassLabmdaToBind) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    core::ProgramPtr program = builder.parseProgram(
    	"int<4> main() {"
    	"	"
    	"	let f = lambda ()->int<4> { return 4; };"
    	"	let g = lambda (()=>'a a)->'a { return a(); };"
    	"	"
    	"	g(f);"
		"	"
    	"	return 0;"
    	"}"
    );

    ASSERT_TRUE(program);

    // check for semantic errors
    EXPECT_TRUE(core::checks::check(program).empty()) << core::checks::check(program);

    LOG(INFO) << "Converting IR to C...";
    auto converted = sequential::SequentialBackend::getDefault()->convert(program);
    LOG(INFO) << "Printing converted code: " << *converted;

    string code = toString(*converted);

    EXPECT_PRED2(notContainsSubString, code, "<?>");
    EXPECT_PRED2(notContainsSubString, code, "<a>");
    EXPECT_PRED2(notContainsSubString, code, "UNSUPPORTED");

    // try compiling the code fragment
	utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
	compiler.addFlag("-lm");
	compiler.addFlag("-c"); // do not run the linker
	EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
}

TEST(FunctionCall, DebugCodePrinting) {
    core::NodeManager manager;
    core::IRBuilder builder(manager);

    core::ProgramPtr program = builder.parseProgram(
    	"let int = int<4>;"
    	""
    	"let f = lambda ()->int { return 4; };"
    	""
    	"let g = lambda (int x)->int { return x + 2; };"
    	""
    	"int main() {"
		"	return g(f());"
    	"}"
    );

    ASSERT_TRUE(program);

    // without debug code
    {

		// check for semantic errors
		EXPECT_TRUE(core::checks::check(program).empty()) << core::checks::check(program);

		// create backend instance
		auto be = sequential::SequentialBackend::getDefault();

		// upbdate backend configuration
		be->getConfiguration().addIRCodeAsComment = false;

		LOG(INFO) << "Converting IR to C...";
		auto converted = be->convert(program);
		LOG(INFO) << "Printing converted code: " << *converted;

		string code = toString(*converted);

		EXPECT_PRED2(notContainsSubString, code, "<?>");
		EXPECT_PRED2(notContainsSubString, code, "<a>");
		EXPECT_PRED2(notContainsSubString, code, "UNSUPPORTED");
		EXPECT_PRED2(notContainsSubString, code, "{\n    v0 = fun() -> int<4> {\n        return g(f());\n    };\n}");

		// try compiling the code fragment
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-lm");
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
    }

    // with debug code
	{

		// check for semantic errors
		EXPECT_TRUE(core::checks::check(program).empty()) << core::checks::check(program);

		// create backend instance
		auto be = sequential::SequentialBackend::getDefault();

		// upbdate backend configuration
		be->getConfiguration().addIRCodeAsComment = true;

		LOG(INFO) << "Converting IR to C...";
		auto converted = be->convert(program);
		LOG(INFO) << "Printing converted code: " << *converted;

		string code = toString(*converted);

		EXPECT_PRED2(notContainsSubString, code, "<?>");
		EXPECT_PRED2(notContainsSubString, code, "<a>");
		EXPECT_PRED2(notContainsSubString, code, "UNSUPPORTED");
		EXPECT_PRED2(containsSubString, code, "{\n    v0 = fun() -> int<4> {\n        return g(f());\n    };\n}");

		// try compiling the code fragment
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-lm");
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
	}
}

} // namespace backend
} // namespace insieme
