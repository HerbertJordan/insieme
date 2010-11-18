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
namespace core {

// Forward declarations
class NodeManager;
template<class T> class AnnotatedPtr;
class Type;
typedef AnnotatedPtr<const Type> TypePtr;
class Literal;
typedef AnnotatedPtr<const Literal> LiteralPtr;

class Node;
typedef AnnotatedPtr<const Node> NodePtr;
class Statement;
typedef AnnotatedPtr<const Statement> StatementPtr;


namespace lang {

class BasicGenerator {
	mutable NodeManager& nm;
	struct BasicGeneratorImpl;
	mutable BasicGeneratorImpl* pimpl;

public:
	BasicGenerator(NodeManager& nm);
	~BasicGenerator();

	#define TYPE(_id, _spec) \
	TypePtr get##_id() const; \
	bool is##_id(const NodePtr& p) const;

	#define LITERAL(_id, _name, _spec) \
	LiteralPtr get##_id() const; \
	bool is##_id(const NodePtr& p) const;

	#include "insieme/core/lang/lang.def"

	#undef TYPE
	#undef LITERAL

	// ----- extra material ---

	StatementPtr getNoOp() const;
	bool isNoOp(const NodePtr& p) const;

};

} // namespace lang
} // namespace core
} // namespace insieme
