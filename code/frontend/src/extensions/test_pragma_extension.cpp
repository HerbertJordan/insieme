/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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

#include "insieme/annotations/data_annotations.h"
#include "insieme/annotations/loop_annotations.h"
#include "insieme/annotations/transform.h"
#include "insieme/core/annotations/source_location.h"
#include "insieme/core/ir_expressions.h"
#include "insieme/frontend/convert.h"
#include "insieme/frontend/extensions/test_pragma_extension.h"
#include "insieme/frontend/extensions/frontend_extension.h"
#include "insieme/frontend/pragma/matcher.h"
#include "insieme/frontend/utils/source_locations.h"
#include "insieme/utils/numeric_cast.h"

namespace insieme {
namespace frontend {
namespace extensions {

using namespace insieme::frontend::pragma;

std::function<stmtutils::StmtWrapper(const MatchObject&, stmtutils::StmtWrapper)>
TestPragmaExtension::getMarkerAttachmentLambda() {
	return [this] (pragma::MatchObject object, stmtutils::StmtWrapper) {
		std::cout << "pragma executed" << std::endl;

		stmtutils::StmtWrapper res;
		const std::string want="expected";
		if (object.stringValueExists(want))
			expected = object.getString(want);
		return res;
	};
}

TestPragmaExtension::TestPragmaExtension(): Pragma(clang::SourceLocation(), clang::SourceLocation(), "", MatchMap()) {
	pragmaHandlers.push_back
			(std::make_shared<PragmaHandler>
			 (PragmaHandler("test", "expected", tok::eod, getMarkerAttachmentLambda())));
}

TestPragmaExtension::TestPragmaExtension(const clang::SourceLocation& s1, const clang::SourceLocation& s2, const string& str,
					   const insieme::frontend::pragma::MatchMap& mm): Pragma(s1, s2, str, mm) {
	pragmaHandlers.push_back
			(std::make_shared<PragmaHandler>
			 (PragmaHandler("test", "expected", tok::eod, getMarkerAttachmentLambda())));
}

} // extensions
} // frontend
} // insieme