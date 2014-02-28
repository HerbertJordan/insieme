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

#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_class_info.h"
#include "insieme/core/checks/full_check.h"
#include "insieme/core/lang/ir++_extension.h"
#include "insieme/core/lang/static_vars.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/backend/sequential/sequential_backend.h"
#include "insieme/utils/compiler/compiler.h"

#include "insieme/utils/logging.h"
#include "insieme/utils/test/test_utils.h"

namespace insieme {
namespace backend {

	TEST(CppSnippet, HelloWorld) {

		core::NodeManager manager;
		core::IRBuilder builder(manager);

		// create a code fragment including some member functions
		core::ProgramPtr program = builder.parseProgram(
				"let int = int<4>;"
				"let Math = struct {};"
				""
				"let id = Math::(int a)->int { return a; };"
				""
				"let sum = Math::(int a, int b)->int { return a + b; };"
				""
				"int main() {"
				"	ref<Math> m;"
				"	"
				"	print(\"%d\\n\", m.id(12));"
				"	print(\"%d\\n\", m.sum(12,14));"
				"}"
		);

		ASSERT_TRUE(program);
		std::cout << "Program: " << *program << std::endl;
		EXPECT_TRUE(core::checks::check(program).empty()) << core::checks::check(program);

		// use sequential backend to convert into C++ code
		auto converted = sequential::SequentialBackend::getDefault()->convert(program);
		ASSERT_TRUE((bool)converted);
		std::cout << "Converted: \n" << *converted << std::endl;

		// try compiling the code fragment
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*converted, compiler));
	}


	TEST(CppSnippet, Counter) {

		core::NodeManager manager;
		core::IRBuilder builder(manager);

		// create a code fragment implementing a counter class
		core::ProgramPtr program = builder.parseProgram(
				R"(
				let int = int<4>;
				
				let Counter = struct {
					int value;
				};
				
				let reset = Counter::()->unit {
					this->value = 0;
				};
				
				let inc = Counter::()->int {
					this->value = this->value + 1;
					return *this->value;
				};
				
				let dec = Counter::()->int {
					this->value = this->value - 1;
					return *this->value;
				};
				
				let get = Counter::()->int {
					return *this->value;
				};
				
				let set = Counter::(int x)->unit { 
					this->value = x;
				};
				
				let p = Counter::()->unit {
					print("%d\n", this->get());
				};
				
				int main() {
					ref<Counter> c;
					c.reset();
					c.p();
					c.inc();
					c.p();
					c.inc();
					c.p();
					c.dec();
					c.p();
					c.set(14);
					c.p();
				}
				)"
		);

		ASSERT_TRUE(program);
		std::cout << "Program: " << *program << std::endl;
		EXPECT_TRUE(core::checks::check(program).empty()) << core::checks::check(program);

		// use sequential backend to convert into C++ code
		auto converted = sequential::SequentialBackend::getDefault()->convert(program);
		ASSERT_TRUE((bool)converted);
		std::cout << "Converted: \n" << *converted << std::endl;

		// try compiling the code fragment
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		// compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*converted, compiler));

	}

	TEST(CppSnippet, Inheritance) {

		core::NodeManager manager;
		core::IRBuilder builder(manager);

		// create a code fragment implementing a counter class
		core::ProgramPtr program = builder.parseProgram(
				R"(
				let int = int<4>;

				let A = struct {
					int x;
				};

				let B = struct : A {
					int y;
				};

				let C = struct : B {
					int z;
				};

				int main() {

					// -------- handle an instance of A --------
					ref<A> a;
					a.x = 1;
					
					
					// -------- handle an instance of B --------
					ref<B> b;
					
					// direct access
					b.as(A).x = 1;
					b.y = 2;
					
					// indirect access of A's x
					auto bA = b.as(A);
					bA.x = 3;
					
					
					// -------- handle an instance of C --------
					ref<C> c;
					
					// access B's A's x
					c.as(B).as(A).x = 1;

					// access C's A's x
					c.as(B).y = 2;
					
					c.z = 3;
				}
				)"
		);

		ASSERT_TRUE(program);
		std::cout << "Program: " << core::printer::PrettyPrinter(program) << std::endl;
		EXPECT_TRUE(core::checks::check(program).empty()) << core::checks::check(program);

		// use sequential backend to convert into C++ code
		auto converted = sequential::SequentialBackend::getDefault()->convert(program);
		ASSERT_TRUE((bool)converted);
		std::cout << "Converted: \n" << *converted << std::endl;

		// try compiling the code fragment
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*converted, compiler));

	}

	TEST(CppSnippet, ClassMetaInfo) {

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		// create a class type
		auto counterType = builder.parseType("let Counter = struct { int<4> value; } in Counter");
		ASSERT_TRUE(counterType);

		// create symbol map for remaining task
		std::map<string, core::NodePtr> symbols;
		symbols["Counter"] = counterType;

		auto parse = [&](const string& code) { return builder.parseExpr(code, symbols).as<core::LambdaExprPtr>(); };
		auto parseType = [&](const string& code) { return builder.parseType(code, symbols).as<core::FunctionTypePtr>(); };

		// add member functions to meta info
		core::ClassMetaInfo info;


		// ------- Constructor ----------

		// default
		info.addConstructor(parse("Counter::() { }"));

		// with value
		info.addConstructor(parse("Counter::(int<4> x) { this->value = x; }"));

		// copy constructor
		info.addConstructor(parse("Counter::(ref<Counter> c) { this->value = *c->value; }"));

		// ------- member functions ----------

		// a non-virtual, const function
		info.addMemberFunction("get", parse("Counter::()->int<4> { return *this->value; }"), false, true);

		// a non-virtual, non-const function
		info.addMemberFunction("set", parse("Counter::(int<4> x)->unit { this->value = x; }"), false, false);

		// a virtual, const function
		info.addMemberFunction("print", parse(R"(Counter::()->unit { print("%d\n", *this->value); })"), true, true);

		// a virtual, non-const function
		info.addMemberFunction("clear", parse("Counter::()->unit { }"), true, false);

		// a pure virtual, non-const function
		info.addMemberFunction("dummy1", builder.getPureVirtual(parseType("Counter::()->int<4>")), true, false);

		// a pure virtual, const function
		info.addMemberFunction("dummy2", builder.getPureVirtual(parseType("Counter::()->int<4>")), true, true);

		std::cout << info << "\n";

		// attach
		core::setMetaInfo(counterType, info);

		// verify proper construction
		EXPECT_TRUE(core::checks::check(counterType).empty()) << core::checks::check(counterType);

		// ------------ create code using the counter type --------

		auto prog = builder.parseProgram("int<4> main() { ref<ref<Counter>> c; return *(*c->value); }", symbols);

		// generate code
		auto targetCode = sequential::SequentialBackend::getDefault()->convert(prog);
		ASSERT_TRUE((bool)targetCode);

		// check generated code
		string code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "Counter();");
		EXPECT_PRED2(containsSubString, code, "Counter(int32_t p2);");
		EXPECT_PRED2(containsSubString, code, "Counter(Counter* p2);");

		EXPECT_PRED2(containsSubString, code, "int32_t get() const;");
		EXPECT_PRED2(containsSubString, code, "void set(int32_t p2);");
		EXPECT_PRED2(containsSubString, code, "virtual void print() const;");
		EXPECT_PRED2(containsSubString, code, "virtual void clear();");

		EXPECT_PRED2(containsSubString, code, "virtual int32_t dummy1() =0;");
		EXPECT_PRED2(containsSubString, code, "virtual int32_t dummy2() const =0;");

