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

#include <gtest/gtest.h>
#include <sstream>
#include <cstdio>

#include "insieme/frontend/frontend.h"
#include "insieme/frontend/tu/ir_translation_unit.h"

#include "insieme/core/ir_node.h"
#include "insieme/core/analysis/ir++_utils.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/backend/runtime/runtime_backend.h"

#include "insieme/utils/compiler/compiler.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/container_utils.h"
#include "insieme/utils/map_utils.h"
#include "insieme/utils/timer.h"

#include "insieme/driver/object_file_utils.h"
#include "insieme/utils/config.h"
#include "insieme/driver/integration/tests.h"

namespace insieme {

	using namespace driver::integration;

	// ---------------------------------- Check the runtime backend -------------------------------------

	// the type definition (specifying the parameter type)
	class MigrationIntegrationTest : public ::testing::TestWithParam<IntegrationTestCase> { };

	// define the test case pattern
	TEST_P(MigrationIntegrationTest, CompileableCode) {
		core::NodeManager manager;

		// obtain test case
		driver::integration::IntegrationTestCase testCase = GetParam();

		SCOPED_TRACE("Testing Case: " + testCase.getName());
		LOG(INFO) << "Testing Case: " + testCase.getName();

        std::string filename;
        {
            core::NodeManager tmpManager;
            //load program and create IR TU
            frontend::tu::IRTranslationUnit code = testCase.loadTU(tmpManager);
            char tmpname[] = "tmp.XXXXXX";
            mkstemp(tmpname);
            filename = tmpname;
            insieme::driver::saveLib(code, filename);
            EXPECT_TRUE(insieme::driver::isInsiemeLib(filename));
        }

		// load TU using the frontend (and all its potential extensions)
		insieme::frontend::ConversionJob job;
		job.addLib(insieme::driver::loadLib(manager, filename));
		core::ProgramPtr code = job.execute(manager);

		// create target code using the runtime backend
		auto target = backend::runtime::RuntimeBackend::getDefault()->convert(code);

		// see whether target code can be compiled
		utils::compiler::Compiler compiler = utils::compiler::Compiler::getRuntimeCompiler();

		std::string step="insiemecc_run_c_compile";
		// switch to C++ compiler if necessary
		if (any(testCase.getFiles(), [](const frontend::path& cur) { return *cur.string().rbegin() == 'p'; })) {
			compiler = utils::compiler::Compiler::getRuntimeCompiler(utils::compiler::Compiler::getDefaultCppCompiler());
			step="insiemecc_run_c++_compile";
		}

		// add extra compiler flags from test case
		for(const auto& flag : testCase.getCompilerArguments(step)) {
			compiler.addFlag(flag);
		}

		// add includes
		for(const auto& cur : testCase.getIncludeDirs()) {
			compiler.addIncludeDir(cur.string());
		}

		// add library directories
		for(const auto& cur : testCase.getLibDirs()) {
			compiler.addFlag("-L" + cur.string());
		}

		// add libraries
		for(const auto& cur : testCase.getLibNames()) {
			compiler.addFlag("-l" + cur);
		}

		EXPECT_TRUE(utils::compiler::compile(*target, compiler)) << "\nCode: " << *target;
		unlink(filename.c_str());
	}

	// instantiate the test case
	INSTANTIATE_TEST_CASE_P(MigrationIntegrationCheck, MigrationIntegrationTest, ::testing::ValuesIn(getAllCases()));

}
