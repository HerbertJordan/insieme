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

#pragma once

#include <string>
#include <cstdlib>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
//#include <boost/filesystem/path.hpp>
#include <boost/tokenizer.hpp>

namespace insieme {
namespace iwir {
namespace cmd {

	using namespace std;

	namespace detail {
		class OptionParser;
	}

	/**
	 * A standard command line option utility which can be extended and customized for
	 * individual needs.
	 */
	struct Options {

		/**
		 * A flag indicating whether the passed options are valid. It will not
		 * if a required option is missing or the help flag -h was used.
		 */
		bool valid;

		/**
		 * A flag indicating whether the help option -h was provided. In this case
		 * the options would also be not valid.
		 */
		bool help;

		/**
		 * The input file - IWIR XML
		 */
		string inFile;


		/**
		 * The output file - the final INSPIRE
		 */
		string outFile;

		/**
		 * Parses the given command line options.
		 */
		static detail::OptionParser parse(int argc, char** argv);

	private:

		// allow the parser to construct instances
		friend class detail::OptionParser;

		/**
		 * A constructor for this class.
		 */
		Options() : valid(true), help(false) {}

	};

	namespace detail {

		class OptionParser {

			/**
			 * A function to be processed when parsing
			 */
			typedef std::function<bool(const boost::program_options::variables_map&)> parser_step;

			/**
			 * The number of arguments passed to the program.
			 */
			int argc;

			/**
			 * The arguments passed to the program.
			 */
			char** argv;

			/**
			 * The description of parameters to be parsed by this parser - aggregated
			 * on the fly.
			 */
			boost::program_options::options_description desc;

			/**
			 * A list of extra parser steps to be conducted when parsing program options.
			 */
			vector<parser_step> parser_steps;

		public:

			/**
			 * Creates a new instance of this option parser parsing the given arguments.
			 * This
			 */
			OptionParser(int argc, char** argv);

			/**
			 * Allows to add a flag to the program options using a convenient syntax.
			 *
			 * @param name the name of the flag to be added
			 * @param symbol the one-letter shortcut of the flag
			 * @param target the target to be used for storing whether the flag has been set or not
			 * @param description the description of the parameter to be shown in the help message
			 */
			OptionParser& operator()(const string& name, char symbol, bool& flag, const char* description);

			/**
			 * Allows to add additional program options using a convenient syntax.
			 *
			 * @param name the name of the option to be added
			 * @param target the target to store the parsed result into
			 * @param description the description of the parameter to be shown in the help message
			 */
			template<typename T>
			OptionParser& operator()(const char* name, T& target, const char* description) {
				desc.add_options()(name, boost::program_options::value<T>(&target), description);
				return *this;
			}

			/**
			 * Allows to add additional program options using a convenient syntax.
			 *
			 * @param name the name of the option to be added
			 * @param target the target to store the parsed result into
			 * @param def the default value to be used if non is provided
			 * @param description the description of the parameter to be shown in the help message
			 */
			template<typename T>
			OptionParser& operator()(const char* name, T& target, const T& def, const char* description) {
				desc.add_options()(name, boost::program_options::value<T>(&target)->default_value(def), description);
				return *this;
			}

			/**
			 * Realizes the final step by conducting the actual parsing and returning the
			 * parsed program options.
			 */
			operator Options();

		};

	}

//	class Option {
//		bool isFlag;
//		bool required;
//		string name;
//
//	};


} // end namespace cmd_options
} // end namespace driver
} // end namespace insieme
