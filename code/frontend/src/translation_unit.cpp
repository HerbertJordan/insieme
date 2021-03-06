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

#include "insieme/frontend/translation_unit.h"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <llvm/Support/FileSystem.h>
#include <clang/Serialization/ASTWriter.h>
#pragma GCC diagnostic pop

#include "insieme/frontend/pragma/handler.h"

#include "insieme/frontend/ocl/ocl_compiler.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclGroup.h"

#include "clang/Analysis/CFG.h"

#include "clang/Parse/Parser.h"
#include "clang/Parse/ParseAST.h"

#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaConsumer.h"
#include "clang/Sema/ExternalSemaSource.h"

#include "insieme/utils/set_utils.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/timer.h"
#include "insieme/utils/container_utils.h"


using namespace insieme;
using namespace insieme::core;
using namespace insieme::frontend;
using namespace insieme::frontend::pragma;
using namespace clang;

namespace {

// Instantiate the clang parser and sema to build the clang AST. Pragmas are stored during the parsing
///
void parseClangAST(ClangCompiler &comp, clang::ASTConsumer *Consumer, InsiemeSema& sema) {

	Parser P(comp.getPreprocessor(), sema, false);  // do not skip function bodies
	comp.getPreprocessor().EnterMainSourceFile();

	ParserProxy::init(&P);
	P.Initialize();	  //FIXME
	Consumer->Initialize(comp.getASTContext());
	if (SemaConsumer *SC = dyn_cast<SemaConsumer>(Consumer))
		SC->InitializeSema(sema);

	if (ExternalASTSource *External = comp.getASTContext().getExternalSource()) {
		if(ExternalSemaSource *ExternalSema = dyn_cast<ExternalSemaSource>(External))
			ExternalSema->InitializeSema(sema);
		External->StartTranslationUnit(Consumer);
	}

	Parser::DeclGroupPtrTy ADecl;
	while(!P.ParseTopLevelDecl(ADecl)) {
		if(ADecl) Consumer->HandleTopLevelDecl(ADecl.getPtrAs<DeclGroupRef>());
	}
	Consumer->HandleTranslationUnit(comp.getASTContext());
	ParserProxy::discard();  // FIXME

	// PRINT THE CFG from CLANG just for debugging purposes for the C++ frontend
	if(false) {
		clang::DeclContext* dc = comp.getASTContext().getTranslationUnitDecl();
		std::for_each(dc->decls_begin(), dc->decls_end(), [&] (const clang::Decl* d) {
			if (const clang::FunctionDecl* func_decl = llvm::dyn_cast<const clang::FunctionDecl> (d)) {
				if( func_decl->hasBody() ) {
					clang::CFG::BuildOptions bo;
					bo.AddInitializers = true;
					bo.AddImplicitDtors = true;
					clang::CFG* cfg = clang::CFG::buildCFG(func_decl, func_decl->getBody(), &comp.getASTContext(), bo);
					assert_true(cfg);
					std::cerr << "~~~ Function: "  << func_decl->getNameAsString() << " ~~~~~" << std::endl;
					cfg->dump(comp.getPreprocessor().getLangOpts(), true);
				}
			}
		});
	}
}

} // end anonymous namespace

namespace insieme {
namespace frontend {

TranslationUnit::TranslationUnit(NodeManager& mgr, const path& file,  const ConversionSetup& setup)
	: mMgr(mgr), mFileName(file), setup(setup), mClang(setup, file),  
		mSema(mPragmaList, mClang.getPreprocessor(), mClang.getASTContext(), emptyCons, true) 
	{

	// check for frontend extensions pragma handlers
	// and add user provided pragmas to be handled
	// by insieme
	std::map<std::string,clang::PragmaNamespace *> pragmaNamespaces;
	for(auto extension : setup.getExtensions()) {
		for(auto ph : extension->getPragmaHandlers()) {
			std::string name = ph->getName();
			// if the pragma namespace is not registered already
			// create and register it and store it in the map of pragma namespaces
			if(pragmaNamespaces.find(name) == pragmaNamespaces.end()) {
				pragmaNamespaces[name] = new clang::PragmaNamespace(name);
				mClang.getPreprocessor().AddPragmaHandler(pragmaNamespaces[name]);
			}
			// add the user provided pragma handler
			pragmaNamespaces[name]->AddPragma(pragma::PragmaHandlerFactory::CreatePragmaHandler<pragma::Pragma>(
														mClang.getPreprocessor().getIdentifierInfo(ph->getKeyword()),
														*ph->getToken(), ph->getName(), ph->getFunction()));

		}
	}

	parseClangAST(mClang, &emptyCons, mSema);

    // all pragmas should now have either a decl or a stmt attached.
    // it can be the case that a pragma is at the end of a file
    // and therefore not attached to anything. Find these cases
    // and attach the pragmas to the translation unit declaration.
    for(auto cur : mPragmaList) {
        if(!cur->isStatement() && !cur->isDecl()) {
            cur->setDecl(getCompiler().getASTContext().getTranslationUnitDecl());
        }
    }

	if(mClang.getDiagnostics().hasErrorOccurred()) {
		// errors are always fatal
		throw ClangParsingError(mFileName);
	}
}

TranslationUnit::PragmaIterator TranslationUnit::pragmas_begin() const {
	auto filtering = [](const Pragma&) -> bool { return true; };
	return TranslationUnit::PragmaIterator(getPragmaList(), filtering);
}

TranslationUnit::PragmaIterator TranslationUnit::pragmas_begin(const TranslationUnit::PragmaIterator::FilteringFunc& func) const {
	return TranslationUnit::PragmaIterator(getPragmaList(), func);
}

TranslationUnit::PragmaIterator TranslationUnit::pragmas_end() const {
	return TranslationUnit::PragmaIterator(getPragmaList().end());
}

bool TranslationUnit::PragmaIterator::operator!=(const PragmaIterator& iter) const {
	return (pragmaIt != iter.pragmaIt);
}

void TranslationUnit::PragmaIterator::inc() { 
	while(pragmaIt != pragmaItEnd) {
		++pragmaIt;

		if(pragmaIt != pragmaItEnd && filteringFunc(**pragmaIt)) {
			return;
		}
	} 
}

PragmaPtr TranslationUnit::PragmaIterator::operator*() const {
	assert(pragmaIt != pragmaItEnd);
	return *pragmaIt;
}

} // end frontend namespace
} // end insieme namespace

