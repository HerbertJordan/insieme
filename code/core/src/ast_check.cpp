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

#include <algorithm>

#include "ast_check.h"
#include "container_utils.h"

namespace insieme {
namespace core {

bool Message::operator==(const Message& other) const {
	return type == other.type && address == other.address && errorCode == other.errorCode;
}

bool Message::operator<(const Message& other) const {
	if (type != other.type) {
		return type < other.type;
	}
	return address < other.address;
}

namespace {

	/**
	 * A check combining multiple AST Checks.
	 */
	class CombinedASTCheck : public ASTCheck {

		/**
		 * The list of checks to be represented.
		 */
		CheckList checks;

	public:

		/**
		 * Default constructor for this combined check resulting in a check
		 * combining the given checks.
		 *
		 * @param checks the checks to be conducted by the resulting check
		 */
		CombinedASTCheck(const CheckList& checks = CheckList()) : checks(checks) {};

	protected:

		MessageList visitNode(const NodeAddress& node) {
			// aggregate list of all error / warning messages
			MessageList list;
			for_each(checks.begin(), checks.end(), [&list, &node](const CheckPtr& cur) {
				addAll(list, cur->visit(node));
			});
			return list;
		}
	};


	/**
	 * A check conducting AST Checks recursively throughout an AST. Thereby, each
	 * node might be checked multiple times in case it is shared.
	 */
	class RecursiveASTCheck : public ASTCheck {

		/**
		 * The check to be conducted recursively.
		 */
		CheckPtr check;

	public:

		/**
		 * A default constructor for this recursive AST check implementation.
		 *
		 * @param check the check to be conducted recursively on all nodes.
		 */
		RecursiveASTCheck(const CheckPtr& check) : check(check) {};

	protected:

		MessageList visitNode(const NodeAddress& node) {

			// create resulting message list
			MessageList res;

			// create internally maintained visitor performing the actual check
			// NOTE: the visitor pointer is required to support recursion
			ASTVisitor<void, Address>* visitor;
			auto lambdaVisitor = makeLambdaVisitor<Address>([&res, &visitor, this](const NodeAddress& node) {

				// check the current node and collect results
				addAll(res, this->check->visit(node));

				// visit / check all child nodes
				int numChildren = node->getChildList().size();
				for (int i=0; i<numChildren; i++) {
					visitor->visit(node.getAddressOfChild(i));
				}
			});

			// update pointer ..
			visitor = &lambdaVisitor;

			// trigger the visit (only once)
			visitor->visit(node);

			// return collected messages
			return res;
		}
	};

	/**
	 * A check conducting AST Checks recursively throughout an AST. Thereby, each
	 * node is only visited once, even if it is shared multiple times.
	 */
	class VisitOnceASTCheck : public ASTCheck {

		/**
		 * The check to be conducted recursively.
		 */
		CheckPtr check;

	public:

		/**
		 * A default constructor for this AST check implementation.
		 *
		 * @param check the check to be conducted recursively on all nodes.
		 */
		VisitOnceASTCheck(const CheckPtr& check) : check(check) {};

	protected:

		MessageList visitNode(const NodeAddress& node) {

			// create resulting message list
			MessageList res;

			// the list of nodes already visited
			std::unordered_set<NodePtr, hash_target<NodePtr>, equal_target<NodePtr>> all;

			// create internally maintained visitor performing the actual check
			// NOTE: the visitor pointer is required to support recursion
			ASTVisitor<void, Address>* visitor;
			auto lambdaVisitor = makeLambdaVisitor<Address>([&res, &visitor, &all, this](const NodeAddress& node) {
				// add current node to set ...
				bool isNew = all.insert(node.getAddressedNode()).second;
				if (!isNew) {
					// ... and if already checked, we are done!
					return;
				}

				// check the current node and collect results
				addAll(res, this->check->visit(node));

				// visit / check all child nodes
				std::size_t numChildren = node->getChildList().size();
				for (std::size_t i=0; i<numChildren; i++) {
					visitor->visit(node.getAddressOfChild(i));
				}
			});

			// update pointer ..
			visitor = &lambdaVisitor;

			// trigger the visit (only once)
			visitor->visit(node);

			// return collected messages
			return res;
		}
	};

}

MessageList check(const NodePtr& node, const CheckPtr& check) {
	return check->visit(node);
}

CheckPtr makeRecursive(const CheckPtr& check) {
	return make_check<RecursiveASTCheck>(check);
}

CheckPtr makeVisitOnce(const CheckPtr& check) {
	return make_check<VisitOnceASTCheck>(check);
}

CheckPtr combine(const CheckPtr& a) {
	return combine(toVector<CheckPtr>(a));
}

CheckPtr combine(const CheckPtr& a, const CheckPtr& b) {
	return combine(toVector<CheckPtr>(a,b));
}

CheckPtr combine(const CheckPtr& a, const CheckPtr& b, const CheckPtr& c) {
	return combine(toVector<CheckPtr>(a,b,c));
}

CheckPtr combine(const CheckList& list) {
	return make_check<CombinedASTCheck>(list);
}


} // end namespace core
} // end namespace insieme

std::ostream& operator<<(std::ostream& out, const insieme::core::Message& message) {

	// start with type ...
	switch(message.getType()) {
	case insieme::core::Message::Type::WARNING:
		out << "WARNING: "; break;
	case insieme::core::Message::Type::ERROR:
		out << "ERROR:   "; break;
	}

	// add error code
	out << "[" << format("%05d", message.getErrorCode()) << "]";

	// .. continue with location ..
	out << "@ (";
	const insieme::core::NodeAddress& address = message.getAddress();
	if (address.isValid()) {
		out << address;
	} else {
		out << "<unknown>";
	}

	// .. and conclude with the message.
	out << ") - MSG: " << message.getMessage();
	return out;
}
