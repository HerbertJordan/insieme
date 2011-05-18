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

#include "insieme/utils/test/test_utils.h"

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <boost/optional.hpp>

#include "insieme/utils/test/test_config.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/string_utils.h"

namespace insieme {
namespace utils {
namespace test {

	namespace fs = boost::filesystem;

	namespace {

		vector<IntegrationTestCase> loadAllCases() {
			// create a new result vector
			vector<IntegrationTestCase> res;

			// obtain access to the test directory
			const fs::path testDir(TEST_ROOT_DIR);

			// check whether the directory is correct
			if (!fs::exists(testDir)) {
				LOG(log::WARNING) << "Test-Directory path not properly set!";
				return res;
			}

			// read the test.cfg file
			const fs::path testConfig = testDir / "test.cfg";
			if (!fs::exists(testConfig)) {
				LOG(log::WARNING) << "Not test-configuration file found!";
				return res;
			}

			fs::ifstream configFile;
			configFile.open(testConfig);
			if (!configFile.is_open()) {
				LOG(log::WARNING) << "Unable to open test-configuration file!";
				return res;
			}
			vector<string> testCases;
			while (!configFile.eof()) {
				string testCase;
				configFile >> testCase;
				std::remove(testCase.begin(), testCase.end(), ' ');
				if (!testCase.empty()) {
					testCases.push_back(testCase);
				}
			}
			configFile.close();


			// load individual test cases
			for(auto it=testCases.begin(); it != testCases.end(); ++it) {
				const string& cur = *it;

				// check test case directory
				const fs::path testCaseDir = testDir / cur;
				if (!fs::exists(testCaseDir)) {
					LOG(log::WARNING) << "Directory for test case " + cur + " not found!";
					continue;
				}

				// read inputs.data (if present)
				vector<string> files;
				vector<string> includeDirs;
				auto inputFile = testCaseDir / "inputs.data";
				if (fs::exists(inputFile)) {
					// read file list from inputs.data
					fs::ifstream inputs;
					inputs.open(inputFile);
					if (!inputs.is_open()) {
						LOG(log::WARNING) << "Unable to open input file " + inputFile.string();
						continue;
					}

					// read entry by entry
					while (!inputs.eof()) {
						string file;
						inputs >> file;
						std::remove(file.begin(), file.end(), ' ');
						if (!file.empty()) {
							if (file[0] != '-') {
								// it's an input file
								files.push_back((testCaseDir / file).string());
							} else if (file.length() > 2){
								// it's an include directory
								includeDirs.push_back((testCaseDir / file.substr(2)).string());
							}
						}
					}
					inputs.close();

				} else {
					// use default file name
					files.push_back((testCaseDir / (cur + ".c")).string());
				}

				// add test case
				res.push_back(IntegrationTestCase(cur, files, includeDirs));
			}

			return res;
		}
	}

	// a global variable containing the list of test cases after they have been loaded the first time
	boost::optional<vector<IntegrationTestCase>> TEST_CASES = 0;

	const vector<IntegrationTestCase>& getAllCases() {
		// check whether cases have been loaded before
		if (!TEST_CASES) {
			TEST_CASES = boost::optional<vector<IntegrationTestCase>>(loadAllCases());
		}
		return *TEST_CASES;
	}


} // end namespace test
} // end namespace utils
} // end namespace insieme