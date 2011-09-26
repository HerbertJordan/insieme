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

#include "insieme/backend/runtime/runtime_preprocessing.h"

#include "insieme/core/expressions.h"
#include "insieme/core/ast_builder.h"
#include "insieme/core/transform/node_mapper_utils.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/encoder/encoder.h"

#include "insieme/backend/runtime/runtime_extensions.h"
#include "insieme/backend/runtime/runtime_entities.h"

namespace insieme {
namespace backend {
namespace runtime {


	namespace {


		core::StatementPtr registerEntryPoint(core::NodeManager& manager, const core::ExpressionPtr& workItemImpl) {
			core::ASTBuilder builder(manager);
			auto& basic = manager.getBasicGenerator();
			auto& extensions = manager.getLangExtension<Extensions>();

			// create register call
			return builder.callExpr(basic.getUnit(), extensions.registerWorkItemImpl, workItemImpl);
		}

		WorkItemImpl wrapEntryPoint(core::NodeManager& manager, const core::ExpressionPtr& entry) {
			core::ASTBuilder builder(manager);
			const core::lang::BasicGenerator& basic = manager.getBasicGenerator();
			const Extensions& extensions = manager.getLangExtension<Extensions>();

			// create new lambda expression wrapping the entry point
			assert(entry->getType()->getNodeType() == core::NT_FunctionType && "Only functions can be entry points!");
			core::FunctionTypePtr entryType = static_pointer_cast<const core::FunctionType>(entry->getType());
			assert(entryType->isPlain() && "Only plain functions can be entry points!");


			// define parameter of resulting lambda
			core::VariablePtr workItem = builder.variable(builder.refType(extensions.workItemType));
			core::TypePtr tupleType = DataItem::toLWDataItemType(builder.tupleType(entryType->getParameterTypes()));
			core::ExpressionPtr paramTypes = core::encoder::toIR(manager, tupleType);

			vector<core::ExpressionPtr> argList;
			unsigned counter = 0;
			transform(entryType->getParameterTypes(), std::back_inserter(argList), [&](const core::TypePtr& type) {
				return builder.callExpr(type, extensions.getWorkItemArgument,
						toVector<core::ExpressionPtr>(workItem, core::encoder::toIR(manager, counter++), paramTypes, basic.getTypeLiteral(type)));
			});

			// produce replacement
			core::TypePtr unit = basic.getUnit();
			core::ExpressionPtr call = builder.callExpr(entryType->getReturnType(), entry, argList);
			core::ExpressionPtr exit = builder.callExpr(unit, extensions.exitWorkItem, workItem);
			WorkItemVariant variant(builder.lambdaExpr(unit, builder.compoundStmt(call, exit), toVector(workItem)));
			return WorkItemImpl(toVector(variant));
		}


		core::ProgramPtr replaceMain(core::NodeManager& manager, const core::ProgramPtr& program) {
			core::ASTBuilder builder(manager);
			auto& basic = manager.getBasicGenerator();
			auto& extensions = manager.getLangExtension<Extensions>();

			core::TypePtr unit = basic.getUnit();
			core::TypePtr intType = basic.getUInt4();

			// build up list of statements for the body
			vector<core::StatementPtr> stmts;

			// -------------------- assemble parameters ------------------------------

			core::VariablePtr argc = builder.variable(basic.getInt4());
			core::VariablePtr argv = builder.variable(builder.refType(builder.arrayType(builder.refType(builder.arrayType(basic.getChar())))));
			vector<core::VariablePtr> params = toVector(argc,argv);

			// ------------------- Add list of entry points --------------------------

			vector<core::ExpressionPtr> workItemImpls;
			for_each(program->getEntryPoints(), [&](const core::ExpressionPtr& entry) {
				core::ExpressionPtr impl = WorkItemImpl::encode(manager, wrapEntryPoint(manager, entry));
				workItemImpls.push_back(impl);
				stmts.push_back(registerEntryPoint(manager, impl));
			});

			// ------------------- Start standalone runtime  -------------------------

			// construct light-weight data item tuple
			core::ExpressionPtr expr = builder.tupleExpr(toVector<core::ExpressionPtr>(argc, argv));
			core::TupleTypePtr tupleType = static_pointer_cast<const core::TupleType>(expr->getType());
			expr = builder.callExpr(DataItem::toLWDataItemType(tupleType), extensions.wrapLWData, toVector(expr));

			// create call to standalone runtime
			if (!workItemImpls.empty()) {
				stmts.push_back(builder.callExpr(unit, extensions.runStandalone, workItemImpls[0], expr));
			}

			// ------------------- Add return   -------------------------

			stmts.push_back(builder.returnStmt(builder.intLit(0)));

			// ------------------- Creation of new main function -------------------------


			core::FunctionTypePtr mainType = builder.functionType(toVector(argc->getType(), argv->getType()), basic.getInt4());

			// create new main function
			core::StatementPtr body = builder.compoundStmt(stmts);
			core::ExpressionPtr main = builder.lambdaExpr(mainType, params, body);

			// return resulting program
			return core::Program::create(manager, toVector(main), true);
		}

	}


