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

#include "cmd_line_utils.h"

#include <iostream>

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>

using namespace std;
namespace po = boost::program_options;

// current version number of the compiler: todo put into a file or something else
#define VERSION_NUMBER "2.0.1"

// initialize the static references of the CommandLineOptions args
#define FLAG(opt_name, opt_id, var_name, var_help) \
	bool CommandLineOptions::var_name = false;
#define OPTION(opt_name, opt_id, var_name, var_type, var_help) \
	var_type CommandLineOptions::var_name;
#include "options.inc"
#undef FLAG
#undef OPTION

namespace {
ostream& operator<<(ostream& out, const vector<string>& argList) {
	std::copy( argList.begin(), argList.end(), std::ostream_iterator<std::string>(cout, ", ") );
	return out;
}

void HandleImmediateArgs(po::options_description const& desc) {
	if( CommandLineOptions::Help ) {
		cout << desc << endl;
		// we exit from the compiler
		exit(1);
	} if( CommandLineOptions::Version ) {
		cout << "This is the Insieme (tm) compiler version: " << VERSION_NUMBER << endl <<
				"Realized by the Distributed and Parallel Systems (DPS) group, copyright 2008-2010, " <<
				"University of Innsbruck\n";
		exit(1);
	}
}
}

CommandLineOptions& CommandLineOptions::Parse(int argc, char** argv, bool debug) {
	po::options_description cmdLineOpts("Insieme (tm) compiler:\nOptions");

	po::positional_options_description posDesc;
	posDesc.add("input-file", -1);

	#define OPTION(opt_name, opt_id, var_name, var_type, var_help) \
		(opt_name, po::value< var_type >(), var_help)
	#define FLAG(opt_name, opt_id, var_name, var_help) \
		(opt_name, var_help)
	// Declare a group of options that will be allowed on the command line
	cmdLineOpts.add_options()
		#include "options.inc"
	;
	#undef OPTION
	#undef FLAG

	po::variables_map varsMap;

	try {
		// parse the command line and stores the options on the varsMap
		po::store(po::command_line_parser(argc, argv).options(cmdLineOpts).positional(posDesc).run(), varsMap);
		po::notify(varsMap);

		// when the debug flag is enabled, the list of command line arguments are written to the std console
		if( debug ) {
			cout << "(DEBUG) Command line arguments: " << endl;
			#define FLAG(opt_name, opt_id, var_name, var_help) \
		varsMap.count(opt_id) && cout << "\t--" << opt_id << endl;
			#define OPTION(opt_name, opt_id, var_name, var_type, var_help) \
				varsMap.count(opt_id) && cout << "\t--" << opt_id << ": " << varsMap[opt_id].as< var_type >() << endl;
			#include "options.inc"
			#undef OPTION
			#undef FLAG
		}

		// assign the value to the class fields
		#define FLAG(opt_name, opt_id, var_name, var_help) \
			(varsMap.count(opt_id)) ? CommandLineOptions::var_name = true : CommandLineOptions::var_name = false;
		#define OPTION(opt_name, opt_id, var_name, var_type, var_help) \
			if(varsMap.count(opt_id)) CommandLineOptions::var_name = varsMap[opt_id].as< var_type >();
		#include "options.inc"
		#undef OPTION
		#undef FLAG

		// Handle immediate flags (like --help or --version)
		HandleImmediateArgs(cmdLineOpts);

	} catch(boost::program_options::unknown_option& ex) {
		cout << "Usage error: " << ex.what() << endl;
		cout << cmdLineOpts << endl;
		exit(1);
	}

}
