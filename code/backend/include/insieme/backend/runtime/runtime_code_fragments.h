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

#include "insieme/backend/c_ast/c_code.h"

#include "insieme/backend/converter.h"
#include "insieme/backend/runtime/runtime_extensions.h"
#include "insieme/backend/runtime/runtime_entities.h"

namespace insieme {
namespace backend {
namespace runtime {

	// ------------------------------------------------------------------------
	//  Within this header file a list of special code fragments used for
	//  creating code to be executed on the Insieme runtime is defined.
	// ------------------------------------------------------------------------

	class ContextHandlingFragment;
	typedef Ptr<ContextHandlingFragment> ContextHandlingFragmentPtr;

	class TypeTable;
	typedef Ptr<TypeTable> TypeTablePtr;

	class ImplementationTable;
	typedef Ptr<ImplementationTable> ImplementationTablePtr;

	/**
	 * This code fragment is containing code for context handling functions like
	 * insieme_init_context - invoked right after loading a context (shared library
	 * file) or the insieme_cleanup_context which is triggered just before
	 * the file is unloaded.
	 */
	class ContextHandlingFragment : public c_ast::CodeFragment {

		const Converter& converter;

	public:

		ContextHandlingFragment(const Converter& converter);

		static ContextHandlingFragmentPtr get(const Converter& converter);

		const c_ast::IdentifierPtr getInitFunctionName();

		const c_ast::IdentifierPtr getCleanupFunctionName();

		virtual std::ostream& printTo(std::ostream& out) const;

	};

	class TypeTableStore;

	/**
	 * The type table fragment contains code creating and handling the type table
	 * used by the Insieme runtime to obtain information regarding data item types.
	 */
	class TypeTable : public c_ast::CodeFragment {

		const Converter& converter;

		TypeTableStore* store;

	public:

		TypeTable(const Converter& converter);

		~TypeTable();

		static TypeTablePtr get(const Converter& converter);

		const c_ast::ExpressionPtr getTypeTable();

		virtual std::ostream& printTo(std::ostream& out) const;

		unsigned registerType(const core::TypePtr& type);

	};

	struct WorkItemImplCode;

	/**
	 * The implementation table fragment represents code resulting in the creation of the
	 * implementation table. This table consists of a list of work item implementations providing
	 * access points for work-item executions.
	 */
	class ImplementationTable : public c_ast::CodeFragment {

		const Converter& converter;

		utils::map::PointerMap<core::ExpressionPtr, unsigned> index;

		vector<WorkItemImplCode> workItems;

	public:

		ImplementationTable(const Converter& converter);

		static ImplementationTablePtr get(const Converter& converter);

		const c_ast::ExpressionPtr getImplementationTable();

		unsigned registerWorkItemImpl(const core::ExpressionPtr& implementation);

		virtual std::ostream& printTo(std::ostream& out) const;

	};

} // end namespace runtime
} // end namespace backend
} // end namespace insieme