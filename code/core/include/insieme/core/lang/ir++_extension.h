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

#pragma once

#include "insieme/core/lang/extension.h"

namespace insieme {
namespace core {
namespace lang {

	/**
	 * This class offers a list of IR extensions required within the class meta-info
	 * object to model IR++ concepts.
	 */
	class IRppExtensions : public core::lang::Extension {

		/**
		 * Allow the node manager to create instances of this class.
		 */
		friend class core::NodeManager;

		/**
		 * Creates a new instance based on the given node manager.
		 */
		IRppExtensions(core::NodeManager& manager)
				: core::lang::Extension(manager) {}

	public:

	//////////////////////////////////////////////////////////////////////////////////////////
	// virtuals handling
	
		/**
		 * A literal to be used to represent pure virtual functions.
		 */
		LANG_EXT_LITERAL(PureVirtual, "<pure virtual>", "(type<'a>)->'a");


	//////////////////////////////////////////////////////////////////////////////////////////
	// constructor behaviour
	
		/**
		 * A construct supporting the construction and initialization of an array
		 * of objects.
		 */
		LANG_EXT_DERIVED(ArrayCtor,
				"let int = uint<8> in "
				""
				"(('a)->ref<'a> allocator, 'b::() ctor, int size)->ref<array<'b,1>> { "
				"	// define the type to be allocated \n"
 				"	let wrapper = struct { int size; array<'b,1> data; }; "
				"	"
				"	// allocate the memory \n"
				"	ref<wrapper> res = allocator((wrapper){ size, array.create.1D(lit('b), size) }); "
				"	"
				"	// init elements \n"
				"	for(int i=0u .. size) {"
				"		ctor(res->data[i]);"
				"	}"
				"	"
				"	// return array reference \n"
				"	return res->data;"
				"}"
		);

		/**
		 * A construct supporting the construction and initialization of a vector
		 * of objects.
		 */
		LANG_EXT_DERIVED(VectorCtor,
				"let int = uint<8> in "
				""
				"(('a)->ref<'a> allocator, 'b::() ctor, intTypeParam<#s> size)->ref<vector<'b,#s>> { "
				"	// define the type to be allocated \n"
				"	let wrapper = struct { int size; vector<'b,#s> data; }; "
				"	"
				"	// allocate the memory \n"
				"	ref<wrapper> res = allocator((wrapper){ to.uint(size), vector.init.undefined(lit('b), size) });"
				"	"
				"	// init elements \n"
				"	for(int i=0u .. *res->size) {"
				"		ctor(res->data[i]);"
				"	}"
				"	"
				"	// return array reference \n"
				"	return res->data;"
				"}"
		);

		/**
		 * A destructor supporting the destruction of an array of objects.
		 */
		LANG_EXT_DERIVED(ArrayDtor,
				"let int = uint<8> in "
				""
				"(ref<array<'b,1>> data, (ref<'a>)->unit deallocator, ~'b::() dtor) -> unit { "
				"	// define the type to be allocated \n"
				"	let wrapper = struct { int size; array<'b,1> data; }; "
				"	"
				"	// access wrapper struct \n"
				"	ref<wrapper> block = ref.expand(data, dp.member(dp.root, lit(\"data\")), lit(wrapper)); "
				"	"
				"	// destroy all elments within the array \n"
				"	for(int i=0u .. *(block->size)) {"
				"		dtor(block->data[i]);"
				"	}"
				"	"
				"	// free memory \n"
				"	deallocator(block);"
				"}"
		);


	//////////////////////////////////////////////////////////////////////////////////////////
	// cpp references

		/**
		 * An operator converting a C++ reference into an IR reference.
		 */
		LANG_EXT_DERIVED(RefCppToIR,
				"(struct { ref<'a> _cpp_ref } x)->ref<'a> { return x._cpp_ref; }"
		);

		/**
		 * An operator converting an IR reference into a C++ reference.
		 */
		LANG_EXT_DERIVED(RefIRToCpp,
				"let cppRef = struct { ref<'a> _cpp_ref } in "
				"(ref<'a> x)->cppRef { return (cppRef) { x }; }"
		);

		/**
		 * An operator converting a const C++ reference into an IR reference.
		 */
		LANG_EXT_DERIVED(RefConstCppToIR,
				"(struct { ref<'a> _const_cpp_ref } x)->ref<'a> { return x._const_cpp_ref; }"
		);

		/**
		 * An operator converting an IR reference into a const C++ reference.
		 */
		LANG_EXT_DERIVED(RefIRToConstCpp,
				"let cppRef = struct { ref<'a> _const_cpp_ref } in "
				"(ref<'a> x)->cppRef { return (cppRef) { x }; }"
		);

