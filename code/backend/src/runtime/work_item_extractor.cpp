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

#include "insieme/backend/runtime/work_item_extractor.h"

#include "insieme/c_info/naming.h"

#include "insieme/core/expressions.h"
#include "insieme/core/ast_builder.h"
#include "insieme/core/transform/node_mapper_utils.h"

#include "insieme/backend/runtime/runtime_extensions.h"

namespace insieme {
namespace backend {
namespace runtime {


	namespace {


		core::StatementPtr registerEntryPoint(core::NodeManager& manager, const core::ExpressionPtr& entry) {
			core::ASTBuilder builder(manager);
			auto& basic = manager.getBasicGenerator();
			Extensions extensions(manager);

			// check whether entry expression is of a function type
			assert(entry->getType()->getNodeType() == core::NT_FunctionType && "Only functions can be entry points!");

			return builder.callExpr(basic.getUnit(), extensions.registerWorkItem, entry);
		}


		core::ProgramPtr extractWorkItems(core::NodeManager& manager, const core::ProgramPtr& program) {
			core::ASTBuilder builder(manager);
			auto& basic = manager.getBasicGenerator();
			Extensions extensions(manager);

			core::TypePtr unit = basic.getUnit();
			core::TypePtr intType = basic.getUInt4();

			// build up list of statements for the body
			vector<core::StatementPtr> stmts;

			// ------------------- Start with a call to init -------------------------

			stmts.push_back(builder.callExpr(unit, extensions.initRuntime));

			// ------------------- Add list of entry points --------------------------

			for_each(program->getEntryPoints(), [&](const core::ExpressionPtr& entry) {
				stmts.push_back(registerEntryPoint(manager, entry));
			});

			// ------------------- Start standalone runtime  -------------------------

			// add call to irt_get_default_worker_count
			core::TypePtr getWorkerCountType = builder.functionType(core::TypeList(), intType);
			core::ExpressionPtr getWorkerCount = builder.callExpr(intType, builder.literal(getWorkerCountType,"irt_get_default_worker_count"), toVector<core::ExpressionPtr>());

			// add call to irt_runtime_standalone
			core::TypePtr contextFunType = builder.functionType(core::TypeList(), manager.basic.getUnit());
			core::TypePtr standaloneFunType = builder.functionType(toVector(intType, contextFunType,contextFunType), unit);
			core::ExpressionPtr standalone = builder.literal(standaloneFunType, "irt_runtime_standalone");
			core::ExpressionPtr start = builder.callExpr(unit, standalone, toVector(getWorkerCount, extensions.initContext, extensions.cleanupContext));

			stmts.push_back(start);

			// ------------------- Creation of new main function -------------------------

			// assemble parameters
			vector<core::VariablePtr> params; // no parameters so far (not supported)

			core::FunctionTypePtr mainType = builder.functionType(core::TypeList(), unit);

			// create new main function
			core::StatementPtr body = builder.compoundStmt(stmts);
			core::ExpressionPtr main = builder.lambdaExpr(mainType, params, body);

			// return resulting program
			return core::Program::create(manager, toVector(main), true);
		}

		/**
		 *
		 */
		core::NodePtr extractWorkItems(core::NodeManager& manager, const core::NodePtr& node) {
			auto nodeType = node->getNodeType();

			// handle programs specially
			if (nodeType == core::NT_Program) {
				return extractWorkItems(manager, static_pointer_cast<const core::Program>(node));
			}

			// if it is a expression, wrap it within a program and resolve equally
			if (core::ExpressionPtr expr = dynamic_pointer_cast<const core::Expression>(node)) {
				return extractWorkItems(manager, core::Program::create(manager, toVector(expr)));
			}

			// nothing to do otherwise
			return node;
		}

	}



	core::NodePtr WorkItemExtractor::process(core::NodeManager& manager, const core::NodePtr& code) {

		// TODO:
		//    - convert entry points to work items
		// 	  - create alternative main conducting a runtime call (+ initContext())
		//	  - identification and creation of work items


		core::NodePtr res = extractWorkItems(manager, code);

		// so far, nothing
		return res;
	}


} // end namespace runtime
} // end namespace backend
} // end namespace insieme