//		std::cout << *targetCode;

		// try compiling the code fragment
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));


		// -------------------------------------------- add destructor -----------------------------------------

		info.setDestructor(parse("~Counter::() {}"));
		core::setMetaInfo(counterType, info);
		EXPECT_TRUE(core::checks::check(counterType).empty()) << core::checks::check(counterType);

		targetCode = sequential::SequentialBackend::getDefault()->convert(prog);
		ASSERT_TRUE((bool)targetCode);

//		std::cout << *targetCode;

		// check generated code
		code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "~Counter();");
		EXPECT_PRED2(notContainsSubString, code, "virtual ~Counter();");
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));


		// ---------------------------------------- add virtual destructor -----------------------------------------

		info.setDestructorVirtual();
		core::setMetaInfo(counterType, info);
		EXPECT_TRUE(core::checks::check(counterType).empty()) << core::checks::check(counterType);

		targetCode = sequential::SequentialBackend::getDefault()->convert(prog);
		ASSERT_TRUE((bool)targetCode);

//		std::cout << *targetCode;

		// check generated code
		code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "virtual ~Counter();");
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));

	}


	TEST(CppSnippet, VirtualFunctionCall) {

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		std::map<string, core::NodePtr> symbols;

		// create a class A with a virtual function
		core::TypePtr classA = builder.parseType("let A = struct { } in A");
		symbols["A"] = classA;

		auto funType = builder.parseType("A::(int<4>)->int<4>", symbols).as<core::FunctionTypePtr>();

		core::ClassMetaInfo infoA;
		infoA.addMemberFunction("f", builder.getPureVirtual(funType), true);
		core::setMetaInfo(classA, infoA);

		// create a class B
		core::TypePtr classB = builder.parseType("let B = struct : A { } in B", symbols);
		symbols["B"] = classB;

		core::ClassMetaInfo infoB;
		infoB.addMemberFunction("f", builder.parseExpr("B::(int<4> x)->int<4> { return x + 1; }", symbols).as<core::LambdaExprPtr>(), true);
		core::setMetaInfo(classB, infoB);

		auto res = builder.parseProgram(
				"let f = lit(\"f\":A::(int<4>)->int<4>);"
				""
				"let ctorB1 = B::() { };"
				"let ctorB2 = B::(int<4> x) { };"
				""
				"int<4> main() {"
				"	ref<A> x = ctorB1(new(B));"
				"	ref<A> y = ctorB2(new(B), 5);"
				"	x->f(3);"
				"	delete(x);"
				"	return 0;"
				"}", symbols);

		ASSERT_TRUE(res);

		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "(*x).f(3);");

		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));

	}

	TEST(CppSnippet, ConstructorCall) {

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		// create a example code using all 3 ctor variants
		auto res = builder.parseProgram(
				R"(
					let int = int<4>;
					
					let A = struct { int x; };
					
					let ctorA1 = A::() { this->x = 0; };
					let ctorA2 = A::(int x) { this->x = x; };
					
					int main() {
						// on stack
						ref<A> a1 = ctorA1(var(A));
						ref<A> a2 = ctorA2(var(A), 1);
					
						// on heap
						ref<A> a3 = ctorA1(new(A));
						ref<A> a4 = ctorA2(new(A), 1);
					
						// in place
						ref<A> a5; ctorA1(a5);
						ref<A> a6; ctorA2(a6, 1);
					
						return 0;
					}
				)"
		);

		ASSERT_TRUE(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "A a1;");
		EXPECT_PRED2(containsSubString, code, "A a2(1);");
		EXPECT_PRED2(containsSubString, code, "A* a3 = new A();");
		EXPECT_PRED2(containsSubString, code, "A* a4 = new A(1);");
		EXPECT_PRED2(containsSubString, code, "new (&a5) A();");
		EXPECT_PRED2(containsSubString, code, "new (&a6) A(1);");

		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));

	}

	TEST(CppSnippet, DestructorCall) {

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		auto res = builder.parseProgram(
				R"(
					let int = int<4>;
					
					let A = struct { int x; };
					
					let ctorA = A::(int<4> x) { 
						this->x = x; 
						print("Creating: %d\n", x);
					};
					
					let dtorA = ~A::() { 
						print("Clearing: %d\n", *this->x);
						this->x = 0; 
					};
					
					int main() {
					
						// create an un-initialized memory location
						ref<A> a = ctorA(new(A), 3);
						
						// init using in-place constructor
						ctorA(a, 2);
					
						// invoce destructor
						dtorA(a);
					
						return 0;
					}
				)"
		);

		ASSERT_TRUE(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "(*a).A::~A()");

		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));

	}

	TEST(CppSnippet, ArrayConstruction) {

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		std::map<string, core::NodePtr> symbols;
		symbols["createArray"] = mgr.getLangExtension<core::lang::IRppExtensions>().getArrayCtor();

		auto res = builder.parseProgram(
				R"(
					let int = int<4>;
					
					let A = struct { int x; };
					
					let ctorA = A::() { 
						this->x = 4; 
					};
					
					int main() {
						
						// create an array of objects of type A on the stack
						ref<array<A,1>> a = createArray(ref.var, ctorA, 5u);
						
						// create an array of objects of type A on the heap
						ref<array<A,1>> b = createArray(ref.new, ctorA, 5u);
						
						// update an element
						a[3]->x = 12;
						b[3]->x = 12;
						
						return 0;
					}
				)",
				symbols
		);

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "A a[5u];");
		EXPECT_PRED2(containsSubString, code, "A* b = new A[5u];");

		EXPECT_PRED2(containsSubString, code, "a[3].x = 12;");
		EXPECT_PRED2(containsSubString, code, "b[3].x = 12;");


		// check whether ctor is present!
		EXPECT_PRED2(containsSubString, code, "A();");
		EXPECT_PRED2(containsSubString, code, "A::A() : x(4) {");

		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}

	TEST(CppSnippet, VectorConstruction) {

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		std::map<string, core::NodePtr> symbols;
		symbols["createVector"] = mgr.getLangExtension<core::lang::IRppExtensions>().getVectorCtor();

		auto res = builder.parseProgram(
				R"(
					let int = int<4>;
					
					let A = struct { int x; };
					
					let ctorA = A::() { 
						this->x = 4; 
					};
					
					int main() {
						
						let size = lit("not important" : intTypeParam<5>);

						// create an array of objects of type A on the stack
						ref<array<A,1>> a = createVector(ref.var, ctorA, size);
						
						// create an array of objects of type A on the heap
						ref<array<A,1>> b = createVector(ref.new, ctorA, size);
						
//						// create an array of objects of type A on the stack
//						ref<vector<A,5>> c = createVector(ref.var, ctorA, size);
//						
//						// create an array of objects of type A on the heap
//						ref<vector<A,5>> d = createVector(ref.new, ctorA, size);

						// update an element
						a[3]->x = 12;
						b[3]->x = 12;
//						c[3]->x = 12;
//						d[3]->x = 12;
						
						return 0;
					}
				)",
				symbols
		);

		// TODO: also support ref<vector<X,y>> as the value type

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "A a[5u];");
		EXPECT_PRED2(containsSubString, code, "A* b = new A[5u];");
