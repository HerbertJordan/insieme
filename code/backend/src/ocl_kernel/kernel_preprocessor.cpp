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

#include <algorithm>

#include "insieme/utils/logging.h"
#include "insieme/utils/map_utils.h"
#include "insieme/utils/container_utils.h"

#include "insieme/core/expressions.h"
#include "insieme/core/ast_builder.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/transform/node_mapper_utils.h"
#include "insieme/core/transform/node_replacer.h"

#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/ast_check.h"
#include "insieme/core/checks/ir_checks.h"

#include "insieme/annotations/c/naming.h"
#include "insieme/annotations/ocl/ocl_annotations.h"

#include "insieme/backend/ocl_kernel/kernel_extensions.h"
#include "insieme/backend/ocl_kernel/kernel_preprocessor.h"

namespace insieme {
namespace backend {
namespace ocl_kernel {

	using namespace insieme::annotations::ocl;

	namespace {

		/**
		 * Tests whether the given lambda is marked to converted into an OpenCL kernel.
		 */
		bool isKernel(const core::LambdaExprPtr& lambda) {
			if (!lambda->hasAnnotation(BaseAnnotation::KEY)) {
				return false;
			}

			BaseAnnotationPtr&& annotations = lambda->getAnnotation(BaseAnnotation::KEY);
			assert(annotations && "BaseAnnotation is empty");

			return any(annotations->getAnnotationList(), [](const BaseAnnotation::SubAnnotationPtr& cur) {
				return dynamic_pointer_cast<KernelFctAnnotation>(cur);
			});
		}

		core::JobExprPtr getGlobalJob(const core::LambdaExprPtr& kernel) {
			core::StatementPtr body = kernel->getLambda()->getBody();
			core::StatementPtr expr = static_pointer_cast<const core::CompoundStmt>(body)->getStatements()[1];
			return static_pointer_cast<const core::JobExpr>(core::analysis::getArgument(expr, 0));
		}

		typedef utils::map::PointerMap<core::VariablePtr, AddressSpace> AddressSpaceMap;

		/**
		 * Determines for each of the parameters of the given kernel whether it is referencing
		 * a memory location within the local or global memory space.
		 */
		AddressSpaceMap getAddressSpaces(const core::LambdaExprPtr& kernel) {

			// The address spaces are deduced as follows:
			//	- variables used to initialized values within the local declarations of
			//		the global job are referencing memory locations within the global memory
			//	- all other variables are referencing local memory locations


			// get outer-most job expression
			core::JobExprPtr job = getGlobalJob(kernel);

			// collect list of initialization values
			std::vector<core::ExpressionPtr> initValues;
			for_each(job->getLocalDecls(), [&](const core::DeclarationStmtPtr& cur) {
				initValues.push_back(cur->getInitialization());
			});

			// fix address spaces for each parameter
			AddressSpaceMap res;
			auto& params = kernel->getParameterList();
			for_each(params.begin(), params.end()-2, [&](const core::VariablePtr& cur) {
				AddressSpace space = AddressSpace::PRIVATE;
				if (cur->getType()->getNodeType() == core::NT_RefType) {
					space = (contains(initValues, cur)) ? AddressSpace::GLOBAL : AddressSpace::LOCAL;
				}
				res[cur] = space;
			});
			return res;
		}


		/**
		 * Maps variables to their origins. The origin of a variable is either another variable
		 * within an outer scope or a initialization value.
		 */
		typedef utils::map::PointerMap<core::VariablePtr, core::ExpressionPtr> VariableMap;

		/**
		 * Combines the given variable maps by computing the concatenation of res and other.
		 * Hence, if A was mapped to B in res and B was mapped to C in other, the resulting
		 * map will map A to C. The resulting map will be the res map - which is modified
		 * during the execution.
		 */
		VariableMap& compose(VariableMap& res, const VariableMap& other) {
			for_each(res, [&](VariableMap::value_type& entry){
				const core::ExpressionPtr& value = entry.second;
				if (value->getNodeType() == core::NT_Variable) {
					auto pos = other.find(static_pointer_cast<const core::Variable>(value));
					if (pos != other.end()) {
						entry.second = pos->second;
					}
				}
			});
			return res;
		}


		VariableMap& mapBodyVars(VariableMap& res, const core::CallExprPtr& call);
		VariableMap& mapBodyVars(VariableMap& res, const core::JobExprPtr& job);


