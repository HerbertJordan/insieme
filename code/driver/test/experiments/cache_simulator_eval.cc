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

#include "insieme/frontend/frontend.h"
#include "insieme/analysis/features/cache_features.h"
#include "insieme/analysis/dep_graph.h"

#include "insieme/transform/primitives.h"
#include "insieme/transform/pattern/ir_pattern.h"
#include "insieme/transform/polyhedral/transformations.h"
#include "insieme/transform/filter/filter.h"

#include "insieme/utils/test/integration_tests.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/timer.h"

#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/transform/node_replacer.h"

#include "insieme/driver/measure/measure.h"

namespace insieme {

using namespace analysis::features;
using namespace utils::test;
using namespace transform;
using namespace transform::pattern;
using namespace transform::polyhedral;
using namespace core;
using namespace core::printer;
using namespace driver::measure;

#include "../include/integration_tests.inc"


//	TEST(CacheOptimizerTest, MatrixMultiplication) {
//		Logger::setLevel(ERROR);
//
//		NodeManager manager;
//
//		// load test case
//		ProgramPtr prog = load(manager, "matrix_mul_static");
//		ASSERT_TRUE(prog);
//
////		std::cout << "Pattern 1: \n" << transform::pattern::irp::innerMostForLoopNest(1) << "\n\n";
////		std::cout << "Pattern 2: \n" << transform::pattern::irp::innerMostForLoopNest(2) << "\n\n";
////		std::cout << "Pattern 3: \n" << transform::pattern::irp::innerMostForLoopNest(3) << "\n\n";
//
//		// find second-innermost loops to be tiled
//		auto targetFilter = filter::allMatches(insieme::transform::pattern::irp::innerMostForLoopNest(3));
//
//		vector<NodeAddress> targets = targetFilter(prog);
//		EXPECT_EQ(1u, targets.size());
//
//		NodeAddress target = targets[0];
//
//		// make sure loop tiling is working
//		ASSERT_NO_THROW(makeLoopTiling(4,4,4)->apply(target));
//
//		// build model for local L3 (i7-620)
//		EarlyTermination<LRUCacheModel<64,8192,16>> L3;
//
//		// check all kind of cubic tile sizes
//		std::cout << "TS, misses, time\n";
//		for(int i=8; i<=1000; i+=8) {
//			auto version = makeLoopTiling(i,i,i)->apply(target);
//			utils::Timer timer("");
//			L3.eval(version);
//			timer.stop();
//			//std::cout << "TileSize: " << format("%4d", i) << " - misses: " <<  L3.getFeatureValue() << " - " << L3.getMissRatio() << " - " << timer.getTime() << "sec\n";
//			std::cout << format("%4d,%.6f, %.2f\n", i, L3.getMissRatio(), timer.getTime());
//		}
//
//		// check 2 fixed and one variable parameter
//		std::cout << "TS, misses, time\n";
//		for(int i=8; i<=1000; i+=32) {
//			auto version = makeLoopTiling(i,10,120)->apply(target);
//			utils::Timer timer("");
//			L3.eval(version);
//			timer.stop();
//			//std::cout << "TileSize: " << format("%4d", i) << " - misses: " <<  L3.getFeatureValue() << " - " << L3.getMissRatio() << " - " << timer.getTime() << "sec\n";
//			std::cout << format("%4d,%.6f, %.2f\n", i, L3.getMissRatio(), timer.getTime());
//		}
//
//
//		EarlyTermination<LRUCacheModel<64,8192,16>> fast;
//		LRUCacheModel<64,8192,16> full;
//
//		// compare with and without early-termination
//		std::cout << "TS, missesFast, timeFast, missesFull, timeFull\n";
//		for(int i=8; i<=1000; i+=64) {
//			auto version = makeLoopTiling(i,i,i)->apply(target);
//
//			double fastTime = TIME(fast.eval(version));
//			double fullTime = TIME(full.eval(version));
//
//			//std::cout << "TileSize: " << format("%4d", i) << " - misses: " <<  L3.getFeatureValue() << " - " << L3.getMissRatio() << " - " << timer.getTime() << "sec\n";
//			std::cout << format("%4d,%.6f,%.2f,%.6f,%.2f\n", i, fast.getMissRatio(), fastTime, full.getMissRatio(), fullTime);
//
//		}
//
//	}
//
//
//	TEST(CacheOptimizerTest, MM_PapiCounter) {
//		Logger::setLevel(ERROR);
//
//		NodeManager manager;
//
//		// load test case
//		ProgramPtr prog = load(manager, "matrix_mul_static");
//		ASSERT_TRUE(prog);
//
//		// find matrix-multiplication loop
//		auto targetFilter = filter::allMatches(insieme::transform::pattern::irp::innerMostForLoopNest(3));
//
//		vector<NodeAddress> targets = targetFilter(prog);
//		EXPECT_EQ(1u, targets.size());
//
//		StatementAddress target = targets[0].as<StatementAddress>();
//
//
//		auto trans = makeLoopTiling(64,64,64);
//
//		// collect execution time and misses
//		auto metrics = toVector(Metric::TOTAL_EXEC_TIME_NS, Metric::TOTAL_L3_CACHE_MISS);
//		auto ms = milli * s;
//
//		std::cout << "Running measurement ...\n";
//		auto res = measure(target, metrics, 1);
//		std::cout << "Time, Misses\n";
//		for_each(res, [&](std::map<MetricPtr, Quantity>& cur) {
//			std::cout << cur[Metric::TOTAL_EXEC_TIME_NS].to(ms) << "," << cur[Metric::TOTAL_L3_CACHE_MISS] << "\n";
//		});
//
//		std::cout << "Running measurement ...\n";
//		StatementAddress stmt = core::transform::replaceAddress(manager, target, trans->apply(target)).as<StatementAddress>();
//		res = measure(stmt, metrics, 1);
//
//		std::cout << "Original: " << core::printer::PrettyPrinter(target) << "\n";
//		std::cout << "Modified: " << core::printer::PrettyPrinter(stmt) << "\n";
//
//		std::cout << "Time, Misses\n";
//		for_each(res, [&](std::map<MetricPtr, Quantity>& cur) {
//			std::cout << cur[Metric::TOTAL_EXEC_TIME_NS].to(ms) << "," << cur[Metric::TOTAL_L3_CACHE_MISS] << "\n";
//		});
//
//
//	}
//
//	vector<std::map<MetricPtr, Quantity>> measure(const StatementAddress& stmt, int tileSize) {
//
//		// transform code
//		TransformationPtr trans = makeNoOp();
//		if (tileSize > 1) {
//			trans = makeLoopTiling(tileSize, tileSize, tileSize);
//		}
//
//		// execute remotely
//		// auto executor = makeRemoteExecutor("m01", "csaf7445");
//
//		// measure transformed code
//		auto metrics = toVector(Metric::TOTAL_EXEC_TIME_NS, Metric::TOTAL_L1_DATA_CACHE_MISS, Metric::TOTAL_L2_CACHE_MISS, Metric::TOTAL_L3_CACHE_MISS);
//		//return measure(trans->apply(stmt), metrics, 1, executor);
//		return measure(trans->apply(stmt), metrics, 1);
//	}
//
//
//	TEST(CacheOptimizerTest, MM_Collect_Time_CacheMiss_Correlation) {
//		Logger::setLevel(ERROR);
//
//		NodeManager manager;
//
//		// load test case
//		ProgramPtr prog = load(manager, "matrix_mul_static");
//		ASSERT_TRUE(prog);
//
//		// find matrix-multiplication loop
//		auto targetFilter = filter::allMatches(insieme::transform::pattern::irp::innerMostForLoopNest(3));
//
//		vector<NodeAddress> targets = targetFilter(prog);
//		EXPECT_EQ(1u, targets.size());
//
//		StatementAddress target = targets[0].as<StatementAddress>();
//
//
//		// run the measurement sequence
//
//		// collect execution time and misses
//		auto ms = milli * s;
//
//		std::cout << "TS, Time, L1 Misses, L2 Misses, L3 Misses\n";
//		for (int ts = 1; ts <= 1024; ts = ts << 1) {
//
//			auto res = measure(target, ts);
//			for_each(res, [&](std::map<MetricPtr, Quantity>& cur) {
//				std::cout << ts << ", ";
//				std::cout << cur[Metric::TOTAL_EXEC_TIME_NS].to(ms) << ",";
//				std::cout << cur[Metric::TOTAL_L1_DATA_CACHE_MISS] << ",";
//				std::cout << cur[Metric::TOTAL_L2_CACHE_MISS] << ",";
//				std::cout << cur[Metric::TOTAL_L3_CACHE_MISS] << "\n";
//			});
//
//		}
//
//	}

}
