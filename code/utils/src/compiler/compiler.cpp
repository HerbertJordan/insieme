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

#include "insieme/utils/compiler/compiler.h"
#include "insieme/utils/compiler/compiler_config.h"

#include <cstdio>
#include <sstream>
#include <fstream>

#include <boost/filesystem.hpp>

#include "insieme/utils/container_utils.h"


namespace insieme {
namespace utils {
namespace compiler {

	namespace fs = boost::filesystem;

	Compiler Compiler::getDefaultC99Compiler() {
		// create a default version of a C99 compiler
//		Compiler res(C_COMPILER); // TODO: re-enable when constant is set properly
		Compiler res("gcc");
		res.addFlag("--std=c99");
		res.addFlag("-x c");
		return res;
	}

	string Compiler::getCommand(const vector<string>& inputFiles, const string& outputFile) const {
		// build up compiler command
		std::stringstream cmd;

		cmd << executable;
		cmd << " " << join(" ", flags);
		cmd << " " << join(" ", inputFiles);
		cmd << " -o " << outputFile;

		return cmd.str();
	}



	bool compile(const vector<string>& sourcefile, const string& targetfile, const Compiler& compiler) {

		string&& cmd = compiler.getCommand(sourcefile, targetfile);

		LOG(DEBUG) << "Running command: " << cmd << "\n";
		int res = system(cmd.c_str());
		LOG(DEBUG) << "Result of command " << cmd << ": " << res << "\n";

		return res == 0;
	}


	bool compile(const Printable& source, const Compiler& compiler) {

		// create a temporary source file
		// TODO: replace with boost::filesystem::temp_directory_path() when version 1.46 is available
		char sourceFile[] = P_tmpdir "/srcXXXXXX";
		int src = mkstemp(sourceFile);
		assert(src != -1);
		close(src);

		LOG(DEBUG) << "Using temporary file " << string(sourceFile) << " as a source file for compilation.";

		// write source to file
		std::fstream srcFile(sourceFile, std::fstream::out);
		srcFile << source << "\n";
		srcFile.close();

		// create temporary target file name
		char targetFile[] = P_tmpdir "/trgXXXXXX";
		int trg = mkstemp(targetFile);
		assert(trg != -1);
		close(trg);

		LOG(DEBUG) << "Using temporary file " << string(targetFile) << " as a target file for compilation.";

		// conduct compilation
		bool res = compile(sourceFile, targetFile, compiler);

		// delete source and target files
		if (boost::filesystem::exists(sourceFile)) {
			boost::filesystem::remove(sourceFile);
		}
		if (boost::filesystem::exists(targetFile)) {
			boost::filesystem::remove(targetFile);
		}

		return res;
	}



} // end namespace compiler
} // end namespace utils
} // end namespace insieme