		VariableMap& mapBodyVars(VariableMap& res, const core::CallExprPtr& call) {
			auto& basic = call->getNodeManager().getBasicGenerator();

			core::LambdaExprPtr fun = static_pointer_cast<const core::LambdaExpr>(call->getFunctionExpr());
			auto body = fun->getBody();
			if (body->getNodeType() == core::NT_CompoundStmt) {
				body = static_pointer_cast<const core::CompoundStmt>(body)->getStatements()[0];
			}

			// check for termination
			if (!core::analysis::isCallOf(body, basic.getParallel())) {
				// this is the innermost lambda containing the kernel code
				for_each(fun->getParameterList(), [&](const core::VariablePtr& param) {
					res[param] = param;
				});
			} else {
				res = mapBodyVars(res, static_pointer_cast<const core::JobExpr>(core::analysis::getArgument(body, 0)));
			}

			// compute local parameter to argument mapping
			VariableMap cur;
			for_range(make_paired_range(fun->getParameterList(), call->getArguments()),
					[&](const std::pair<const core::VariablePtr, const core::ExpressionPtr>& pair) {
						cur[pair.first] = pair.second;
			});

			// compose result of sub-tree and call
			return compose(res, cur);
		}

		VariableMap& mapBodyVars(VariableMap& res, const core::JobExprPtr& job) {

			// compute variable map recursively
			core::BindExprPtr bind = static_pointer_cast<const core::BindExpr>(job->getDefaultStmt());
			mapBodyVars(res, bind->getCall());

			// update variable map ...
			core::DeclarationStmtPtr decl;

			// compute local local-declaration mapping
			VariableMap cur;
			for_each(job->getLocalDecls(), [&](const core::DeclarationStmtPtr& decl) {
				cur[decl->getVariable()] = decl->getInitialization();
			});

			// compose result of sub-tree and call
			return compose(res, cur);
		}

		VariableMap mapBodyVars(const core::LambdaExprPtr& kernel) {
			VariableMap map;
			return mapBodyVars(map, getGlobalJob(kernel));
		}


		core::StatementPtr getKernelCore(const core::BindExprPtr& bind) {
			auto& basic = bind->getNodeManager().getBasicGenerator();

			core::StatementPtr body = static_pointer_cast<const core::LambdaExpr>(bind->getCall()->getFunctionExpr())->getBody();
			if (body->getNodeType() == core::NT_CompoundStmt) {
				const vector<core::StatementPtr>& stmts = static_pointer_cast<const core::CompoundStmt>(body)->getStatements();
				if (core::analysis::isCallOf(stmts[0], basic.getParallel())) {
					body = stmts[0];
				}
			}

			if (!core::analysis::isCallOf(body, basic.getParallel())) {
				return body;
			}

			core::JobExprPtr job = static_pointer_cast<const core::JobExpr>(core::analysis::getArgument(body, 0));
			return getKernelCore(static_pointer_cast<const core::BindExpr>(job->getDefaultStmt()));
		}


		core::StatementPtr getKernelCore(const core::LambdaExprPtr& lambda) {
			return getKernelCore(static_pointer_cast<const core::BindExpr>(getGlobalJob(lambda)->getDefaultStmt()));
		}


		bool isGetIDHelper(const core::ExpressionPtr& expr, std::size_t length) {

			if (expr->getNodeType() != core::NT_LambdaExpr) {
				return false;
			}

			const core::LambdaExprPtr& lambda = static_pointer_cast<const core::LambdaExpr>(expr);

			core::StatementPtr body = lambda->getBody();
			if (body->getNodeType() != core::NT_CompoundStmt) {
				return false;
			}

			vector<core::StatementPtr> stmts = static_pointer_cast<const core::CompoundStmt>(body)->getStatements();
			if (stmts.size() != length) {
				return false;
			}

			for (std::size_t i = 0; i< length-1; i++) {
				if (stmts[i]->getNodeType() != core::NT_DeclarationStmt) {
					return false;
				}
			}

			return stmts[length-1]->getNodeType() == core::NT_SwitchStmt;
		}

		bool isGetLocalID(const core::ExpressionPtr& expr) {
			return isGetIDHelper(expr, 2);
		}

		bool isGetGlobalID(const core::ExpressionPtr& expr) {
			return isGetIDHelper(expr, 3);
		}


		class BuildInReplacer : public core::transform::CachedNodeMapping {

			core::NodeManager& manager;

			core::VariablePtr globalSizeVar;
			core::VariablePtr localSizeVar;
			core::VariablePtr numGroupsVar;

