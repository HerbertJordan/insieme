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

#include "insieme/core/program.h"

#include "insieme/frontend/compiler.h"

namespace clang {
namespace idx {
class TranslationUnit;
} // end idx namespace
} // end clang namespace

namespace insieme {
namespace frontend {

// ------------------------------------ TranslationUnit ---------------------------
/**
 * A translation unit contains informations about the compiler (needed to keep alive object instantiated by clang),
 * and the pragmas encountred during the processing of the translation unit.
 */
class TranslationUnit: public boost::noncopyable {
protected:
	std::string 	mFileName;
	ClangCompiler	mClang;
	PragmaList 		mPragmaList;
public:
	TranslationUnit(const std::string& fileName): mFileName(fileName), mClang(fileName) { }
	/**
	 * Returns a list of pragmas defined in the translation unit
	 */
	const PragmaList& 	 getPragmaList() const { return mPragmaList; }
	const ClangCompiler& getCompiler() const { return mClang; }
	const std::string& 	 getFileName() const { return mFileName; }
};

typedef TranslationUnit* TranslationUnitPtr;

// ------------------------------------ Program ---------------------------
/**
 * A program is made of a set of compilation units, we need to keep this object so we can create a complete call graph and thus
 * determine which part of the input program should be handled by insieme and which should be kept as the original.
 */
class Program: public boost::noncopyable {

	// Implements the pimpl pattern so we don't need to introduce an explicit dependency to Clang headers
	class ProgramImpl;
	typedef std::auto_ptr<ProgramImpl> ProgramImplPtr;
	ProgramImplPtr pimpl;

	// Reference to the NodeManager used to convert the translation units into IR code
	const insieme::core::SharedNodeManager& mMgr;

	// The IR program node containing the converted IR
	insieme::core::ProgramPtr mProgram;

	friend class ::TypeConversion_FileTest_Test;
	friend class ::StmtConversion_FileTest_Test;
public:
	typedef std::set<TranslationUnitPtr> TranslationUnitSet;

	Program(const insieme::core::SharedNodeManager& mgr);

	/**
	 * Add a single file to the program
	 */
	TranslationUnit& addTranslationUnit(const std::string& fileName);

	/**
	 * Add multiple files to the program
	 */
	void addTranslationUnits(const std::vector<std::string>& fileNames) {
		std::for_each(fileNames.begin(), fileNames.end(), [ this ](const std::string& fileName){ this->addTranslationUnit(fileName); });
	}

	// convert the program into the IR representation
	const insieme::core::ProgramPtr& convert();

	const insieme::core::ProgramPtr& getProgram() const { return mProgram; }

	/**
	 * Returns a list of parsed translation units
	 */
	const TranslationUnitSet& getTranslationUnits() const;

	static const TranslationUnit& getTranslationUnit(const clang::idx::TranslationUnit* tu);

	clang::idx::Program& getClangProgram();
	clang::idx::Indexer& getClangIndexer();

	void dumpCallGraph() const;
};

} // end frontend namespace
} // end insieme namespace
