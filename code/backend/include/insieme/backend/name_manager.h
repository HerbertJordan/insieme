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

#include <unordered_map>

#include "insieme/core/ast_node.h"

#include "insieme/utils/map_utils.h"

namespace insieme {
namespace backend {

	/**
	 * An abstract interface for a name manager implementation. The task of the name
	 * manager is to provide names for types, functions and variables within the generated
	 * C program, thereby avoiding collisions. The abstract interface is used by the backend.
	 * Concrete implementations are free to pick an arbitrary naming schema.
	 */
	class NameManager {

	public:

		/**
		 * A virtual destructor to support proper cleanups for sub-classes.
		 */
		virtual ~NameManager() {};

		/**
		 * Obtains a name for the construct represented by the given node pointer.
		 *
		 * @param ptr the construct to be named
		 * @param fragment a hint for the construction of the name - may be considered or not
		 * @return a name to be used within the generated C code
		 */
		virtual string getName(const core::NodePtr& ptr, const string& fragment = "") =0;

		/**
		 * Fixes the name of a construct within this name manager. Future getName(..) calls will
		 * return the given name for the given construct.
		 *
		 * @param ptr the construct to be named
		 * @param name the name to be assigned to the given construct
		 */
		virtual void setName(const core::NodePtr& ptr, const string& name) =0;

	};


	/**
	 * Generates unique names for anonymous IR nodes when required.
	 * Uses a simple counting system. Not thread safe, and won't necessarily generate the same name
	 * for the same node in different circumstances. Names will, however, stay the same for unchanged
	 * programs over multiple runs of the compiler.
	 */
	class SimpleNameManager : public NameManager {

		/**
		 * A counter used to generate individual names.
		 */
		unsigned long num;

		/**
		 * A prefix to be used for all kind of generated names.
		 */
		const string prefix;

		/**
		 * The internal cache used to maintain mapped names.
		 */
		utils::map::PointerMap<core::NodePtr, string> nameMap;

	public:

		/**
		 * The default constructor for a new name manager.
		 *
		 * @param prefix a string to be used in front of every name generated by this name manager
		 */
		SimpleNameManager(const string& prefix = "__insieme_") : num(0), prefix(prefix) { }

		/**
		 * Obtains a reference to the common prefix used by this name manager.
		 */
		virtual const string& getNamePrefix() const { return prefix; };

		/**
		 * Obtains a name for the construct represented by the given node pointer.
		 *
		 * @param ptr the construct to be named
		 * @param fragment a hint for the construction of the name - may be considered or not
		 * @return a name to be used within the generated C code
		 */
		virtual string getName(const core::NodePtr& ptr, const string& fragment = "");

		/**
		 * Fixes the name of a construct within this name manager. Future getName(..) calls will
		 * return the given name for the given construct.
		 *
		 * @param ptr the construct to be named
		 * @param name the name to be assigned to the given construct
		 */
		virtual void setName(const core::NodePtr& ptr, const string& name);

	};

} // end: namespace simple_backend
} // end: namespace insieme