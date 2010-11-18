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

// ignore warnings
#pragma GCC diagnostic ignored "-Wall"

void decl_stmt_test() {
	#pragma test \
	"decl ref<int<4>> v1 = ( var(0))"
	int b = 0;

	#pragma test \
	"decl ref<uint<4>> v1 = ( var(0))"
	unsigned int c;

	#pragma test \
	"decl ref<real<4>> v1 = ( var(0.0))"
	float d;

	// ...
}

// Simple struct
void binary_op_test() {

	int a = 0, b = 0;
	unsigned c;

	#pragma test \
	"(( *v2)+( *v1))"
	a + b;

	#pragma test \
	"(v2 := (( *v2)+( *v1)))"
	a += b;

	#pragma test \
	"(( *v2)-( *v1))"
	a - b;

	#pragma test \
	"(( *v1)-CAST<uint<4>>(1))"
	c - 1;

	#pragma test \
	"(v2 := (( *v2)-( *v1)))"
	a -= b;

	#pragma test \
	"<?>([v1, v2]rec v5.{v5=fun[ref<int<4>> v3, ref<int<4>> v4]() {int.add(ref.deref(v4), 1); return int.sub(ref.deref(v3), 1);}})</?>()"
	(a+1, b-1);
}

void unary_op_test() {

	#pragma test "decl ref<int<4>> v1 = ( var(0))"
	int a = 0;

//	#pragma test "bool.not(a)" TODO
	!a;

	#pragma test "v1"
	+a;

//	#pragma test "a" TODO
	-a;

	// #pragma test "decl ref<array<ref<int<4>>,1>> v2 = ( var(v1))"
	int* b = &a;

	//#pragma test "(( *v1)[0])"
	*b;

	#pragma test \
	"<?>([v1]rec v4.{v4=fun[ref<int<4>> v3]() {int<4> v2 = ref.deref(v3); ref.assign(v3, int.add(ref.deref(v3), cast<int<4>>(1))); return v2;}})</?>()"
	a++;

	#pragma test \
	"<?>([v1]rec v4.{v4=fun[ref<int<4>> v3]() {int<4> v2 = ref.deref(v3); ref.assign(v3, int.sub(ref.deref(v3), cast<int<4>>(1))); return v2;}})</?>()"
	a--;

	#pragma test \
	"<?>([v1]rec v3.{v3=fun[ref<int<4>> v2]() {ref.assign(v2, int.add(ref.deref(v2), cast<int<4>>(1))); ref.deref(v2);}})</?>()"
	++a;

	#pragma test \
	"<?>([v1]rec v3.{v3=fun[ref<int<4>> v2]() {ref.assign(v2, int.sub(ref.deref(v2), cast<int<4>>(1))); ref.deref(v2);}})</?>()"
	--a;
}


struct Person { int weigth; int age; };

void member_access_test() {
	struct Person p;

	#pragma test "( *v1).weigth"
	p.weigth;

	struct Person* ptr = &p;
	//#pragma test "( *(( *v1)[0])).age"
	ptr->age;
}

void if_stmt_test() {

	int cond = 0;

	#pragma test \
	"if(CAST<bool>(( *v1))) { (v1 := (( *v1)+1));} else { (v1 := (( *v1)-1));}"
	if(cond) {
		cond += 1;
	} else {
		cond -= 1;
	}

	#pragma test \
	"if((( *v1)==0)) { (v1 := (( *v1)+( *v1)));} else { }"
	if(cond == 0) {
		cond += cond;
	}

	int a=1;
	#pragma test \
	"<?>([v1]rec v3.{v3=fun[ref<int<4>> v2]() {if(cast<bool>(ref.deref(v2))) return int.add(ref.deref(v2), 1) else return int.sub(ref.deref(v2), 1);}})</?>()" // FIXME (use ITE)
	a ? a+1 : a-1;

	#pragma test \
	"<?>([v1]rec v3.{v3=fun[ref<int<4>> v2]() {if(int.eq(ref.deref(v2), 0)) return int.add(ref.deref(v2), 1) else return int.sub(ref.deref(v2), 1);}})</?>()" // FIXME (use ITE)
	a == 0 ? a+1 : a-1;
}

