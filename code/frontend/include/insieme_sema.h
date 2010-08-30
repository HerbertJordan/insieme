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

// defines which are needed by LLVM
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#include "lib/Sema/Sema.h"
#include "clang/Parse/Action.h"

using clang::SourceLocation;

namespace insieme {
namespace frontend {

class Pragma;
typedef std::shared_ptr<Pragma> PragmaPtr;
typedef std::vector<PragmaPtr> 	PragmaList;

class MatchMap;

// ------------------------------------ InsiemeSema ---------------------------

class InsiemeSema: public clang::Sema{
	class InsiemeSemaImpl;
	InsiemeSemaImpl* pimpl;

	bool isInsideFunctionDef;

	void matchStmt(clang::Stmt* S, const clang::SourceRange& bounds, const clang::SourceManager& sm, PragmaList& matched);

public:
	InsiemeSema (PragmaList& pragma_list, clang::Preprocessor& pp, clang::ASTContext& ctxt, clang::ASTConsumer& consumer, bool CompleteTranslationUnit = true,
				 clang::CodeCompleteConsumer* CompletionConsumer = 0);
	
	void addPragma(PragmaPtr P);

	OwningStmtResult ActOnCompoundStmt(SourceLocation L, SourceLocation R, MultiStmtArg Elts, bool isStmtExpr);
								   
	OwningStmtResult ActOnIfStmt(SourceLocation IfLoc, FullExprArg CondVal, DeclPtrTy CondVar, StmtArg ThenVal, SourceLocation ElseLoc, StmtArg ElseVal);

	OwningStmtResult ActOnForStmt(SourceLocation ForLoc, SourceLocation LParenLoc, StmtArg First, FullExprArg Second, DeclPtrTy SecondVar, FullExprArg Third,
						  		  SourceLocation RParenLoc, StmtArg Body);
								  
	DeclPtrTy ActOnStartOfFunctionDef(clang::Scope *FnBodyScope, clang::Declarator &D);
	
	DeclPtrTy ActOnStartOfFunctionDef(clang::Scope *FnBodyScope, DeclPtrTy D);

	DeclPtrTy ActOnFinishFunctionBody(DeclPtrTy Decl, StmtArg Body);
	
	DeclPtrTy ActOnDeclarator(clang::Scope *S, clang::Declarator &D);
								 
	template <class T>
	void ActOnPragma(const std::string& name, const MatchMap& mmap, SourceLocation startLoc, SourceLocation endLoc) {
		addPragma( PragmaPtr(new T(startLoc, endLoc, name, mmap)) );
	}
	
	void dump();
};

} // End frontend namespace
} // End insieme namespace