		public:

			BuildInReplacer(core::NodeManager& manager, const core::VariablePtr& globalSize, const core::VariablePtr& localSize, const core::VariablePtr& numGroups)
					: manager(manager),  globalSizeVar(globalSize), localSizeVar(localSize), numGroupsVar(numGroups) {}


			const core::NodePtr resolveElement(const core::NodePtr& ptr) {

				core::ASTBuilder builder(manager);
				auto& basic = manager.getBasicGenerator();
				auto& extensions = manager.getLangExtension<Extensions>();

				// perform conversion in post-order
				core::NodePtr res = ptr->substitute(manager, *this);

				// only interested in lambda expressions
				if (ptr->getNodeType() != core::NT_CallExpr) {
					return res;
				}

				core::CallExprPtr call = static_pointer_cast<const core::CallExpr>(res);

				// ceck for access to global ids
				const core::TypePtr uint4 = basic.getUInt4();
				auto& fun = call->getFunctionExpr();
				if (isGetGlobalID(fun)) {
					return builder.callExpr(uint4, extensions.getGlobalID, toVector(call->getArgument(0)));
				}

				if (isGetLocalID(fun)) {
					return builder.callExpr(uint4, extensions.getLocalID, toVector(call->getArgument(0)));
				}

				if (basic.isVectorSubscript(fun)) {
					auto target = call->getArgument(0);
					auto index = call->getArgument(1);

					if (*target == *globalSizeVar) {
						return builder.callExpr(uint4, extensions.getGlobalSize, toVector(index));
					}
					if (*target == *localSizeVar) {
						return builder.callExpr(uint4, extensions.getLocalSize, toVector(index));
					}
					if (*target == *numGroupsVar) {
						return builder.callExpr(uint4, extensions.getNumGroups, toVector(index));
					}
				}

				if (basic.isBarrier(fun)) {
					auto threadGroup = call->getArgument(0);
					if (core::analysis::isCallOf(threadGroup, basic.getGetThreadGroup())) {
						auto arg = core::analysis::getArgument(threadGroup, 0);
						if (arg->getNodeType() == core::NT_Literal) {
							core::LiteralPtr argument = static_pointer_cast<const core::Literal>(arg);
							core::LiteralPtr lit;
							if (argument->getValue() == "0") {
								lit = builder.literal(threadGroup->getType(),"CLK_LOCAL_MEM_FENCE");
							} else if (argument->getValue() == "1") {
								lit = builder.literal(threadGroup->getType(),"CLK_GLOBAL_MEM_FENCE");
							}
							if (lit) {
								return builder.callExpr(call->getType(), basic.getBarrier(), toVector<core::ExpressionPtr>(lit));
							}
						}
					}
				}

				return res;
			}


		};

		template<typename T>
		T& copyCName(T& target, const core::NodePtr& src) {
			if (src->hasAnnotation(annotations::c::CNameAnnotation::KEY)) {
				target->addAnnotation(src->getAnnotation(annotations::c::CNameAnnotation::KEY));
			}
			return target;
		}


		// --------------------------------------------------------------------------------------------------------------
		//
		// --------------------------------------------------------------------------------------------------------------

		class TypeWrapper : public core::transform::CachedNodeMapping {

			core::NodeManager& manager;
			const Extensions& extensions;

		public:

			TypeWrapper(core::NodeManager& manager) :
				manager(manager),  extensions(manager.getLangExtension<Extensions>()) {}