void for_stmt_test() {

	int it = 0;
	int a = 1;

	// standard for loop
	#pragma test \
	"for(decl ref<int<4>> v1 = ( var(0)) .. 100 : 1) { { };}"
	for(int i=0; i<100; i++) { ; }

	// for loop using a variable declared outside
	#pragma test \
	"{ for(decl ref<int<4>> v3 = ( var(0)) .. 100 : 1) { (v2 := ( *v3)); }; (v1 := 100);}"
	for(it=0; it<100; ++it) { a=it; }

	#pragma test \
	"{ for(decl ref<int<4>> v3 = ( var(( *v2))) .. 100 : 6) { (v2 := ( *v3)); }; (v1 := 100);}"
	for(it=a; it<100; it+=6) { a=it; }

	#pragma test \
	"while((( *v1)<100)) { { { }; }; (v1 := (( *v1)+1));}"
	for(; it<100; it+=1) { ; }

	#pragma test \
	"{ decl ref<int<4>> v3 = ( var(1)); decl ref<int<4>> v4 = ( var(2)); for(decl ref<int<4>> v1 = ( var(0)) .. 100 : 1) { (v2 := ( *v1)); };}"
	for(int i=0,j=1,z=2; i<100; i+=1) { a=i; }

	int mq, nq;
	#pragma test \
	"{ (v1 := 0); while((( *v2)>1)) { { }; <?>([v1, v2]rec v8.{v8=fun[ref<int<4>> v6, ref<int<4>> v7]() {([v6]rec v5.{v5=fun[ref<int<4>> v4]() {int<4> v3 = ref.deref(v4); ref.assign(v4, int.add(ref.deref(v4), cast<int<4>>(1))); return v3;}})(); return ref.assign(v7, int.div(ref.deref(v7), 2));}})</?>(); };}"
    for( mq=0; nq>1; mq++,nq/=2 ) ;

	//(v1 := 0);
	//while((( *v2)>1)) {
	//	{ };
	//	fun(ref<int<4>> v5, ref<int<4>> v6) {
	//		fun(ref<int<4>> v4) {
	//			decl int<4> v3 = ( *v4);
	//			(v4 := (( *v4)+CAST<int<4>>(1)));
	//			return v3;
	//		}(v5);
	//		return (v6 := (( *v6)/2));
	//	}(v1, v2);
	//};
}

