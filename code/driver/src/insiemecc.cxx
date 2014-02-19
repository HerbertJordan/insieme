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

/**
 * Within this file a small, simple example of a compiler driver utilizing
 * the insieme compiler infrastructure is presented.
 *
 * This file is intended to provides a template for implementing new compiler
 * applications utilizing the Insieme compiler and runtime infrastructure.
 */

#include <string>

#include <boost/algorithm/string.hpp>

#include "insieme/utils/logging.h"
#include "insieme/utils/compiler/compiler.h"

#include "insieme/frontend/frontend.h"

#include "insieme/backend/runtime/runtime_backend.h"

#include "insieme/driver/cmd/options.h"
#include "insieme/driver/object_file_utils.h"
#include "insieme/core/checks/full_check.h"
#include "insieme/core/printer/pretty_printer.h"


using namespace std;

namespace fs = boost::filesystem;

namespace fe = insieme::frontend;
namespace co = insieme::core;
namespace be = insieme::backend;
namespace dr = insieme::driver;
namespace cp = insieme::utils::compiler;
namespace cmd = insieme::driver::cmd;



int main(int argc, char** argv) {
	// filter logging messages
	Logger::setLevel(ERROR);

//	// for debugging
//	std::cout << "Arguments: \n";
//	for(int i=0; i<argc; i++) {
//		std::cout << "\t" << argv[i] << "\n";
//	}
//	std::cout << "\n";

	// Step 1: parse input parameters
	//		This part is application specific and need to be customized. Within this
	//		example a few standard options are considered.
	bool compileOnly;
	bool runIRChecks;
    bool optimize;
	string tuCodeFile;		// a file to dump the TU code
	string irCodeFile;		// a file to dump IR code to
	string trgCodeFile;		// a file to dump the generated target code
	bool keepOutputCode;		
 	cmd::Options options = cmd::Options::parse(argc, argv)
		// one extra parameter to limit the compiler to creating an .o file
		("compile", 	'c', 	compileOnly, 	"compilation only")
        ("strict-semantic", 'S', runIRChecks,   "semantic checks")
        ("full-optimization", 'O',      optimize,   "full optimization")
		("tu-code", 	tuCodeFile, 	string(""), "dump translation unit code")
		("ir-code", 	irCodeFile, 	string(""), "dump IR code")
		("trg-code", 	trgCodeFile, 	string(""), "dump target code")
		("keep-code",  keepOutputCode,	false, "do not delete output code")
	;

	//indicates that a shared object files should be created
	bool createSharedObject = options.outFile.find(".so")!=std::string::npos;

    //enable semantic check plugin if needed
    options.job.setOption(fe::ConversionSetup::StrictSemanticChecks, runIRChecks);

	// disable cilk, omp, ocl support for insiemecc
	options.job.setOption(fe::ConversionJob::Cilk, false);
    // turn off openmp frontend if -fopenmp is not provided
    if (options.job.getFFlags().find("-fopenmp") == options.job.getFFlags().end())
    	options.job.setOption(fe::ConversionJob::OpenMP, false);
	options.job.setOption(fe::ConversionJob::OpenCL, false);

	if (!options.valid) return (options.help)?0:1;

	// Step 2: filter input files
	vector<fe::path> inputs;
	vector<fe::path> libs;
	vector<fe::path> extLibs;

	for(const fe::path& cur : options.job.getFiles()) {
		auto ext = fs::extension(cur);
		if (ext == ".o" || ext == ".so") {
			if (dr::isInsiemeLib(cur)) {
				libs.push_back(cur);
			} else {
				extLibs.push_back(cur);
			}
		} else {
			inputs.push_back(cur);
		}
	}

//std::cout << "Libs:    " << libs << "\n";
//std::cout << "Inputs:  " << inputs << "\n";
//std::cout << "ExtLibs: " << extLibs << "\n";
//std::cout << "OutFile: " << options.outFile << "\n";
//std::cout << "Compile Only: " << compileOnly << "\n";
//std::cout << "SharedObject: " << createSharedObject << "\n";
//std::cout << "WorkingDir: " << boost::filesystem::current_path() << "\n";

	// update input files
	options.job.setFiles(inputs);

    // Step 3: load input code
	co::NodeManager mgr;

	// load libraries
	options.job.setLibs(::transform(libs, [&](const fe::path& cur) {
		std::cout << "Loading " << cur << " ...\n";
		return dr::loadLib(mgr, cur);
	}));

	// if it is compile only or if it should become an object file => save it
	if (compileOnly || createSharedObject) {
		auto res = options.job.toIRTranslationUnit(mgr);
		dr::saveLib(res, options.outFile);
		return dr::isInsiemeLib(options.outFile) ? 0 : 1;
	}

	// dump the translation unit file (if needed for de-bugging)
	if (tuCodeFile != "") {
		auto tu = options.job.toIRTranslationUnit(mgr);
		std::ofstream out(tuCodeFile);
		out << tu;
	}

	// convert src file to target code
    auto program = options.job.execute(mgr);

	// dump IR code
	if (irCodeFile != "") {
		std::ofstream out(irCodeFile);
		out << co::printer::PrettyPrinter(program);
	}

	if (runIRChecks) {
		std::cout << "Running semantic checks ...\n";
		std::cout << "Errors:\n" << co::checks::check(program);
	}

	// Step 3: produce output code
	//		This part converts the processed code into C-99 target code using the
	//		backend producing parallel code to be executed using the Insieme runtime
	//		system. Backends targeting alternative platforms may be present in the
	//		backend modul as well.
	cout << "Creating target code ...\n";
	auto targetCode = be::runtime::RuntimeBackend::getDefault()->convert(program);

	// dump source file if requested
	if (trgCodeFile != "") {
		std::ofstream out(trgCodeFile);
		out << *targetCode;
	}


	// Step 4: build output code
	//		A final, optional step is using a third-party C compiler to build an actual
	//		executable.
	//		if any of the translation units is has cpp belongs to cpp code, we'll use the
	//		cpp compiler, C otherwise
	cout << "Building binaries ...\n";

	cp::Compiler compiler =
			(options.job.isCxx())
				? cp::Compiler::getDefaultCppCompilerO3()
				: cp::Compiler::getDefaultC99CompilerO3();

	compiler = cp::Compiler::getRuntimeCompiler(compiler);
	
    //add needed library flags
    for(auto cur : extLibs) {
		string libname = cur.filename().string();
		// add libraries by splitting their paths, truncating the filename of the library in the process (lib*.so*)
		compiler.addExternalLibrary(cur.parent_path().string(), libname.substr(3,libname.find(".")-3));
	}

    //add the given optimization flags (-f flags)
    for(auto cur : options.job.getFFlags()) {
        compiler.addFlag(cur);
    }
    
    //FIXME: Add support for -O1, -O2, ... 
    //if an optimization flag is set (e.g. -O3) 
    //set this flag in the backend compiler
    if (optimize)
        compiler.addFlag("-O3");

    //check if the c++11 standard was set when calling insieme
    //if yes, use the same standard in the backend compiler
    if (options.job.getStandard() == fe::ConversionSetup::Standard::Cxx11) {
        compiler.addFlag("-std=c++0x");
    }
    
	bool success = cp::compileToBinary(*targetCode, options.outFile, compiler, keepOutputCode);

	// done
	return (success)?0:1;
}
