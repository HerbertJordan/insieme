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

#include <sstream>

// defines which are needed by LLVM
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <clang/Lex/Preprocessor.h>

#include "insieme/frontend/compiler.h"

#include "insieme/frontend/utils/error_report.h"
#include "insieme/frontend/utils/source_locations.h"

using namespace clang;

namespace insieme {
namespace frontend {
namespace utils {

void compilerMessage(const DiagnosticLevel& 		level, 
					 const clang::SourceLocation& 	loc, 
					 const std::string& 			msg, 
					 const ClangCompiler& 			clangComp ) 
{
	std::ostringstream errMsg;
	errMsg << msg;

	SourceManager& manager = clangComp.getSourceManager();
    errMsg << " at location (" << frontend::utils::Line(loc, manager) << ":" <<
            frontend::utils::Column(loc, manager) << ")." << std::endl;

    clang::Preprocessor& pp = clangComp.getPreprocessor();
    pp.Diag(loc, pp.getDiagnostics().getCustomDiagID( level, errMsg.str() ));
}

} // end utils namespace 
} // end frontend namespace 
} // end insieme namespace 
