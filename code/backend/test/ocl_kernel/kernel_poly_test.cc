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

#include <vector>
#include <iostream>
#include <memory>
#include <gtest/gtest.h>

#include "insieme/core/ir_program.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/ir_address.h"
#include "insieme/utils/logging.h"

#include "insieme/annotations/c/naming.h"
#include "insieme/annotations/ocl/ocl_annotations.h"

#include "insieme/frontend/program.h"
#include "insieme/frontend/clang_config.h"
#include "insieme/frontend/ocl/ocl_host_compiler.h"

#include "insieme/backend/ocl_kernel/kernel_poly.h"
#include "insieme/backend/ocl_kernel/kernel_config.h"

using namespace insieme::core;
using namespace insieme::core::lang;
using namespace insieme::annotations::c;
using namespace insieme::utils::set;

using namespace insieme::utils::set;
using namespace insieme::utils::log;

TEST(KernelPoly, RangeTest) {
	NodeManager manager;
	ProgramPtr program = Program::get(manager);

	// Frontend PATH
	CommandLineOptions::IncludePaths.push_back(std::string(SRC_DIR) + "inputs"); // this is for CL/cl.h in host.c
	CommandLineOptions::IncludePaths.push_back(std::string(SRC_DIR)); // this is for ocl_device.h in kernel.cl

	// Backend PATH
	CommandLineOptions::IncludePaths.push_back(std::string(OCL_KERNEL_TEST_DIR));
	CommandLineOptions::Defs.push_back("INSIEME");

	LOG(INFO) << "Converting input program '" << string(OCL_KERNEL_TEST_DIR) << "vec_add.c" << "' to IR...\n";
	insieme::frontend::Program prog(manager);

	prog.addTranslationUnit(std::string(OCL_KERNEL_TEST_DIR) + "vec_add.c");
	// 	prog.addTranslationUnit(std::string(SRC_DIR) + "/inputs/hello_host.c"); // Other Input test :)
	//prog.addTranslationUnit(std::string(OCL_KERNEL_TEST_DIR) + "kernel.cl");
	program = prog.convert();
	std::cout << "Done.\n";

	LOG(INFO) << "Starting OpenCL host code transformations";
	insieme::frontend::ocl::HostCompiler hc(program);
	hc.compile();

//	std::cout << "Printing the IR: " << pp;

	LOG(INFO) << "Start OpenCL Polyhedral analysis\n";

//	insieme::backend::ocl_kernel::KernelPoly polyAnalyzer(program);
//	insieme::core::printer::PrettyPrinter pp(polyAnalyzer.getKernels().at(0).getAddressedNode());
//	std::cout << pp;

//	EXPECT_EQ(1u, polyAnalyzer.getKernels().size());

}
