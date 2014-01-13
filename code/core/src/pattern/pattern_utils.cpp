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

#include "insieme/core/pattern/pattern_utils.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/utils/container_utils.h"
#include "insieme/core/ir_visitor.h"

namespace insieme {
namespace core {
namespace pattern {
namespace irp {

	namespace {
		template<typename T>
		vector<T> collectAll(const TreePatternPtr& pattern, const T& root, bool matchTypes) {
			// just iterate through the tree and search for matches
			vector<T> res;
			core::visitDepthFirst(root, [&](const T& cur) {
				if (pattern->match(cur)) {
					res.push_back(cur);
				}
			}, true, matchTypes);

			return res;
		}
	}

	vector<core::NodePtr> collectAll(const TreePatternPtr& pattern, const core::NodePtr& root, bool matchTypes) {
		return collectAll<core::NodePtr>(pattern, root, matchTypes);
	}
	vector<core::NodeAddress> collectAll(const TreePatternPtr& pattern, const core::NodeAddress& root, bool matchTypes) {
		return collectAll<core::NodeAddress>(pattern, root, matchTypes);
	}

	
	void matchAll(const TreePatternPtr& pattern, const core::NodePtr& root, std::function<void(NodeMatch match)> lambda, bool matchTypes) {
		core::visitDepthFirst(root, [&](const NodePtr& cur) {
			MatchOpt mo = pattern->matchPointer(cur);
			if(mo) lambda(mo.get());
		}, true, matchTypes);
	}
	void matchAll(const TreePatternPtr& pattern, const core::NodeAddress& root, std::function<void(AddressMatch match)> lambda, bool matchTypes) {
		core::visitDepthFirst(root, [&](const NodeAddress& cur) {
			AddressMatchOpt mo = pattern->matchAddress(cur);
			if(mo) lambda(mo.get());
		}, true, matchTypes);
	}
	
	NodePtr replaceAll(const TreePatternPtr& pattern, const core::NodePtr& root, std::function<core::NodePtr(AddressMatch match)> lambda, bool matchTypes) {
		// visit in preorder and collect matches, then go through them in reverse (postorder)
		// postorder implies that we can easily handle non-overlapping results
		std::vector<NodeAddress> res = collectAll(pattern, NodeAddress(root), matchTypes);

		auto ret = root;
		while(!res.empty()) {
			auto next = res.back();
			res.pop_back();
			auto newAddr = next.switchRoot(ret);
			AddressMatchOpt mo = pattern->matchAddress(NodeAddress(newAddr.getAddressedNode()));
			if(mo) {
				ret = core::transform::replaceAddress(root->getNodeManager(), newAddr, lambda(mo.get())).getRootNode();
			}
		}
		return ret;
	}

} // end namespace irp
} // end namespace pattern
} // end namespace core
} // end namespace insieme