//		EXPECT_PRED2(containsSubString, code, "A c[5u];");
//		EXPECT_PRED2(containsSubString, code, "A* d = new A[5u];");

		EXPECT_PRED2(containsSubString, code, "a[3].x = 12;");
		EXPECT_PRED2(containsSubString, code, "b[3].x = 12;");
//		EXPECT_PRED2(containsSubString, code, "c[3].x = 12;");
//		EXPECT_PRED2(containsSubString, code, "d[3].x = 12;");


		// check whether ctor is present!
		EXPECT_PRED2(containsSubString, code, "A();");
		EXPECT_PRED2(containsSubString, code, "A::A() : x(4) {");

		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}

	TEST(CppSnippet, InitializerList) {

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		auto res = builder.normalize(builder.parseProgram(
				R"(
					let int = int<4>;
					
					let B = struct { };
					let A = struct : B { int x; int y; int z; };

					let ctorA = A::(int y) {
						this->y = y; 
						for ( int i = 0 .. 10 ) {
							this->y = 1;
							this->z = 3;
						}
						this->x = 4; 
						this->z = 2;
					};
					
					int main() {
						
						// call constructor
						ref<A> a = ctorA(ref.var(undefined(lit(A))), 10);
						
						return 0;
					}
				)"
		));

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