	core::NodePtr StandaloneWrapper::process(core::NodeManager& manager, const core::NodePtr& node) {

		// simply convert entry points to work items and add new main
		auto nodeType = node->getNodeType();

		// handle programs specially
		if (nodeType == core::NT_Program) {
			return replaceMain(manager, static_pointer_cast<const core::Program>(node));
		}

		// if it is a expression, wrap it within a program and resolve equally
		if (core::ExpressionPtr expr = dynamic_pointer_cast<const core::Expression>(node)) {
			return replaceMain(manager, core::Program::create(manager, toVector(expr)));
		}

		// nothing to do otherwise
		return node;
	}


	// -----------------------------------------------------------------------------------------
	// 	          Replace parallel and job expressions with work-item equivalents
	// -----------------------------------------------------------------------------------------

	namespace {

		namespace coder = core::encoder;

		/**
		 * A small helper-visitor collecting all variables which should be automatically
		 * captured by jobs for their branches.
		 */
		class VariableCollector : public core::ASTVisitor<> {

			/**
			 * A set of variables to be excluded.
			 */
			const utils::set::PointerSet<core::VariablePtr>& excluded;

			/**
			 * A reference to the resulting list of variables.
			 */
			vector<core::VariablePtr>& list;


		public:

			/**
			 * Creates a new instance of this visitor based on the given list of variables.
			 * @param list the list to be filled by this collector.
			 */
			VariableCollector(const utils::set::PointerSet<core::VariablePtr>& excluded, vector<core::VariablePtr>& list) : core::ASTVisitor<>(false), excluded(excluded), list(list) {}

		protected:

			/**
			 * Visits a variable and adds the variable to the resulting list (without duplicates).
			 * It also terminates the recursive decent.
			 */
			void visitVariable(const core::VariablePtr& var) {
				// collect this variable
				if (excluded.find(var) == excluded.end() && !contains(list, var)) {
					list.push_back(var);
				}
			}

			/**
			 * Visiting a lambda expression terminates the recursive decent since a new scope
			 * is started.
			 */
			void visitLambdaExpr(const core::LambdaExprPtr& lambda) {
				// break recursive decent when new scope is reached
			}

			/**
			 * Do not collect parameters of bind expressions.
			 */
			void visitBindExpr(const core::BindExprPtr& bind) {
				// only visit bound expressions
				auto boundExpressions = bind->getBoundExpressions();
				for_each(boundExpressions, [this](const core::ExpressionPtr& cur) {
					this->visit(cur);
				});
			}

			/**
			 * Types are generally ignored by this visitor for performance reasons (no variables will
			 * occur within types).
			 */
			void visitType(const core::TypePtr& type) {
				// just ignore types
			}

