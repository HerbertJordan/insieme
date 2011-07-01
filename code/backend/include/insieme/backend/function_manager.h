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

#include "insieme/core/forward_decls.h"

#include "insieme/backend/c_ast/c_code.h"

namespace insieme {
namespace backend {

	class Converter;
	class FunctionInfo;
	class LambdaInfo;
	class BindInfo;

	namespace detail {
		class FunctionInfoStore;
	}

	class FunctionManager {

		const Converter& converter;

		detail::FunctionInfoStore* store;

	public:

		FunctionManager(const Converter& converter);

		~FunctionManager();

		const FunctionInfo& getInfo(const core::LiteralPtr& literal);

		const LambdaInfo& getInfo(const core::LambdaExprPtr& lambda);

		const BindInfo& getInfo(const core::BindExprPtr& bind);

//		const c_ast::NodePtr getCallable(const core::LiteralPtr& literal);
//
//		const c_ast::NodePtr getCallable(const core::LambdaExprPtr& lambda);
//
//		const c_ast::NodePtr getCallable(const core::BindExprPtr& bind);

		const c_ast::NodePtr getValue(const core::LiteralPtr& literal);

		const c_ast::NodePtr getValue(const core::LambdaExprPtr& literal);

		const c_ast::NodePtr getValue(const core::BindExprPtr& literal);

	};


	struct ElementInfo {

		virtual ~ElementInfo() {}
	};

	struct FunctionInfo : public ElementInfo {

		// to be included:
		// 		- the C-AST function represented
		//		- prototype
		//		- definition
		//		- lambdaWrapper

		c_ast::FunctionPtr function;

		c_ast::CodeFragmentPtr prototype;

		c_ast::CodeFragmentPtr lambdaWrapper;


	};

	struct LambdaInfo : public FunctionInfo {

		// to be included
		// 		- the definition of the function

		c_ast::CodeFragmentPtr definition;

	};

	struct BindInfo : public ElementInfo {

		// to be included
		//		- the closure type definition
		//		- the mapper function definition
		//		- the constructor

		c_ast::IdentifierPtr closureName;

		c_ast::TypePtr closureType;

		c_ast::CodeFragmentPtr definitions;

		c_ast::IdentifierPtr mapperName;

		c_ast::IdentifierPtr constructorName;
	};

} // end namespace backend
} // end namespace insieme