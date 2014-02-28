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

#include "insieme/backend/addons/static_variables.h"


#include "insieme/backend/converter.h"
#include "insieme/backend/type_manager.h"
#include "insieme/backend/function_manager.h"
#include "insieme/backend/operator_converter.h"

#include "insieme/backend/c_ast/c_ast_utils.h"
#include "insieme/backend/statement_converter.h"

#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/lang/static_vars.h"


namespace insieme {
namespace backend {
namespace addons {

	namespace {

		class StaticVarBackendExtension : public core::lang::Extension {

			/**
			 * Allow the node manager to create instances of this class.
			 */
			friend class core::NodeManager;

			/**
			 * Creates a new instance based on the given node manager.
			 */
			StaticVarBackendExtension(core::NodeManager& manager)
					: core::lang::Extension(manager) {}

		public:

			/**
			 * A literal masking the initialization of a static literal using constants.
			 */
			LANG_EXT_LITERAL(InitStaticConst, "InitStaticConst", "('a)->ref<'a>");

			/**
			 * A literal masking the initialization of a static literal using lazy expressions.
			 */
			LANG_EXT_LITERAL(InitStaticLazy, "InitStaticLazy", "(()=>'a)->ref<'a>");

		};



		OperatorConverterTable getStaticVariableOperatorTable(core::NodeManager& manager) {
			OperatorConverterTable res;

			const auto& ext  = manager.getLangExtension<core::lang::StaticVariableExtension>();
			const auto& ext2 = manager.getLangExtension<StaticVarBackendExtension>();

			#include "insieme/backend/operator_converter_begin.inc"

			res[ext.getCreateStatic()] 	= OP_CONVERTER({
				return nullptr;			// no instruction required at this point
			});


			// -------------------- Constant initialization -----------------------

			res[ext.getInitStaticConst()] 	= OP_CONVERTER({

				// we have to build a new function for this init call containing a static variable
				//
				//		A* initXY(lazy) {
				//			static A a = lazy();
				//			return &a;
				//		}
				//

				// ... and since the construction of a function and all its dependencies is anoying
				// in the C-AST we create a new IR function including an artificial construct
				// InitStatic that only covers the magic part ...

				auto fun = call->getFunctionExpr().as<core::LambdaExprPtr>();
				auto retType = fun->getFunctionType()->getReturnType();

				auto& mgr = NODE_MANAGER;
				auto& ext = mgr.getLangExtension<StaticVarBackendExtension>();
				core::IRBuilder builder(mgr);

				auto init = builder.callExpr(ext.getInitStaticConst(), call[1]);
				auto lambda = builder.lambdaExpr(call->getType(), init, core::VariableList());

				// this function call is equivalent to a call to the new artifical lambda
				return CONVERT_EXPR(builder.callExpr(lambda, core::ExpressionList()));
			});

			res[ext2.getInitStaticConst()] = OP_CONVERTER({

				// a call to this is translated to the following:
				//
				//			static A a = lazy();
				//			return &a;
				//
				// where A is the type of the resulting object.

				auto A = call->getType().as<core::RefTypePtr>()->getElementType();

				// get meta-type
				auto& infoA = GET_TYPE_INFO(A);

				// create variable
				auto var = C_NODE_MANAGER->create<c_ast::Variable>(
						infoA.lValueType,
						C_NODE_MANAGER->create("a")
				);

				// create init value
				auto& mgr = NODE_MANAGER;
				core::IRBuilder builder(mgr);
				auto init = CONVERT_EXPR(call[0]);

				// built the static initialization
				auto decl = C_NODE_MANAGER->create<c_ast::VarDecl>(var, init);
				decl->isStatic = true;

				// build return statement
				auto ret = C_NODE_MANAGER->create<c_ast::Return>(c_ast::ref(var));

				// build compound
				auto comp = C_NODE_MANAGER->create<c_ast::Compound>(toVector<c_ast::NodePtr>(decl, ret));

				// done
				return C_NODE_MANAGER->create<c_ast::StmtExpr>(comp);
			});


			// ---------------- lazy initialization -------------------------

			res[ext.getInitStaticLazy()] 	= OP_CONVERTER({

				// we have to build a new function for this init call containing a static variable
				//
				//		A* initXY(lazy) {
				//			static A a = lazy();
				//			return &a;
				//		}
				//

				// ... and since the construction of a function and all its dependencies is anoying
				// in the C-AST we create a new IR function including an artificial construct
				// InitStatic that only covers the magic part ...

				auto fun = call->getFunctionExpr().as<core::LambdaExprPtr>();
				auto retType = fun->getFunctionType()->getReturnType();

				auto& mgr = NODE_MANAGER;
				auto& ext = mgr.getLangExtension<StaticVarBackendExtension>();
				core::IRBuilder builder(mgr);

				auto param = fun->getParameterList()[1];

				auto init = builder.callExpr(ext.getInitStaticLazy(), param);
				auto lambda = builder.lambdaExpr(retType, init, toVector(param));

				// this function call is equivalent to a call to the new artifical lambda
				return CONVERT_EXPR(builder.callExpr(lambda, call[1]));
			});

			res[ext2.getInitStaticLazy()] = OP_CONVERTER({

				// a call to this is translated to the following:
				//
				//			static A a = lazy();
				//			return &a;
				//
				// where A is the type of the resulting object.

				auto A = call->getType().as<core::RefTypePtr>()->getElementType();

				// get meta-type
				auto& infoA = GET_TYPE_INFO(A);

				// create variable
				auto var = C_NODE_MANAGER->create<c_ast::Variable>(
						infoA.lValueType,
						C_NODE_MANAGER->create("a")
				);

				// create init value
				auto& mgr = NODE_MANAGER;
				core::IRBuilder builder(mgr);
				auto init = CONVERT_EXPR(builder.callExpr(call[0], toVector<core::ExpressionPtr>()));

				// built the static initialization
				auto decl = C_NODE_MANAGER->create<c_ast::VarDecl>(var, init);
				decl->isStatic = true;

				// build return statement
				auto ret = C_NODE_MANAGER->create<c_ast::Return>(c_ast::ref(var));

				// build compound
				auto comp = C_NODE_MANAGER->create<c_ast::Compound>(toVector<c_ast::NodePtr>(decl, ret));

				// done
				return C_NODE_MANAGER->create<c_ast::StmtExpr>(comp);
			});

			#include "insieme/backend/operator_converter_end.inc"

			return res;
		}

	} // namespace

	void StaticVariables::installOn(Converter& converter) const {
		
		// register additional operators
		converter.getFunctionManager().getOperatorConverterTable().insertAll(getStaticVariableOperatorTable(converter.getNodeManager()));

	}

} // end namespace addons
} // end namespace backend
} // end namespace insieme