			/**
			 * The default behavior for all other node types is to recursively decent by iterating
			 * through the child-node list.
			 */
			void visitNode(const core::NodePtr& node) {
				assert(node->getNodeType() != core::NT_LambdaExpr);
				// visit all children recursively
				for_each(node->getChildList(), [this](const core::NodePtr& cur){
					this->visit(cur);
				});
			}

		};


		/**
		 * Collects a list of variables to be captures by a job for proper initialization
		 * of the various job branches.
		 */
		vector<core::VariablePtr> getVariablesToBeCaptured(const core::ExpressionPtr& code, const utils::set::PointerSet<core::VariablePtr>& excluded = utils::set::PointerSet<core::VariablePtr>()) {

			vector<core::VariablePtr> res;

			// collect all variables potentially captured by this job
			VariableCollector collector(excluded, res);
			collector.visit(code);

			return res;
		}


		std::pair<WorkItemImpl, core::ExpressionPtr> wrapJob(core::NodeManager& manager, const core::JobExprPtr& job) {
			core::ASTBuilder builder(manager);
			const core::lang::BasicGenerator& basic = manager.getBasicGenerator();
			const Extensions& extensions = manager.getLangExtension<Extensions>();

			// define parameter of resulting lambda
			core::VariablePtr workItem = builder.variable(builder.refType(extensions.workItemType));

			// collect parameters to be captured by the job
			vector<core::VariablePtr> capturedVars = getVariablesToBeCaptured(job);

			// add local declarations
			core::TypeList list;
			core::ExpressionList capturedValues;
			utils::map::PointerMap<core::VariablePtr, unsigned> varIndex;
			for_each(job->getLocalDecls(), [&](const core::DeclarationStmtPtr& cur) {
				varIndex.insert(std::make_pair(cur->getVariable(), list.size()));
				list.push_back(cur->getVariable()->getType());
				capturedValues.push_back(cur->getInitialization());
			});
			for_each(capturedVars, [&](const core::VariablePtr& cur) {
				varIndex.insert(std::make_pair(cur, list.size()));
				list.push_back(cur->getType());
				capturedValues.push_back(cur);
			});

			core::TypePtr unit = basic.getUnit();

			// create variable replacement map
			core::TupleTypePtr tupleType = builder.tupleType(list);
			core::TypePtr dataItemType = DataItem::toLWDataItemType(tupleType);
			core::ExpressionPtr paramTypeToken = coder::toIR<core::TypePtr>(manager, dataItemType);
			utils::map::PointerMap<core::VariablePtr, core::ExpressionPtr> varReplacements;
			for_each(varIndex, [&](const std::pair<core::VariablePtr, unsigned>& cur) {
				core::TypePtr varType = cur.first->getType();
				core::ExpressionPtr index = coder::toIR(manager, cur.second);
				core::ExpressionPtr access = builder.callExpr(varType, extensions.getWorkItemArgument,
						toVector<core::ExpressionPtr>(workItem, index, paramTypeToken, coder::toIR(manager, varType)));
				varReplacements.insert(std::make_pair(cur.first, access));
			});

			auto fixVariables = [&](const core::ExpressionPtr& cur)->core::ExpressionPtr {
				return core::transform::replaceVarsGen(manager, cur, varReplacements);
			};

			auto fixBranch = [&](const core::ExpressionPtr& branch)->core::ExpressionPtr {
				core::CallExprPtr call = builder.callExpr(unit, branch, core::ExpressionList());
				core::ExpressionPtr res = core::transform::tryInlineToExpr(manager, call);
				return fixVariables(res);
			};

			// create function processing the job (forming the entry point)
			core::StatementList body;
			core::StatementPtr returnStmt = builder.returnStmt(basic.getUnitConstant());
			for(auto it = job->getGuardedStmts().begin(); it != job->getGuardedStmts().end(); ++it) {
				const core::JobExpr::GuardedStmt& cur = *it;
				core::ExpressionPtr condition = fixVariables(cur.first);
				core::ExpressionPtr branch = fixBranch(cur.second);
				body.push_back(builder.ifStmt(condition, builder.compoundStmt(branch, returnStmt)));
			}

			// add default branch
			body.push_back(fixBranch(job->getDefaultStmt()));

			// add exit work-item call
			body.push_back(builder.callExpr(unit, extensions.exitWorkItem, workItem));

			// produce work item implementation
			WorkItemVariant variant(builder.lambdaExpr(unit, builder.compoundStmt(body), toVector(workItem)));
			WorkItemImpl impl(toVector(variant));


			// ------------------- initialize work item parameters -------------------------

			// construct light-weight data item tuple
			core::ExpressionPtr tuple = builder.tupleExpr(capturedValues);
			core::ExpressionPtr parameters = builder.callExpr(dataItemType, extensions.wrapLWData, toVector(tuple));

			// return implementation + parameters
			return std::make_pair(impl, parameters);
		}


