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

#include "insieme/core/ir.h"
#include "insieme/core/ir_address.h"

#include "insieme/utils/printable.h"

#include "insieme/analysis/cba/framework/call_site_manager.h"

namespace insieme {
namespace analysis {
namespace cba {

	// a simple function
	typedef core::ExpressionInstance ContextFreeCallable;

	// the type used to represent functions / closures
	template<typename Context>
	struct Callable : public utils::Printable, public utils::Hashable {
		Callee callee;
		Context context;

		Callable(const core::LiteralInstance& lit)
			: callee(lit), context() {}
		Callable(const core::LambdaExprInstance& fun)
			: callee(fun), context() {}
		Callable(const core::BindExprInstance& bind, const Context& context)
			: callee(bind), context(context) {}
		Callable(const core::ExpressionInstance& expr, const Context& context = Context())
			: callee(expr), context(context) {}
		Callable(const Callee& callee, const Context& context = Context())
			: callee(callee), context(context) {}
		Callable(const Callable& other)
			: callee(other.callee), context(other.context) {}

		bool operator<(const Callable& other) const {
			if (callee != other.callee) return callee < other.callee;
			if (context != other.context) return context < other.context;
			return false;
		}
		bool operator==(const Callable& other) const {
			if (this == &other) return true;
			return callee == other.callee && context == other.context;
		}

		bool operator!=(const Callable& other) const { return !(*this == other); }

		bool isLiteral() const { return callee.isLiteral(); };
		bool isLambda() const { return callee.isLambda(); };
		bool isBind() const { return callee.isBind(); }

		std::size_t getNumParams() const { return callee.getNumParams(); }

		const core::NodeInstance& getDefinition() const {
			return callee.getDefinition();
		}

		core::StatementInstance getBody() const {
			return callee.getBody();
		}

		const Context& getContext() const {
			return context;
		}

		std::ostream& printTo(std::ostream& out) const {
			if (callee.isLiteral()) return out << callee;
			return out << "(" << callee << "," << context << ")";
		}

		size_t hash() const {
			return utils::combineHashes(callee, context);
		}
	};

} // end namespace cba
} // end namespace analysis
} // end namespace insieme
