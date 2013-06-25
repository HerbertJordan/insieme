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

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include "insieme/frontend/utils/header_tagger.h"

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <clang/AST/Decl.h>
#include <clang/AST/ASTContext.h>
#pragma GCC diagnostic pop

#include "insieme/utils/logging.h"
#include "insieme/core/ir_node.h"
#include "insieme/annotations/c/include.h"

namespace insieme {
namespace frontend {
namespace utils {

	namespace fs = boost::filesystem;
	namespace ba = boost::algorithm;

	namespace {

		/**
		 * A utility function cutting down std-lib header files.
		 */
		boost::optional<fs::path> toStdLibHeader(const fs::path& path, const vector<fs::path>& libDirs) {
			static const boost::optional<fs::path> fail;

			if (libDirs.empty()) return fail;

			if (contains(libDirs, path)) {
				return fs::path();
			}

			if (!path.has_parent_path()) {
				return fail;
			}

			// if it is within the std-lib directory, build relative path
			auto res = toStdLibHeader(path.parent_path(), libDirs);
			return (res)? (*res/path.filename()) : fail;

		}

		bool isStdLibHeader(const fs::path& path, const vector<fs::path>& libDirs) {
			return toStdLibHeader(path, libDirs);
		}

		bool isHeaderFile(const string& name) {
			// everything ending wiht .h or .hpp or nothing (e.g. vector) => so check for not being c,cpp,...
			return !name.empty() &&
					!(ba::ends_with(name, ".c") ||
					ba::ends_with(name, ".cc") ||
					ba::ends_with(name, ".cpp") ||
					ba::ends_with(name, ".cxx") ||
					ba::ends_with(name, ".C"));
		}


		string getTopLevelInclude(const clang::SourceLocation& loc, const clang::SourceManager& sm) {

			// if it is a dead end
			if (!loc.isValid()) return "";

			// get the presumed location (whatever this is, ask clang) ...
			clang::PresumedLoc ploc = sm.getPresumedLoc(loc);

			// .. and retrieve the associated include
			clang::SourceLocation includeLoc = ploc.getIncludeLoc();

			// check whether the stack can be continued
			if (!includeLoc.isValid()) {
				return ""; 		// this happens when element is declared in c / cpp file => no header
			}

			// if the next file is no longer a header, the current file is the desired one
			if (!isHeaderFile(sm.getPresumedLoc(includeLoc).getFilename())) {
				return ploc.getFilename();
			}

			// decent further
			return getTopLevelInclude(includeLoc, sm);
		}

	}

	void addHeaderForDecl(const core::NodePtr& node, const clang::Decl* decl, const vector<fs::path>& stdLibDirs) {

		// check whether there is a declaration at all
		if (!decl) return;

		VLOG(2) << "Searching header for: " << node << " of type " << node->getNodeType() << "\n";

		clang::SourceManager& sm = decl->getASTContext().getSourceManager();
		string fileName = getTopLevelInclude(decl->getLocation(), sm);

		VLOG(2) << "Header file obtained: " << fileName << "\n";

		// file must be a header file
		if (!isHeaderFile(fileName)) {
			return;			// not to be attached
		}

		// do not add headers for external declarations unless those are within the std-library
		if (const clang::FunctionDecl* funDecl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
			// TODO: this is just based on integration tests - to make them work, no real foundation :(
			if (funDecl->isExternC() && !isStdLibHeader(fileName, stdLibDirs)) return;
		}

		// get absolute path of header file
		fs::path header = fs::canonical(fileName);

		// check whether it is within the STL library
		if (auto stdLibHeader = toStdLibHeader(header, stdLibDirs)) {
			header = *stdLibHeader;
		}

		// use resulting header
		insieme::annotations::c::attachInclude(node, header.string());
	}

} // end namespace utils
} // end namespace frontend
} // end namespace utils