		class WorkItemIntroducer : public core::transform::CachedNodeMapping {

			core::NodeManager& manager;
			const core::lang::BasicGenerator& basic;
			const Extensions& ext;
			core::ASTBuilder builder;

		public:

			WorkItemIntroducer(core::NodeManager& manager)
				: manager(manager), basic(manager.getBasicGenerator()),
				  ext(manager.getLangExtension<Extensions>()), builder(manager) {}

			virtual const core::NodePtr resolveElement(const core::NodePtr& ptr) {

				// skip types
				if (ptr->getNodeCategory() == core::NC_Type) {
					return ptr; // don't touch types
				}

				// start by processing the child nodes (bottom up)
				core::NodePtr res = ptr->substitute(manager, *this);

				// test whether it is something of interest
				if (res->getNodeType() == core::NT_JobExpr) {
					return convertJob(static_pointer_cast<const core::JobExpr>(res));
				}

				// handle parallel call
				if (core::analysis::isCallOf(res, basic.getParallel())) {

					const core::ExpressionPtr& job = core::analysis::getArgument(res, 0);
					assert(*job->getType() == *ext.jobType && "Argument hasn't been converted!");
					return builder.callExpr(builder.refType(ext.workItemType), ext.parallel, job);
				}

				// handle merge call
				if (core::analysis::isCallOf(res, basic.getMerge())) {
					return builder.callExpr(basic.getUnit(), ext.merge, core::analysis::getArgument(res, 0));
				}

				// handle pfor calls
				if (core::analysis::isCallOf(res, basic.getPFor())) {
					return convertPfor(static_pointer_cast<const core::CallExpr>(res));
				}

				// nothing interesting ...
				return res;
			}

		private:

			core::ExpressionPtr convertJob(const core::JobExprPtr& job) {

				// extract range
				WorkItemRange range = coder::toValue<WorkItemRange>(job->getThreadNumRange());

				// create job parameters
				core::ExpressionPtr min = range.min;
				core::ExpressionPtr max = range.max;
				core::ExpressionPtr mod = range.mod;

				auto info = wrapJob(manager, job);
				core::ExpressionPtr wi = coder::toIR(manager, info.first);
				core::ExpressionPtr data = info.second;

				// create call to job constructor
				return builder.callExpr(ext.jobType, ext.createJob, toVector(min,max,mod, wi, data));

			}

			core::ExpressionPtr convertPfor(const core::CallExprPtr& call) {
				// check that it is indeed a pfor call
				assert(core::analysis::isCallOf(call, basic.getPFor()));

				// construct call to pfor ...
				const core::ExpressionList& args = call->getArguments();

				auto info = pforBodyToWorkItem(args[4]);
				core::ExpressionPtr bodyImpl = coder::toIR(manager, info.first);
				core::ExpressionPtr data = info.second;
				core::TypePtr resType = builder.refType(ext.workItemType);

				return builder.callExpr(resType, ext.pfor,
						toVector(args[0], args[1], args[2], args[3], bodyImpl, data));

//				irt_work_item* irt_pfor(irt_work_item* self, irt_work_group* group, irt_work_item_range range, irt_wi_implementation_id impl_id, irt_lw_data_item* args);
			}