void switch_stmt_test() {

	int a=0;

	#pragma test \
	"{ decl int<4> v2 = CAST<int<4>>(( *v1)); switch(v2) { case 1: { } default: { } };}"
	switch(a) {
	case 1:
		break;
	}
	//decl int<a> v2 = CAST<int<a>>(( *v1));
	//switch(v2) {
	//	case 1: { }
	//	default: { }
	//};


	// EVIL CODE
	#pragma test \
	"{ decl int<4> v2 = CAST<int<4>>((( *v1)+8)); (v1 := (( *v1)+1)); switch(v2) { case 1: { } default: { } };}"
	switch(a+8) {
	a += 1;
	case 1:
		break;
	}
	//decl int<a> v2 = CAST<int<a>>((( *v1)+8));
	//(v1 := (( *v1)+1));
	//switch(v2) {
	//	case 1: { }
	//	default: { }
	//};


	#pragma test \
	"{ decl int<4> v2 = CAST<int<4>>(( *v1)); switch(v2) { case 0: { } default: <?>([v1]rec v5.{v5=fun[ref<int<4>> v4]() {int<4> v3 = ref.deref(v4); ref.assign(v4, int.add(ref.deref(v4), cast<int<4>>(1))); return v3;}})</?>() };}"
	switch(a) {
	case 0:
		break;
	default:
		a++;
	}
	//decl int<a> v2 = CAST<int<a>>(( *v1));
	//switch(v2) {
	//	case 0: { }
	//	default: fun(ref<int<4>> v4) {
	//		decl int<4> v3 = ( *v4);
	//		(v4 := (( *v4)+CAST<int<4>>(1)));
	//		return v3;
	//	}(v1)
	//};

	#pragma test \
	"{ decl int<4> v2 = CAST<int<4>>(( *v1)); switch(v2) { case 1: (v1 := (( *v1)+1)) case 2: { decl ref<int<4>> v3 = ( var(0)); (( *v3)+1); } default: { { decl ref<int<4>> v3 = ( var(0)); (( *v3)+1); }; (( *v1)-1); } };}"
	switch(a) {
	case 1:
		a+=1;
		break;
	case 2:
		{ int c; c+1; }
	default:
		a-1;
		break;
	}

	//decl int<a> v2 = CAST<int<a>>(( *v1));
	//switch(v2) {
	//	case 1: (v1 := (( *v1)+1))
	//	case 2: {
	//		decl ref<int<4>> v3 = ( var(0));
	//		(( *v3)+1);
	//	}
	//	default: {
	//		{
	//			decl ref<int<4>> v3 = ( var(0));
	//			(( *v3)+1);
	//		};
	//		(( *v1)-1);
	//	}
	//};

}


void while_stmt_test() {
	int it = 0;
	#pragma test \
	"while(int.ne(( *v1), 0)) { (v1 := (( *v1)-1));}"
	while(it != 0) { it-=1; }
}

#pragma test "recFun v1 <?>{v1=fun[](int<4> v3) {return v2(int.sub(v3, 1));}, v2=fun[](int<4> v4) {return v1(int.add(v4, 1));}}</?>"
int f(int x) {
	return g(x-1);
}

#pragma test "recFun v1 <?>{v1=fun[](int<4> v3) {return v2(int.add(v3, 1));}, v2=fun[](int<4> v4) {return v1(int.sub(v4, 1));}}</?>"
int g(int x) {
	return f(x+1);
}
//recFun v1 {
//    v2 = fun(int<4> v4) {
//        return v1((v4+1));
//    };
//    v1 = fun(int<4> v3) {
//        return v2((v3-1));
//    };
//}

void rec_function_call_test() {
	#pragma test "recFun v1 <?>{v1=fun[](int<4> v3) {return v2(int.sub(v3, 1));}, v2=fun[](int<4> v4) {return v1(int.add(v4, 1));}}</?>(10)"
	f(10);
}

void evil(int** anything) { }

void vector_stmt_test() {

	//#pragma test "decl ref<vector<ref<int<4>>,5>> v1 = ( var(vector.initUniform(( var(0)))))" // FIXME
	int a[5];

	#pragma test \
	"vector.subscript(( *v1), CAST<uint<4>>(0))"
	a[0];

	#pragma test \
	"(vector.subscript(( *v1), CAST<uint<4>>(0)) := 1)"
	a[0] = 1;

	//#pragma test \
	"decl ref<vector<vector<ref<int<4>>,2>,2>> v1 = ( var(vector.initUniform(vector.initUniform(( var(0))))))"
	int b[2][2];

	#pragma test \
	"vector.subscript(vector.subscript(( *v1), CAST<uint<4>>(0)), CAST<uint<4>>(0))"
	b[0][0];

	#pragma test \
	"(vector.subscript(vector.subscript(( *v1), CAST<uint<4>>(1)), CAST<uint<4>>(1)) := 0)"
	b[1][1] = 0;

	//#pragma test \
	"recFun v3 <?>{v3=fun[](array<ref<array<ref<int<4>>,1>>,1> v2) {}}</?>(( *v1))"
	evil(b);
}
