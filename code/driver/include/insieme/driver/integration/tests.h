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

#include <string>
#include <map>
#include <vector>
#include <boost/optional.hpp>

#include "insieme/utils/string_utils.h"
#include "insieme/utils/printable.h"

#include "insieme/core/ir_program.h"
#include "insieme/frontend/frontend.h"

#include "insieme/driver/integration/properties.h"

namespace insieme {
namespace driver {
namespace integration {

	using std::map;
	using std::string;
	using std::vector;

	/**
	 * Instances of this class represent integration test cases within
	 */
	class IntegrationTestCase : public utils::Printable {

		/**
		 * The name of this test case.
		 */
		string name;

		/**
		 * The directory forming containing the test case.
		 */
		frontend::path dir;

		/**
		 * The input files of this test case.
		 */
		vector<frontend::path> files;

		/**
		 * Extra include directories to be considered.
		 */
		vector<frontend::path> includeDirs;

		/**
		 * A flag indicating whether OpenMP should be enabled within the frontend or not.
		 */
		bool enableOpenMP;

		/**
		 * A flag indicating whether OpenCL should be enabled within the frontend or not.
		 */
		bool enableOpenCL;

		/**
		 * A flag indicating whether OpenCL should be enabled within the frontend or not.
		 */
		bool enableCXX11;

		/**
		 * A list of macro definitions to be forwarded to the frontend.
		 */
		map<string, string> definitions;

		/**
		 * A list of arguments to be passed on to the compiler when building this test case.
		 */
		vector<string> compilerArguments;

		/**
		 * The properties configured for this test case.
		 */
		Properties properties;

	public:

		/**
		 * Creates a new test case based on the given arguments.
		 */
		IntegrationTestCase(const string& name,
							const frontend::path& dir,
							const vector<frontend::path>& files,
							const vector<frontend::path>& includeDirs, 
							bool enableOpenMP, bool enableOpenCL, bool enableCXX11, 
							const map<string,string>& definitions, const vector<string>& arguments,
							const Properties& properties)
			: name(name), dir(dir), files(files), includeDirs(includeDirs), enableOpenMP(enableOpenMP), enableOpenCL(enableOpenCL),
			  enableCXX11(enableCXX11), definitions(definitions), compilerArguments(arguments), properties(properties) {}

		/**
		 * Obtains the name of this test case.
		 */
		const string& getName() const {
			return name;
		}

		/**
		 * Obtains the directory forming this test case.
		 */
		const frontend::path& getDirectory() const {
			return dir;
		}

		/**
		 * Obtains the files this test case is consisting of.
		 */
		const vector<frontend::path>& getFiles() const {
			return files;
		}

		/**
		 * Obtains the list of include directories.
		 */
		const vector<frontend::path>& getIncludeDirs() const {
			return includeDirs;
		}

		/**
		 * Determines whether the OpenMP conversion should be enabled within the frontend or not.
		 */
		bool isEnableOpenMP() const {
			return enableOpenMP;
		}

		/**
		 * Determines whether the OpenCL conversion should be enabled within the frontend or not.
		 */
		bool isEnableOpenCL() const {
			return enableOpenCL;
		}

		/**
		 * Determines whether the standard to use is C++11.
		 */
		bool isCXX11() const {
			return enableCXX11;
		}

		/**
		 * Obtains the list of macro definitions to be passed on the the frontend.
		 */
		const map<string, string>& getDefinitions() const {
			return definitions;
		}

		/**
		 * Obtains a list of additional arguments to be passed on to the compiler when building the test case.
		 */
		const vector<string>& getCompilerArguments() const {
			return compilerArguments;
		}

		/**
		 * Get the properties configured for this test case.
		 */
		const Properties& getProperties() const {
			return properties;
		}

		/**
		 * Allows to print this test case to some stream.
		 */
		virtual std::ostream& printTo(std::ostream& out) const {
			return out << name;
		}

		/**
		 * An equality operator for integration test case instances.
		 */
		bool operator==(const IntegrationTestCase& other) const {
			return name == other.name;
		}

		/**
		 * A comparison operator for integration test case instances.
		 */
		bool operator<(const IntegrationTestCase& other) const {
			return name < other.name;
		}

		/**
		 * Loads this test case.
		 */
		core::ProgramPtr load(core::NodeManager& manager) const;

		/**
		 * Loads the translation unit for this test case.
		 */
		frontend::tu::IRTranslationUnit loadTU(core::NodeManager& manager) const;

	};

	// an optional type wrapping a test case
	typedef boost::optional<IntegrationTestCase> IntegrationTestCaseOpt;

	/**
	 * Obtains a full list of all test cases available within the system.
	 */
	const vector<IntegrationTestCase>& getAllCases();

	/**
	 * Obtains the test case matching the given name.
	 *
	 * @param name the name of the test case looking for
	 * @return an optional representing the test case or being uninitialized if there is no such test case.
	 */
	const IntegrationTestCaseOpt getCase(const string& name);

	/**
	 * Obtains the test case stored within the given directory.
	 *
	 * @param path the directory representing the integration test
	 * @return an optional representing the test case or being uninitialized if there is no such test case.
	 */
	const IntegrationTestCaseOpt getCaseAt(const string& path);

	/**
	 * This function is loading the integration test with the given name.
	 *
	 * @param manager the manager to be used to load the specified test program
	 * @param name the integration test to be loaded
	 * @param enableOpenMP a flag allowing to enable / disable the OpenMP conversion
	 * @param definitions the list of definitions to be passed to the pre-processor
	 * @return the loaded program
	 */
	core::ProgramPtr loadIntegrationTest(core::NodeManager& manager, const std::string& name,
			bool enableOpenMP = true, const std::map<string,string>& definitions = std::map<string,string>());

	/**
	 * Allow Integration Tests to be properly printed within gtest.
	 */
	inline std::ostream& operator<<(std::ostream& out, const IntegrationTestCase& test) {
		return test.printTo(out);
	}

} // end namespace integration
} // end namespace driver
} // end namespace insieme
