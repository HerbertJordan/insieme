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
		 * class which helps finding the more suitable header for a declaration, not allways top
		 * level since we might have a system header included deep in a includes chain.
		 * the most apropiate header has to be computed
		 */
		class HeaderTagger { 
			
			vector<fs::path> stdLibDirs;
			vector<fs::path> userIncludeDirs;
			vector<fs::path> searchPath;
			const clang::SourceManager& sm;
		public:

			HeaderTagger(const vector<fs::path>& stdLibDirs, const vector<fs::path>& userIncludeDirs, const clang::SourceManager& srcMgr ):
				stdLibDirs( ::transform(stdLibDirs, [](const fs::path& cur) { return fs::canonical(cur); } ) ), 
				userIncludeDirs( ::transform(userIncludeDirs, [](const fs::path& cur) { return fs::canonical(cur); } ) ), 
				sm(srcMgr)
			{ 
				for (const fs::path& header : userIncludeDirs)
					searchPath.push_back(fs::canonical(header));
			
				for (const fs::path& header : stdLibDirs)
					searchPath.push_back(fs::canonical(header));

			}

			/**
			 * A utility function cutting down std-lib header files.
			 */
			boost::optional<fs::path> toStdLibHeader(const fs::path& path) const {
				static const boost::optional<fs::path> fail;

				if (stdLibDirs.empty()) { return fail; }

				if (contains(stdLibDirs, fs::canonical(path) )) {
					return fs::path();
				}

				if (!path.has_parent_path()) {
					return fail;
				}

				// if it is within the std-lib directory, build relative path
				auto res = toStdLibHeader(path.parent_path());
				return (res)? (*res/path.filename()) : fail;
			}

			bool isStdLibHeader(const clang::SourceLocation& loc) const{
				if (!loc.isValid()) return false;
				return isStdLibHeader ( sm.getPresumedLoc(loc).getFilename());
			}

			bool isStdLibHeader(const fs::path& path) const{
				return toStdLibHeader(fs::canonical(path));
			}
		
			bool isUserLibHeader(const clang::SourceLocation& loc) const{
				if (!loc.isValid()) return false;
				return isUserLibHeader ( sm.getPresumedLoc(loc).getFilename());
			}	

			bool isUserLibHeader(const fs::path& path) const{
				return toUserLibHeader(path);	
			}

			boost::optional<fs::path> toUserLibHeader(const fs::path& path) const {
				static const boost::optional<fs::path> fail;

				if (userIncludeDirs.empty()) { return fail; }

				if (contains(userIncludeDirs, fs::canonical(path) )) {
					return fs::path();
				}

				if (!path.has_parent_path()) {
					return fail;
				}

				// if it is within the user-added-include directory, build relative path
				auto res = toUserLibHeader(path.parent_path());
				return (res)? (*res/path.filename()) : fail;
			}


			bool isHeaderFile(const string& name) const {
				// everything ending wiht .h or .hpp or nothing (e.g. vector) => so check for not being c,cpp,...
				VLOG(2) << "isHeaderFile? " << name;
				return !name.empty() &&
						!(ba::ends_with(name, ".c") ||
						ba::ends_with(name, ".cc") ||
						ba::ends_with(name, ".cpp") ||
						ba::ends_with(name, ".cxx") ||
						ba::ends_with(name, ".C"));
			}

			string getTopLevelInclude(const clang::SourceLocation& loc) {

				// if it is a dead end
				if (!loc.isValid()) {
					std::cout << "loc is invalid" << std::endl;
					return "";
				}

				// get the presumed location (whatever this is, ask clang) ...
				// ~ ploc represents the file were loc is located
				clang::PresumedLoc ploc = sm.getPresumedLoc(loc);

				// .. and retrieve the associated include
				// ~ from where the file ploc represents was included
				clang::SourceLocation includeLoc = ploc.getIncludeLoc();
				
				// check whether the stack can be continued
				if (!includeLoc.isValid()) {
					//VLOG(2) << "no headerfile";
					return ""; 		// this happens when element is declared in c / cpp file => no header
				}
				
				// ~ pIncludeLoc represents the file were includLoc is located
				clang::PresumedLoc pIncludeLoc = sm.getPresumedLoc(includeLoc);




				//*******************
				//
				// travel down until we are at the sourcefile then work up again
				// if userProvided header, we need to go further
				// if userSearchPath/systemSearch path we need to include de header
				//
				//*******************
				//VLOG(2) << "presumedIncludeLoc: " << pIncludeLoc.getFilename();
				//VLOG(2) << "included file: " << ploc.getFilename();

				// descent further as long as we have a header file as presumed include loc
				if ( isHeaderFile(pIncludeLoc.getFilename()) ) {

					// check if last include was in the search path and next is not,
					// this case is a system header included inside of a programmer include chain
					// BUT if both are still in the search path, continue cleaning the include
					if((	(isStdLibHeader(ploc.getFilename()) && !isStdLibHeader(sm.getPresumedLoc(includeLoc).getFilename()))
						||	(isUserLibHeader(ploc.getFilename()) && !isUserLibHeader(sm.getPresumedLoc(includeLoc).getFilename())))
							&& !isIntrinsicHeader(sm.getPresumedLoc(includeLoc).getFilename())) {
						//VLOG(2) << "userProvidedHeader";
						return ploc.getFilename();
					}
					
					//VLOG(2) << "recCase " << pIncludeLoc.getFilename();
					return getTopLevelInclude(includeLoc);
				}

				// we already visited all the headers and we are in the .c/.cpp file
				if (isHeaderFile(ploc.getFilename())) {
					//VLOG(2) << "incFile: " << ploc.getFilename();
					if(isIntrinsicHeader(ploc.getFilename())) {
						//VLOG(2) << "intrinsic";
						return ploc.getFilename();
					} 
				
					if (isStdLibHeader(ploc.getFilename()) ) {
						//VLOG(2) << "systemInclude";
						return ploc.getFilename(); // this happens when header file is included straight in the code
					} 
					
					if (isUserLibHeader(ploc.getFilename())) {
						//VLOG(2) << "userInclude";
						return ploc.getFilename();
					} 
						
					//VLOG(2) << "not system header";
					return "";  // this happens when is declared in a header which is not system header
				} 

				/*
				// we already visited all the headers and we are in the .c/.cpp file
				if (!isHeaderFile(sm.getPresumedLoc(includeLoc).getFilename())) {
					return ploc.getFilename();
				}
				*/
				return "";
			}
			
			bool isIntrinsicHeader(const string& name) {
				return toIntrinsicHeader(fs::path(name));
			}

			boost::optional<fs::path> toIntrinsicHeader(const fs::path& path) {
				static const boost::optional<fs::path> fail;
				fs::path filename = path.filename();
				return (!filename.empty() && ba::ends_with(filename.string(), "intrin.h")) ? (filename) : fail;
				//return !name.empty() && ba::ends_with(name, "intrin.h");
				/*if (filename == "mmintrin.h" || filename == "xmmintrin.h" || filename == "emmintrin.h" ||
							filename == "pmmintrin.h" || filename == "tmmintrin.h" || filename == "smmintrin.h" ||
							filename == "nmmintrin.h" || filename == "immintrin.h" || filename == "ia32intrin.h" ||
							filename == "x86intrin.h") {
					return filename;
				}
				*/
			}

		};

	} // annonymous namespace
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


	bool isDefinedInSystemHeader (const clang::Decl* decl, const vector<fs::path>& stdLibDirs, const vector<fs::path>& userIncludeDirs){
		HeaderTagger tagger(stdLibDirs, userIncludeDirs, decl->getASTContext().getSourceManager());
		return tagger.isStdLibHeader(decl->getLocation());
	}

	void addHeaderForDecl(const core::NodePtr& node, const clang::Decl* decl, const vector<fs::path>& stdLibDirs, const vector<fs::path>& userIncludeDirs) {

		// check whether there is a declaration at all
		if (!decl) return;

		if (VLOG_IS_ON(2)){
			std::string name("UNNAMED");
			if (const clang::NamedDecl* nmd = llvm::dyn_cast<clang::NamedDecl>(decl))
				name = nmd->getQualifiedNameAsString();
			VLOG(2) << "Searching header for: " << node << " of type " << node->getNodeType() << " [clang: " << name << "]" ;
		}

		HeaderTagger tagger(stdLibDirs, userIncludeDirs, decl->getASTContext().getSourceManager());
		string fileName = tagger.getTopLevelInclude(decl->getLocation());

		// file must be a header file
		if (!tagger.isHeaderFile(fileName)) {
			VLOG(2) << "'" << fileName << "' not a headerfile";
			return;			// not to be attached
		}

		// do not add headers for external declarations unless those are within the std-library
		if (const clang::FunctionDecl* funDecl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
			// TODO: this is just based on integration tests - to make them work, no real foundation :(
			if( funDecl->isExternC() && 
				!(tagger.isStdLibHeader(fileName) || tagger.isIntrinsicHeader(fileName)) ) return;
		}

		// get absolute path of header file
		fs::path header = fs::canonical(fileName);

		// check if header is in STL
		if( auto stdLibHeader = tagger.toStdLibHeader(header) ) {
			header = *stdLibHeader;
		} else if( auto intrinsicHeader = tagger.toIntrinsicHeader(header) ) {
			header = *intrinsicHeader;
		} else if( auto userLibHeader = tagger.toUserLibHeader(header) ) {
			header = *userLibHeader;
		}

		VLOG(2) << "		header to be attached: " << header.string();

		// use resulting header
		insieme::annotations::c::attachInclude(node, header.string());
	}

} // end namespace utils
} // end namespace frontend
} // end namespace utils
