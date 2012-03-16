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

#include "insieme/analysis/features/code_feature_catalog.h"

#include "insieme/transform/pattern/ir_generator.h"
#include "insieme/transform/pattern/ir_pattern.h"

#include "insieme/core/ir_node.h"

namespace itpi = insieme::transform::pattern::irp;

namespace insieme {
namespace analysis {
namespace features {

	namespace {

		void addScalarFeatures(const core::lang::BasicGenerator& basic, FeatureCatalog& catalog) {

			// create lists of considered types
			std::map<string, vector<core::TypePtr>> types;

			types["any"] = vector<core::TypePtr>();

			types["char"] = toVector(basic.getChar());

			types["int1"] = toVector(basic.getInt1());
			types["int2"] = toVector(basic.getInt2());
			types["int4"] = toVector(basic.getInt4());
			types["int8"] = toVector(basic.getInt8());
			types["int*"] = core::convertList<core::Type>(basic.getSignedIntGroup());

			types["uint1"] = toVector(basic.getUInt1());
			types["uint2"] = toVector(basic.getUInt2());
			types["uint4"] = toVector(basic.getUInt4());
			types["uint8"] = toVector(basic.getUInt8());
			types["uint*"] = core::convertList<core::Type>(basic.getUnsignedIntGroup());

			types["integer"] = core::convertList<core::Type>(basic.getIntGroup());

			types["real4"] = toVector(basic.getFloat());
			types["real8"] = toVector(basic.getDouble());
			types["real*"] = core::convertList<core::Type>(basic.getRealGroup());

			// create lists of considered operations
			std::map<string, vector<core::ExpressionPtr>> ops;

			ops["arithmetic"] = core::convertList<core::Expression>(basic.getArithOpGroup());
			ops["comparison"] = core::convertList<core::Expression>(basic.getCompOpGroup());
			ops["bitwise"] = core::convertList<core::Expression>(basic.getBitwiseOpGroup());

			ops["all"] = vector<core::ExpressionPtr>();
			addAll(ops["all"], ops["arithmetic"]);
			addAll(ops["all"], ops["comparison"]);
			addAll(ops["all"], ops["bitwise"]);

			// modes
			std::map<string, FeatureAggregationMode> modes;
			modes["static"] = FA_Static;
			modes["weighted"] = FA_Weighted;
			modes["real"] = FA_Real;
			modes["polyhedral"] = FA_Polyhedral;


			// create the actual features
			for_each(types, [&](const std::pair<string, vector<core::TypePtr>>& cur_type) {
				for_each(ops, [&](const std::pair<string, vector<core::ExpressionPtr>>& cur_ops){
					for_each(modes, [&](const std::pair<string, FeatureAggregationMode>& cur_mode) {

						string name = format("SCF_NUM_%s_%s_OPs_%s", cur_type.first.c_str(),
								cur_ops.first.c_str(), cur_mode.first.c_str());

						string desc = format("Counts the number of %s operations producing values of type %s - aggregation mode: %s",
								cur_ops.first.c_str(), cur_type.first.c_str(), cur_mode.first.c_str());

						catalog.addFeature(createSimpleCodeFeature(name, desc, SimpleCodeFeatureSpec(cur_type.second, cur_ops.second, cur_mode.second)));
					});
				});
			});
		}