//		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "A var_1(10);");
		EXPECT_PRED2(containsSubString, code, "A::A(int32_t y) : x(4), y(y) {");
		EXPECT_PRED2(containsSubString, code, "(*this).z = 2;");

		EXPECT_PRED2(notContainsSubString, code, "(*this).x = 4;");

		// check whether code is compiling
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}


	TEST(CppSnippet, InitializerList2) {

		// something including a super-constructor call

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		auto res = builder.normalize(builder.parseProgram(
				R"(
					let int = int<4>;
					
					let A = struct { int x; };
					let B = struct : A { int y; };

					let ctorA = A::(int x) {
						this->x = x;
					};


					let ctorB = B::(int x, int y) {
						ctorA(this, x);
						this->y = y; 
					};
					
					int main() {
						
						// call constructor
						ref<B> b = ctorB(ref.var(undefined(lit(B))), 1, 2);
						
						return 0;
					}
				)"
		));

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

//		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "B var_1(1, 2);");
		EXPECT_PRED2(containsSubString, code, "A::A(int32_t x) : x(x) {");
		EXPECT_PRED2(containsSubString, code, "B::B(int32_t x, int32_t var_3) : A(x), y(var_3) {");

		// check whether code is compiling
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}

	TEST(CppSnippet, InitializerList3) {

		// something including a super-constructor call

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		auto res = builder.normalize(builder.parseProgram(
				R"(
					let int = int<4>;
					
					let A = struct { int x; int y; };
					let B = struct : A { int z; };

					let ctorA = A::(int x, int y) {
						this->x = x;
						this->y = y;
					};


					let ctorB = B::(int x, int y, int z) {
						ctorA(this, x, y + z);
						this->z = z; 
					};
					
					int main() {
						
						// call constructor
						ref<B> b = ctorB(ref.var(undefined(lit(B))), 1, 2, 3);
						
						return 0;
					}
				)"
		));

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

