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

#include "insieme/transform/pattern/generator.h"

#include "insieme/utils/container_utils.h"

namespace insieme {
namespace transform {
namespace pattern {
namespace generator {

	namespace tree {

		namespace {


			TreePtr substitute(const TreePtr& target, const TreePtr& replacement, const TreePtr& var) {

				// test current node
				if (target == var) {
					return replacement;
				}

				// replace nodes recursively
				TreeList list = ::transform(target->getSubTrees(), [&](const TreePtr& tree) {
					return substitute(tree, replacement, var);
				});

				return makeTree(target->getId(), list);
			}

		}

		TreePtr Substitute::generate(const Match& match) const {

			// eval sub-terms
			TreePtr a = tree->generate(match);
			TreePtr b = replacement->generate(match);
			TreePtr c = var->generate(match);

			// apply substitution
			return substitute(a, b, c);
		}

	}


	const TreeGeneratorPtr root = std::make_shared<tree::Root>();


} // end namespace generator
} // end namespace pattern
} // end namespace transform
} // end namespace insieme

namespace std {

	std::ostream& operator<<(std::ostream& out, const insieme::transform::pattern::TreeGeneratorPtr& generator) {
		return (generator)?(generator->printTo(out)):(out << "null");
	}

	std::ostream& operator<<(std::ostream& out, const insieme::transform::pattern::ListGeneratorPtr& generator) {
		return (generator)?(generator->printTo(out)):(out << "null");
	}

} // end namespace std