		void addVectorFeatures(const core::lang::BasicGenerator& basic, FeatureCatalog& catalog) {

			// create list of considered types
			std::map<string, vector<core::TypePtr>> types;

			types["any"] = vector<core::TypePtr>();

			types["char"] = toVector(basic.getChar());

			types["int1"] = toVector(basic.getInt1());
			types["int2"] = toVector(basic.getInt2());
			types["int4"] = toVector(basic.getInt4());
			types["int8"] = toVector(basic.getInt8());
			types["int*"] = core::convertList<core::Type>(basic.getSignedIntGroup());

			types["uint1"] = toVector(basic.getUInt1());
			types["uint2"] = toVector(basic.getUInt2());
			types["uint4"] = toVector(basic.getUInt4());
			types["uint8"] = toVector(basic.getUInt8());
			types["uint*"] = core::convertList<core::Type>(basic.getUnsignedIntGroup());

			types["integer"] = core::convertList<core::Type>(basic.getIntGroup());

			types["real4"] = toVector(basic.getFloat());
			types["real8"] = toVector(basic.getDouble());
			types["real*"] = core::convertList<core::Type>(basic.getRealGroup());


			// create a list of considered operation classes
			std::map<string, vector<core::ExpressionPtr>> ops;

			ops["arithmetic"] = core::convertList<core::Expression>(basic.getArithOpGroup());
			ops["comparison"] = core::convertList<core::Expression>(basic.getCompOpGroup());
			ops["bitwise"] = core::convertList<core::Expression>(basic.getBitwiseOpGroup());

			ops["all"] = vector<core::ExpressionPtr>();
			addAll(ops["all"], ops["arithmetic"]);
			addAll(ops["all"], ops["comparison"]);
			addAll(ops["all"], ops["bitwise"]);

			// modes
			std::map<string, FeatureAggregationMode> modes;
			modes["static"] = FA_Static;
			modes["weighted"] = FA_Weighted;
			modes["real"] = FA_Real;
			modes["polyhedral"] = FA_Polyhedral;

			// create the actual features
			for_each(types, [&](const std::pair<string, vector<core::TypePtr>>& cur_type) {
				for_each(ops, [&](const std::pair<string, vector<core::ExpressionPtr>>& cur_ops){
					for_each(modes, [&](const std::pair<string, FeatureAggregationMode>& cur_mode) {

						string name = format("SCF_NUM_%s_%s_VEC_OPs_%s", cur_type.first.c_str(),
								cur_ops.first.c_str(), cur_mode.first.c_str());

						string desc = format("Counts the number of vectorized %s operations operating on %s - aggregation mode: %s",
								cur_ops.first.c_str(), cur_type.first.c_str(), cur_mode.first.c_str());

						catalog.addFeature(createSimpleCodeFeature(name, desc, createVectorOpSpec(cur_type.second, cur_ops.second, -1, cur_mode.second)));
					});
				});
			});

		}

		void addMemoryAccessFeatures(const core::lang::BasicGenerator& basic, FeatureCatalog& catalog) {

			// is adding features for:
			// 	- number of read operations
			//	- number of write operations
			//	- number of scalar read operations
			//  - number of scalar write operations
			//	- number of vector read operations
			//	- number of vector write operations
			//	- number of array read operations
			//	- number of array write operations

			// still to do: Volume
			//	- volume of read operations
			//	- volume of write operations

			std::map<string, MemoryAccessMode> access;
			access["read"] = MemoryAccessMode::READ;
			access["write"] = MemoryAccessMode::WRITE;
			access["read/write"] = MemoryAccessMode::READ_WRITE;

			std::map<string, MemoryAccessTarget> target;
			target["any"] = MemoryAccessTarget::ANY;
			target["scalar"] = MemoryAccessTarget::SCALAR;
			target["vector"] = MemoryAccessTarget::VECTOR;
			target["array"] = MemoryAccessTarget::ARRAY;

			// modes
			std::map<string, FeatureAggregationMode> modes;
			modes["static"] = FA_Static;
			modes["weighted"] = FA_Weighted;
			modes["real"] = FA_Real;
			modes["polyhedral"] = FA_Polyhedral;

			// create the actual features
			for_each(target, [&](const std::pair<string, MemoryAccessTarget>& cur_target){
				for_each(access, [&](const std::pair<string, MemoryAccessMode>& cur_mode) {
					for_each(modes, [&](const std::pair<string, FeatureAggregationMode>& cur_aggreagation) {

						string name = format("SCF_IO_NUM_%s_%s_OPs_%s", cur_target.first.c_str(),
								cur_mode.first.c_str(), cur_aggreagation.first.c_str());

						string desc = format("Counts the number of %s memory accesses to %s elements - aggregation mode: %s",
								cur_mode.first.c_str(), cur_target.first.c_str(), cur_aggreagation.first.c_str());

						catalog.addFeature(createSimpleCodeFeature(name, desc,
								createMemoryAccessSpec(cur_mode.second, cur_target.second, cur_aggreagation.second))
						);
					});
				});
			});


		}