			const core::NodePtr resolveElement(const core::NodePtr& ptr) {

				core::ASTBuilder builder(manager);
				auto& basic = manager.getBasicGenerator();

				// perform conversion in post-order
				core::NodePtr res = ptr->substitute(manager, *this);

				// only interested in lambda expressions
				if (ptr->getNodeType() != core::NT_LambdaExpr) {
					return res;
				}

				// extract lambda
				const core::LambdaExprPtr& kernel = static_pointer_cast<const core::LambdaExpr>(res);

				if (!isKernel(kernel)) {
					return res;
				}


				// TODO:
				// 		- replace build ins
				//		- add address space modifier

				// replace some build-ins



				// create kernel function
				core::StatementPtr core = getKernelCore(kernel);

				// - get_num_groups => first declaration
				// - get_*_size => last two parameters

				LOG(INFO) << "Core Before: " << core::printer::PrettyPrinter(core);

				// ------------------ Update variable names within kernel core -------------------

				// exchange variables within core
				VariableMap&& map = mapBodyVars(kernel);

				AddressSpaceMap varMap = getAddressSpaces(kernel);

				// Separate variable map into local variable declarations and parameters
				VariableMap localVars;
				utils::map::PointerMap<core::VariablePtr, core::ExpressionPtr> parameters;
				for_each(map, [&](const VariableMap::value_type& cur) {
					if (cur.second->getNodeType() == core::NT_Variable) {
						core::VariablePtr var = static_pointer_cast<const core::Variable>(cur.second);

						// copy C-name annotation
						copyCName(var, cur.first);

						core::ExpressionPtr substitute = cur.second;
						auto pos = varMap.find(var);
						if (pos != varMap.end()) {
							substitute = builder.variable(extensions.getType(pos->second, var->getType()), var->getId());
							substitute = extensions.unWrapExpr(pos->second, substitute);
						}

						parameters.insert(std::make_pair(cur.first, substitute));
					} else {

						core::VariablePtr var = builder.variable(extensions.getType(AddressSpace::LOCAL, cur.first->getType()), cur.first->getId());
						core::ExpressionPtr value = extensions.wrapExpr(AddressSpace::LOCAL, cur.second);
						localVars.insert(std::make_pair(var, value));

						parameters.insert(std::make_pair(cur.first, extensions.unWrapExpr(AddressSpace::LOCAL, var)));
					}
				});

				// replace parameters ...
				core = core::transform::replaceVarsGen(manager, core, parameters);

				// add locals ...
				if (!localVars.empty()) {
					vector<core::StatementPtr> stmts;
					for_each(localVars, [&](const VariableMap::value_type& cur) {
						stmts.push_back(builder.declarationStmt(cur.first, cur.second));
					});
					stmts.push_back(core);
					core = builder.compoundStmt(stmts);
				}


				// ------------------------- Replace build-in literals ---------------------------

				core::VariablePtr globalSizeVar = *(kernel->getParameterList().end() - 2);
				core::VariablePtr localSizeVar = *(kernel->getParameterList().end() - 1);
				core::VariablePtr numGroupVar =
						static_pointer_cast<const core::DeclarationStmt>(
						static_pointer_cast<const core::CompoundStmt>(kernel->getBody())->getStatements()[0])->getVariable();
				BuildInReplacer replacer(manager, globalSizeVar, localSizeVar, numGroupVar);
				core = static_pointer_cast<const core::Statement>(core->substitute(manager, replacer));


				LOG(INFO) << "Core After: " << core::printer::PrettyPrinter(core);

				// ------------------ Create resulting lambda expression -------------------

				// build parameter list
				vector<core::VariablePtr> params = vector<core::VariablePtr>(kernel->getParameterList().begin(), kernel->getParameterList().end()-2);

				for_each(params, [&](core::VariablePtr& cur) {
					core::VariablePtr res = builder.variable(extensions.getType(varMap[cur], cur->getType()), cur->getId());
					cur = copyCName(res, cur);
				});

				vector<core::TypePtr> paramTypes = ::transform(params, [](const core::VariablePtr& cur) { return cur->getType(); });

				core::FunctionTypePtr kernelType = builder.functionType(paramTypes, basic.getUnit());
				core::LambdaExprPtr newKernel = builder.lambdaExpr(kernelType, params, core);

				if (kernel->getLambda()->hasAnnotation(annotations::c::CNameAnnotation::KEY)) {
					auto name = kernel->getLambda()->getAnnotation(annotations::c::CNameAnnotation::KEY);
					LOG(INFO) << " Name is moved: " << name->getName();
					newKernel->getLambda()->addAnnotation(name);
					newKernel->addAnnotation(name);
				}


				res = builder.callExpr(kernelType, extensions.kernelWrapper, toVector<core::ExpressionPtr>(newKernel));

				LOG(INFO) << "New Kernel: " << core::printer::PrettyPrinter(res);

				LOG(INFO) << "Errors: " << core::check(newKernel, core::checks::getFullCheck());



				return res;
			}
		};


	}


	core::NodePtr OCLPreprocessor::process(core::NodeManager& manager, const core::NodePtr& code) {

		// TODO:
		// 		- find kernel lambdas, updated types

		// the converter does the magic
		TypeWrapper wrapper(manager);
		return wrapper.map(code);
	}


} // end namespace ocl_kernel
} // end namespace backend
} // end namespace insieme
