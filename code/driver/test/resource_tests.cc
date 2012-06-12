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

#include <gtest/gtest.h>
#include <sstream>

#include "insieme/utils/timer.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/test/integration_tests.h"

#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_address.h"
#include "insieme/core/ir_statistic.h"

#include "insieme/driver/loader/integration_test_loader.h"

namespace insieme {

using namespace utils::test;
using namespace core;


TEST(SpeedTest, GetStatus) {
	Logger::setLevel(ERROR);
	core::NodeManager manager;

	// load test case
	auto root = driver::loader::loadIntegrationTest(manager, "nas/bt/w");
	ASSERT_TRUE(root);

	std::cout << IRStatistic::evaluate(root) << "\n";
	std::cout << NodeStatistic::evaluate(manager) << "\n";

	std::cout << "Node Size:                      " << sizeof(core::Node) << "\n";
	std::cout << "Node Type:                      " << sizeof(core::NodeType) << "\n";
	std::cout << "Node Category:                  " << sizeof(core::NodeCategory) << "\n";
	std::cout << "Node Value Size:                " << sizeof(core::NodeValue) << "\n";
	std::cout << "Node List Size:                 " << sizeof(core::NodeList) << "\n";
	std::cout << "NodeManager*:                   " << sizeof(core::NodeManager*) << "\n";
	std::cout << "EqualityID:                     " << sizeof(uint64_t) << "\n";
	std::cout << "Hash Code:                      " << sizeof(std::size_t) << "\n";
	std::cout << "Node Annotation Container Size: " << sizeof(core::Node::annotation_container) << "\n";

}

// define the test case pattern
TEST(SpeedTest, IRCopy) {
	Logger::setLevel(ERROR);
	core::NodeManager manager;

	// load test case
	auto root = driver::loader::loadIntegrationTest(manager, "nas/bt/w");
	ASSERT_TRUE(root);

	core::NodePtr root2;
	core::NodeManager manager2;

//	std::cout << NodeStatistic::evaluate(manager2) << "\n";

	double copyTime = TIME(root2 = manager2.get(root));

//	std::cout << NodeStatistic::evaluate(manager2) << "\n";


	std::cout << "Time to copy: " << copyTime << "\n";
	EXPECT_EQ(*root, *root2);
}

// define the test case pattern
TEST(SpeedTest, VisitAllPtr) {
	Logger::setLevel(ERROR);
	core::NodeManager manager;
	
	// load test case
	auto root = driver::loader::loadIntegrationTest(manager, "nas/bt/w");
	ASSERT_TRUE(root);

	// visit all pointer
	int counterPtr = 0;
	double timePtr = TIME(visitDepthFirst(root, [&](const NodePtr& cur) { counterPtr++; }, true, true));

	// visit all addresses
	int counterAdr = 0;
	double timeAdr = TIME(visitDepthFirst(NodeAddress(root), [&](const NodeAddress& cur) { counterAdr++; }, true, true));

	std::cout << "Count-Pointer: " << counterPtr << " in " << timePtr << " - avg: " << (timePtr / counterPtr)*1000000000 << "ns\n";
	std::cout << "Count-Address: " << counterAdr << " in " << timeAdr << " - avg: " << (timeAdr / counterAdr)*1000000000 << "ns\n";

	EXPECT_EQ(counterPtr, counterAdr);
}

// define the test case pattern
TEST(SpeedTest, VisitOncePtr) {
	Logger::setLevel(ERROR);
	core::NodeManager manager;

	// load test case
	auto root = driver::loader::loadIntegrationTest(manager, "nas/bt/w");
	ASSERT_TRUE(root);

	// visit all pointer
	int counterPtr = 0;
	double timePtr = TIME(visitDepthFirstOnce(root, [&](const NodePtr& cur) { counterPtr++; }, true, true));

	// visit all addresses
	int counterAdr = 0;
	double timeAdr = TIME(visitDepthFirstOnce(NodeAddress(root), [&](const NodeAddress& cur) { counterAdr++; }, true, true));

	std::cout << "Count-Pointer: " << counterPtr << " in " << timePtr << " - avg: " << (timePtr / counterPtr)*1000000000 << "ns\n";
	std::cout << "Count-Address: " << counterAdr << " in " << timeAdr << " - avg: " << (timeAdr / counterAdr)*1000000000 << "ns\n";

	EXPECT_EQ(counterPtr, counterAdr);
}


}