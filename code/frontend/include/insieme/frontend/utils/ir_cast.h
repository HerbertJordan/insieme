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

namespace insieme {

// forward declarations
namespace core { 
template <class T>
class Pointer;

class Expression;
typedef Pointer<const Expression> ExpressionPtr;

class Type;
typedef Pointer<const Type> TypePtr;

class IRBuilder;
} // end core namespace 

namespace frontend {
namespace utils {

	/**
	 * This function tries to restructure the given expression of a reference to a scalar
	 * into a reference to an array - if possible without using the scalar.to.ref.array literal.
	 *
	 * @param expr the expression to be converted
	 * @return the rewritten, equivalent expression exposing a reference to an array
	 */
	core::ExpressionPtr refScalarToRefArray(const core::ExpressionPtr& expr);

	core::ExpressionPtr cast(const core::ExpressionPtr& expr, const core::TypePtr& trgTy);


	core::TypePtr getArrayElement(const core::TypePtr& type);

	bool isRefRef(const core::TypePtr& type);

	core::TypePtr getVectorElement(const core::TypePtr& type);

	/**
	 * checks if the expression corresponds to a null pointer expression 
	 * (usually those are wrapped on a cast)
	 */
	bool isNullPtrExpression(const core::ExpressionPtr& expr);

} // end utils namespace 
} // end frontend namespace
} // end insisme namespace