		/**
		 * An operator converting a C++ reference into a const C++ reference.
		 */
		LANG_EXT_DERIVED(RefCppToConstCpp,
				"let cppRef = struct { ref<'a> _cpp_ref } in "
				"let constCppRef = struct { ref<'a> _const_cpp_ref } in "
				"(cppRef x)->constCppRef { return (constCppRef) { x._cpp_ref }; }"
		);


	//////////////////////////////////////////////////////////////////////////////////////////
	// temporaries
	
		/**
		 * materialize a value, this operation provides a memory address to a pure right value
		 * it might be used whenever a const ref extends a value livetime.
		 * most probably, is ignored by the backend
		 */
		LANG_EXT_LITERAL(Materialize, "materialize", "('a)->ref<'a>");

	
	//////////////////////////////////////////////////////////////////////////////////////////
	//	explicit C++ casts

		/**
		 * The literal to be used for encoding a static cast operation within
		 * a pre-processing step of the backend.
		 */
		LANG_EXT_LITERAL(StaticCast, "static_cast", "(ref<'a>, type<'b>)->ref<'b>");

		LANG_EXT_LITERAL(StaticCastRefCppToRefCpp, "static_cast", 
				"let cppRefA = struct { ref<'a> _cpp_ref } in "
				"let cppRefB = struct { ref<'b> _cpp_ref } in "
				"(cppRefA, type<cppRefB>)->cppRefB");
		
		LANG_EXT_LITERAL(StaticCastConstCppToConstCpp, "static_cast", 
				"let constCppRefA = struct { ref<'a> _const_cpp_ref } in "
				"let constCppRefB = struct { ref<'b> _const_cpp_ref } in "
				"(constCppRefA, type<constCppRefB>)->constCppRefB");

		LANG_EXT_LITERAL(StaticCastRefCppToConstCpp, "static_cast", 
				"let cppRefA = struct { ref<'a> _cpp_ref } in "
				"let constCppRefB = struct { ref<'b> _const_cpp_ref } in "
				"(cppRefA, type<constCppRefB>)->constCppRefB");

		/**
		 * The literal to be used for encoding a dynamic cast operation within
		 * a pre-processing step of the backend.
		 */
		LANG_EXT_LITERAL(DynamicCast, "dynamic_cast", "(ref<'a>, type<'b>)->ref<'b>")

		LANG_EXT_LITERAL(DynamicCastRefCppToRefCpp, "dynamic_cast", 
				"let cppRefA = struct { ref<'a> _cpp_ref } in "
				"let cppRefB = struct { ref<'b> _cpp_ref } in "
				"(cppRefA, type<cppRefB>)->cppRefB");
		
		LANG_EXT_LITERAL(DynamicCastConstCppToConstCpp, "dynamic_cast", 
				"let constCppRefA = struct { ref<'a> _const_cpp_ref } in "
				"let constCppRefB = struct { ref<'b> _const_cpp_ref } in "
				"(constCppRefA, type<constCppRefB>)->constCppRefB");

		LANG_EXT_LITERAL(DynamicCastRefCppToConstCpp, "dynamic_cast", 
				"let cppRefA = struct { ref<'a> _cpp_ref } in "
				"let constCppRefB = struct { ref<'b> _const_cpp_ref } in "
				"(cppRefA, type<constCppRefB>)->constCppRefB");

		/**
		 * typeid implementation
		 */
		LANG_EXT_LITERAL(Typeid, "typeid",
			"('a)->struct { ref<std::type_info>  _const_cpp_ref; }"
		);

	//////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////
	//	member pointer (only for data member pointers)
	

		/**
		 * a member pointer needs to keep track of:
		 *  - the type the member belongs to
		 *  - the name of the field
		 *  - the field type
		 */
	//	LANG_EXT_LITERAL(MemberPointer, "memb_ptr", "struct { type<'a> objType; identifier id; type<'b> membType; };" );

		/**
		 * to construct a member pointer we provide the tree elements listed above to initialize the struct
		 */
		LANG_EXT_DERIVED(MemberPointerCtor, "let memb_ptr = struct { type<'a> objType; identifier id; type<'b> membType; } in "
											"(type<'a> obj, identifier id, type<'b> field) -> mem_ptr { " 
												"return (mem_ptr) { obj, id, field };" 
											"}");

		/**
		 * the access is done by narrowing the given object to the defined element
		 */
		LANG_EXT_DERIVED(MemberPointerAccess, "let memb_ptr = struct { type<'a> objType; identifier id; type<'b> membType; } in "
											  "(ref<'a> obj, memb_ptr m) -> ref<'b> { "
											  	"return ref.narrow(obj, db.member(db.root, m.id), m.membType);"
											  " }");
					 

	//////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////
	//	C++11 extensions



	}; // extension class

} // end namespace lang
} // end namespace core
} // end namespace insieme