		// add features related to parallelism (e.g. number of barrieres)
		void addParallelFeatures(const core::lang::BasicGenerator& basic, FeatureCatalog& catalog) {
			// create lists of considered operations
			std::map<string, core::ExpressionPtr> ops;
			ops["barrier"] = basic.getBarrier();

			// not sure if all makes sense in this case...
//			ops["all"] = vector<core::ExpressionPtr>();
//			addAll(ops["all"], ops["barrier"]);

			// modes
			std::map<string, FeatureAggregationMode> modes;
			modes["static"] = FA_Static;
			modes["weighted"] = FA_Weighted;
			modes["real"] = FA_Real;
			modes["polyhedral"] = FA_Polyhedral;


			// create the actual features
			for_each(ops, [&](const std::pair<string, core::ExpressionPtr>& cur_ops){
				for_each(modes, [&](const std::pair<string, FeatureAggregationMode>& cur_mode) {

					string name = format("SCF_NUM_%s_Calls_%s",
							cur_ops.first.c_str(), cur_mode.first.c_str());

					string desc = format("Counts the number of %s - aggregation mode: %s",
							cur_ops.first.c_str(), cur_mode.first.c_str());

					catalog.addFeature(createSimpleCodeFeature(name, desc, SimpleCodeFeatureSpec(basic.getUnit(), cur_ops.second, cur_mode.second)));
				});
			});
		}


		// add features that count occurrences of certain patterns
		void addPatternFeatures(const core::lang::BasicGenerator& basic, FeatureCatalog& catalog) {
			// create lists of considered operations
			std::map<string, transform::pattern::TreePatternPtr> patterns;
//			patterns["externalFunctions"] = itpi::callExpr(itpi::literal(any), *any);

			// not sure if all makes sense in this case...
//			ops["all"] = vector<core::ExpressionPtr>();
//			addAll(ops["all"], ops["barrier"]);

			// modes
			std::map<string, FeatureAggregationMode> modes;
			modes["static"] = FA_Static;
			modes["weighted"] = FA_Weighted;
			modes["real"] = FA_Real;
			modes["polyhedral"] = FA_Polyhedral;


			// create the actual features
			for_each(patterns, [&](const std::pair<string, transform::pattern::TreePatternPtr>& cur_pattern){
				for_each(modes, [&](const std::pair<string, FeatureAggregationMode>& cur_mode) {

					string name = format("SCF_NUM_%s_Calls_%s",
							cur_pattern.first.c_str(), cur_mode.first.c_str());

					string desc = format("Counts the number of %s - aggregation mode: %s",
							cur_pattern.first.c_str(), cur_mode.first.c_str());

					catalog.addFeature(createPatternCodeFeature(name, desc, PatternCodeFeatureSpec(cur_pattern.second, cur_mode.second)));
				});
			});
		}

		FeatureCatalog initCatalog() {
			// the node manager managing nodes inside the catalog
			static core::NodeManager manager;
			auto& basic = manager.getLangBasic();

			FeatureCatalog catalog;

			addScalarFeatures(basic, catalog);
			addVectorFeatures(basic, catalog);
			addMemoryAccessFeatures(basic, catalog);
			addParallelFeatures(basic, catalog);




			return catalog;
		}

	}

	const FeatureCatalog& getFullCodeFeatureCatalog() {
		const static FeatureCatalog catalog = initCatalog();
		return catalog;
	}

} // end namespace features
} // end namespace analysis
} // end namespace insieme

