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

#include "insieme/frontend/pragma/insieme.h"

namespace insieme {
namespace frontend {

using namespace insieme::frontend::pragma;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ TestPragma ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
TestPragma::TestPragma(const clang::SourceLocation& startLoc, 
					   const clang::SourceLocation& endLoc,
					   const std::string& 			type, 
					   const pragma::MatchMap& 		mmap) 

	: Pragma(startLoc, endLoc, type) 
{

	pragma::MatchMap::const_iterator fit = mmap.find("expected");
	if(fit != mmap.end()) {
		expected = *fit->second.front()->get<std::string*>();
	}
}

void TestPragma::registerPragmaHandler(clang::Preprocessor& pp) {

	pp.AddPragmaHandler(
		PragmaHandlerFactory::CreatePragmaHandler<TestPragma>(
			pp.getIdentifierInfo("test"), tok::string_literal["expected"] >> tok::eod 
		)
	);

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ InsiemePragma ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
InsiemePragma::InsiemePragma(const clang::SourceLocation& 	startLoc, 
							 const clang::SourceLocation& 	endLoc,
							 const std::string& 			type, 
							 const pragma::MatchMap&		mmap) 
	: Pragma(startLoc, endLoc, type) { }

void InsiemePragma::registerPragmaHandler(clang::Preprocessor& pp) {
	// define a PragmaNamespace for insieme
	clang::PragmaNamespace* insieme = new clang::PragmaNamespace("insieme");
	pp.AddPragmaHandler(insieme);

	// Add an handler for insieme mark pargma:
	// #pragma insieme mark new-line
	insieme->AddPragma(pragma::PragmaHandlerFactory::CreatePragmaHandler<InsiemeMark>(
			pp.getIdentifierInfo("mark"), tok::eod, "insieme")
		);

	// Add an handler for insieme ignore pragma:
	// #pragma insieme ignore new-line
	insieme->AddPragma(pragma::PragmaHandlerFactory::CreatePragmaHandler<InsiemeIgnore>(
			pp.getIdentifierInfo("ignore"), tok::eod, "insieme")
		);

    insieme->AddPragma(pragma::PragmaHandlerFactory::CreatePragmaHandler<InsiemeKernelFile>(
            pp.getIdentifierInfo("kernelFile"), tok::string_literal  >> tok::eod, "insieme")
        );

    insieme->AddPragma(pragma::PragmaHandlerFactory::CreatePragmaHandler<InsiemeKernelFile>(
            pp.getIdentifierInfo("datarange"), tok::expr["lb"] >> 
					 tok::colon >> tok::expr["ub"] >> tok::eod, "insieme")
        );
}

} // end frontend namespace
} // end insieme namespace
