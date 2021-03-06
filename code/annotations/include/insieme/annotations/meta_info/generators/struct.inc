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

#include "clear.inc"

#define INFO_DECL(_text) _text;

#define INFO_STRUCT_BEGIN(_name) \
	namespace _name ## _ns { struct info_type; } \
	typedef _name ## _ns::info_type _name ## _info; \
	namespace _name ## _ns { \
	struct info_type; \
	VALUE_ANNOTATION_CONVERTER(info_type) \
		typedef insieme::core::value_node_annotation<info_type>::type annotation_type;\
		virtual insieme::core::ExpressionPtr toIR(insieme::core::NodeManager& manager, const insieme::core::NodeAnnotationPtr& annotation) const; \
		virtual insieme::core::NodeAnnotationPtr toAnnotation(const insieme::core::ExpressionPtr& node) const; \
	}; \
	\
	struct info_type : \
			public insieme::core::value_annotation::copy_on_migration, \
			public insieme::core::value_annotation::cloneable \
	{ \
		void cloneTo(const insieme::core::NodePtr& target) const; \
		bool operator==(const info_type& other) const; \

#define INFO_FIELD_EXT(_name, _a, _b, _type, _def) \
		typename detail::compiler_type<_type>::type _name;

#define INFO_STRUCT_END() \
	}; }