//		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "B var_1(1, 2, 3);");
		EXPECT_PRED2(containsSubString, code, "A::A(int32_t x, int32_t y) : x(x), y(y) {");
		EXPECT_PRED2(containsSubString, code, "B::B(int32_t x, int32_t y, int32_t var_4) : A(x, y + var_4), z(var_4) {");

		// check whether code is compiling
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}

	TEST(CppSnippet, InitializerList4) {

		// something including a non-parameter!

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		auto res = builder.normalize(builder.parseProgram(
				R"(
					let int = int<4>;
					
					let A = struct { int x; int y; };

					let ctorA = A::(int x, int y) {
						this->x = x;
						this->y = this->x + y;
					};

					int main() {
						
						// call constructor
						ref<A> b = ctorA(ref.var(undefined(lit(A))), 1, 2);
						
						return 0;
					}
				)"
		));

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

//		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "A var_1(1, 2);");
		EXPECT_PRED2(containsSubString, code, "A::A(int32_t x, int32_t y) : x(x) {");

		// check whether code is compiling
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}

	TEST(CppSnippet, Exceptions) {

		// something including a non-parameter!

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		auto res = builder.normalize(builder.parseProgram(
				R"(
					let short = int<2>;
					let int = int<4>;

					int main() {
						
						try {

							throw 4;

						} catch (int x) {
							print("Catched integer %d\n", x);
						} catch (short y) {
							print("Catched short %d\n", y);
						} catch (ref<short> y) {
							print("Catched short %d\n", *y);
						} catch (any a) {
							print("Catched something!\n");
						}

						return 0;
					}
				)"
		));

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, "throw 4;");
		EXPECT_PRED2(containsSubString, code, "} catch(int32_t var_1) {");
		EXPECT_PRED2(containsSubString, code, "} catch(int16_t var_2) {");
		EXPECT_PRED2(containsSubString, code, "} catch(int16_t* var_3) {");
		EXPECT_PRED2(containsSubString, code, "} catch(...) {");

		// check whether code is compiling
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}


	TEST(CppSnippet, StaticVariableConst) {

		// something including a non-parameter!

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		auto& ext = mgr.getLangExtension<core::lang::StaticVariableExtension>();

		std::map<string, core::NodePtr> symbols;
		symbols["init"] = ext.getInitStaticConst();

		auto res = builder.normalize(builder.parseProgram(
				R"(
					let int = int<4>;
					let a = lit("a":ref<struct __static_var { bool initialized; int value; }>);

					int main() {
						
						init(a, 5);

						return 0;
					}
				)", symbols
		));

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, " static int32_t a = 5");

		// check whether code is compiling
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultC99Compiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}

	TEST(CppSnippet, StaticVariable) {

		// something including a non-parameter!

		core::NodeManager mgr;
		core::IRBuilder builder(mgr);

		auto& ext = mgr.getLangExtension<core::lang::StaticVariableExtension>();

		std::map<string, core::NodePtr> symbols;
		symbols["init"] = ext.getInitStaticLazy();

		auto res = builder.normalize(builder.parseProgram(
				R"(
					let int = int<4>;
					let a = lit("a":ref<struct __static_var { bool initialized; int value; }>);

					int main() {
						
						init(a, ()=> 1);
						init(a, ()=> 2);

						return 0;
					}
				)", symbols
		));

		ASSERT_TRUE(res);
		EXPECT_TRUE(core::checks::check(res).empty()) << core::checks::check(res);

		auto targetCode = sequential::SequentialBackend::getDefault()->convert(res);
		ASSERT_TRUE((bool)targetCode);

		std::cout << *targetCode;

		// check generated code
		auto code = toString(*targetCode);
		EXPECT_PRED2(containsSubString, code, " static int32_t a = __insieme_type_");

		// check whether code is compiling
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getDefaultCppCompiler();
		compiler.addFlag("-c"); // do not run the linker
		EXPECT_TRUE(utils::compiler::compile(*targetCode, compiler));
	}
} // namespace backend
} // namespace insieme