			std::pair<WorkItemImpl, core::ExpressionPtr> pforBodyToWorkItem(const core::ExpressionPtr& body) {

				// ------------- build captured data -------------

				// collect variables to be captured
				vector<core::VariablePtr> captured = getVariablesToBeCaptured(body);

				// create tuple of captured data
				core::TypeList typeList;
				core::ExpressionList capturedValues;
				for_each(captured, [&](const core::VariablePtr& cur) {
					typeList.push_back(cur->getType());
					capturedValues.push_back(cur);
				});

				// construct light-weight data tuple to be passed to work item
				core::TupleTypePtr tupleType = builder.tupleType(typeList);
				core::TypePtr dataItemType = DataItem::toLWDataItemType(tupleType);
				core::ExpressionPtr tuple = builder.tupleExpr(capturedValues);
				core::ExpressionPtr data = builder.callExpr(dataItemType, ext.wrapLWData, toVector(tuple));


				// ------------- build up function computing for-loop body -------------

				core::StatementList resBody;

				// define parameter of resulting lambda
				core::VariablePtr workItem = builder.variable(builder.refType(ext.workItemType));

				// create variables containing loop boundaries
				core::TypePtr unit = basic.getUnit();
				core::TypePtr int4 = basic.getInt4();

				core::VariablePtr range = builder.variable(ext.workItemRange);
				core::VariablePtr begin = builder.variable(int4);
				core::VariablePtr end = builder.variable(int4);
				core::VariablePtr step = builder.variable(int4);

				resBody.push_back(builder.declarationStmt(range, builder.callExpr(ext.workItemRange, ext.getWorkItemRange, workItem)));
				resBody.push_back(builder.declarationStmt(begin, builder.accessMember(range, "begin")));
				resBody.push_back(builder.declarationStmt(end, 	 builder.accessMember(range, "end")));
				resBody.push_back(builder.declarationStmt(step,  builder.accessMember(range, "step")));

				// create loop calling body of p-for
				core::VariablePtr iterator = builder.variable(int4);
				core::CallExprPtr loopBodyCall = builder.callExpr(unit, body, iterator);
				core::ExpressionPtr loopBody = core::transform::tryInlineToExpr(manager, loopBodyCall);

				// replace variables within loop body to fit new context
				core::ExpressionPtr paramTypeToken = coder::toIR<core::TypePtr>(manager, dataItemType);
				utils::map::PointerMap<core::VariablePtr, core::ExpressionPtr> varReplacements;
				unsigned count = 0;
				for_each(captured, [&](const core::VariablePtr& cur) {
					core::TypePtr varType = cur->getType();
					core::ExpressionPtr index = coder::toIR(manager, count++);
					core::ExpressionPtr access = builder.callExpr(varType, ext.getWorkItemArgument,
							toVector<core::ExpressionPtr>(workItem, index, paramTypeToken, coder::toIR(manager, varType)));
					varReplacements.insert(std::make_pair(cur, access));
				});

				loopBody = core::transform::replaceVarsGen(manager, loopBody, varReplacements);

				// build for loop
				core::DeclarationStmtPtr iterDecl = builder.declarationStmt(iterator, begin);
				core::ForStmtPtr forStmt = builder.forStmt(iterDecl, loopBody, end, step);
				resBody.push_back(forStmt);

				// add exit work-item call
				resBody.push_back(builder.callExpr(unit, ext.exitWorkItem, workItem));

				core::LambdaExprPtr lambda = builder.lambdaExpr(unit, builder.compoundStmt(resBody), toVector(workItem));
				WorkItemImpl impl(toVector(WorkItemVariant(lambda)));

				// combine results into a pair
				return std::make_pair(impl, data);
			}
		};

	}


	core::NodePtr WorkItemizer::process(core::NodeManager& manager, const core::NodePtr& node) {
		return WorkItemIntroducer(manager).resolveElement(node);
	}


} // end namespace runtime
} // end namespace backend
} // end namespace insieme