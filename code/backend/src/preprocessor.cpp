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

#include "insieme/backend/preprocessor.h"

#include "insieme/backend/ir_extensions.h"

#include "insieme/core/ir_node.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_address.h"

#include "insieme/core/type_utils.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/type_variable_deduction.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/transform/node_mapper_utils.h"

#include "insieme/utils/logging.h"

namespace insieme {
namespace backend {


	PreProcessorPtr getBasicPreProcessorSequence() {
		return makePreProcessor<PreProcessingSequence>(
				makePreProcessor<InlinePointwise>(),
				makePreProcessor<GenericLambdaInstantiator>(),
				makePreProcessor<IfThenElseInlining>(),
				makePreProcessor<RestoreGlobals>(),
				makePreProcessor<InitZeroSubstitution>(),
				makePreProcessor<MakeVectorArrayCastsExplicit>()
		);
	}


	core::NodePtr PreProcessingSequence::process(core::NodeManager& manager, const core::NodePtr& code) {

		// start by copying code to given target manager
		core::NodePtr res = manager.get(code);

		// apply sequence of pre-processing steps
		for_each(preprocessor, [&](const PreProcessorPtr& cur) {
			res = cur->process(manager, res);
		});

		// return final result
		return res;
	}


	// ------- concrete pre-processing step implementations ---------

	core::NodePtr NoPreProcessing::process(core::NodeManager& manager, const core::NodePtr& code) {
		// just copy to target manager
		return manager.get(code);
	}



	// --------------------------------------------------------------------------------------------------------------
	//      ITE to lazy-ITE Convertion
	// --------------------------------------------------------------------------------------------------------------

	class ITEConverter : public core::transform::CachedNodeMapping {

		const core::LiteralPtr ITE;
		const IRExtensions& extensions;

	public:

		ITEConverter(core::NodeManager& manager) :
			ITE(manager.getLangBasic().getIfThenElse()),  extensions(manager.getLangExtension<IRExtensions>()) {};

		/**
		 * Searches all ITE calls and replaces them by lazyITE calls. It also is aiming on inlining
		 * the resulting call.
		 */
		const core::NodePtr resolveElement(const core::NodePtr& ptr) {
			// do not touch types ...
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}

			// apply recursively - bottom up
			core::NodePtr res = ptr->substitute(ptr->getNodeManager(), *this);

			// check current node
			if (!core::analysis::isCallOf(res, ITE)) {
				// no change required
				return res;
			}

			// exchange ITE call
			core::IRBuilder builder(res->getNodeManager());
			core::CallExprPtr call = static_pointer_cast<const core::CallExpr>(res);
			const vector<core::ExpressionPtr>& args = call->getArguments();
			res = builder.callExpr(extensions.lazyITE, args[0], evalLazy(args[1]), evalLazy(args[2]));

			// migrate annotations
			core::transform::utils::migrateAnnotations(ptr, res);

			// done
			return res;
		}

	private:

		/**
		 * A utility method for inlining the evaluation of lazy functions.
		 */
		core::ExpressionPtr evalLazy(const core::ExpressionPtr& lazy) {

			core::NodeManager& manager = lazy->getNodeManager();

			core::FunctionTypePtr funType = core::dynamic_pointer_cast<const core::FunctionType>(lazy->getType());
			assert(funType && "Illegal lazy type!");

			// form call expression
			core::CallExprPtr call = core::CallExpr::get(manager, funType->getReturnType(), lazy, toVector<core::ExpressionPtr>());
			return core::transform::tryInlineToExpr(manager, call);
		}
	};



	core::NodePtr IfThenElseInlining::process(core::NodeManager& manager, const core::NodePtr& code) {
		// the converter does the magic
		ITEConverter converter(manager);
		return converter.map(code);
	}



	// --------------------------------------------------------------------------------------------------------------
	//      PreProcessor InitZero convert => replaces call by actual value
	// --------------------------------------------------------------------------------------------------------------

	class InitZeroReplacer : public core::transform::CachedNodeMapping {

		const core::LiteralPtr initZero;
		core::NodeManager& manager;
		core::IRBuilder builder;
		const core::lang::BasicGenerator& basic;

	public:

		InitZeroReplacer(core::NodeManager& manager) :
			initZero(manager.getLangBasic().getInitZero()), manager(manager), builder(manager), basic(manager.getLangBasic()) {};

		/**
		 * Searches all ITE calls and replaces them by lazyITE calls. It also is aiming on inlining
		 * the resulting call.
		 */
		const core::NodePtr resolveElement(const core::NodePtr& ptr) {
			// do not touch types ...
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}

			// apply recursively - bottom up
			core::NodePtr res = ptr->substitute(ptr->getNodeManager(), *this);

			// check current node
			if (core::analysis::isCallOf(res, initZero)) {
				// replace with equivalent zero value
				res = builder.getZero(static_pointer_cast<const core::Expression>(res)->getType());
			}

			// no change required
			return res;
		}

	};


	core::NodePtr InitZeroSubstitution::process(core::NodeManager& manager, const core::NodePtr& code) {
		// the converter does the magic
		InitZeroReplacer converter(manager);
		return converter.map(code);
	}



	// --------------------------------------------------------------------------------------------------------------
	//      PreProcessor GenericLambdaInstantiator => instantiates generic lambda implementations
	// --------------------------------------------------------------------------------------------------------------

	class LambdaInstantiater : public core::transform::CachedNodeMapping {

		core::NodeManager& manager;

	public:

		LambdaInstantiater(core::NodeManager& manager) : manager(manager) {};

		const core::NodePtr resolveElement(const core::NodePtr& ptr) {

			// check types => abort
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}


			// look for call expressions
			if (ptr->getNodeType() == core::NT_CallExpr) {
				// extract the call
				core::CallExprPtr call = static_pointer_cast<const core::CallExpr>(ptr);

				// only care about lambdas
				core::ExpressionPtr fun = call->getFunctionExpr();
				if (fun->getNodeType() == core::NT_LambdaExpr) {

					// convert to lambda
					core::LambdaExprPtr lambda = static_pointer_cast<const core::LambdaExpr>(fun);

					// check whether the lambda is generic
					if (core::isGeneric(fun->getType())) {

						// compute substitutions
						core::SubstitutionOpt&& map = core::analysis::getTypeVariableInstantiation(manager, call);

						// instantiate type variables according to map
						lambda = core::transform::instantiate(manager, lambda, map);

						// create new call node
						core::ExpressionList arguments;
						::transform(call->getArguments(), std::back_inserter(arguments), [&](const core::ExpressionPtr& cur) {
							return static_pointer_cast<const core::Expression>(this->mapElement(0, cur));
						});

						// produce new call expression
						return core::CallExpr::get(manager, call->getType(), lambda, arguments);

					}
				}
			}

			// decent recursively
			return ptr->substitute(manager, *this);
		}

	};

	core::NodePtr GenericLambdaInstantiator::process(core::NodeManager& manager, const core::NodePtr& code) {
		// the converter does the magic
		LambdaInstantiater converter(manager);
		return converter.map(code);
	}


	// --------------------------------------------------------------------------------------------------------------
	//      PreProcessor InlinePointwise => replaces invocations of pointwise operators with in-lined code
	// --------------------------------------------------------------------------------------------------------------

	class PointwiseReplacer : public core::transform::CachedNodeMapping {

		core::NodeManager& manager;
		const core::lang::BasicGenerator& basic;

	public:

		PointwiseReplacer(core::NodeManager& manager) : manager(manager), basic(manager.getLangBasic()) {};

		const core::NodePtr resolveElement(const core::NodePtr& ptr) {

			// check types => abort
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}

			// look for call expressions
			if (ptr->getNodeType() == core::NT_CallExpr) {
				// extract the call
				core::CallExprPtr call = static_pointer_cast<const core::CallExpr>(ptr);

				// only care about calls to pointwise operations
				if (core::analysis::isCallOf(call->getFunctionExpr(), basic.getVectorPointwise())) {

					// get argument and result types!
					assert(call->getType()->getNodeType() == core::NT_VectorType && "Result should be a vector!");
					assert(call->getArgument(0)->getType()->getNodeType() == core::NT_VectorType && "Argument should be a vector!");

					core::VectorTypePtr argType = static_pointer_cast<const core::VectorType>(call->getArgument(0)->getType());
					core::VectorTypePtr resType = static_pointer_cast<const core::VectorType>(call->getType());

					// extract generic parameter types
					core::TypePtr in = argType->getElementType();
					core::TypePtr out = resType->getElementType();

					assert(resType->getSize()->getNodeType() == core::NT_ConcreteIntTypeParam && "Result should be of fixed size!");
					core::ConcreteIntTypeParamPtr size = static_pointer_cast<const core::ConcreteIntTypeParam>(resType->getSize());

					// extract operator
					core::ExpressionPtr op = static_pointer_cast<const core::CallExpr>(call->getFunctionExpr())->getArgument(0);

					// create new lambda, realizing the point-wise operation
					core::IRBuilder builder(manager);

					core::FunctionTypePtr funType = builder.functionType(toVector<core::TypePtr>(argType, argType), resType);

					core::VariablePtr v1 = builder.variable(argType);
					core::VariablePtr v2 = builder.variable(argType);
					core::VariablePtr res = builder.variable(resType);

					// create vector init expression
					vector<core::ExpressionPtr> fields;

					// unroll the pointwise operation
					core::TypePtr unitType = basic.getUnit();
					core::TypePtr longType = basic.getUInt8();
					core::ExpressionPtr vectorSubscript = basic.getVectorSubscript();
					for(std::size_t i=0; i<size->getValue(); i++) {
						core::LiteralPtr index = builder.literal(longType, boost::lexical_cast<std::string>(i));

						core::ExpressionPtr a = builder.callExpr(in, vectorSubscript, v1, index);
						core::ExpressionPtr b = builder.callExpr(in, vectorSubscript, v2, index);

						fields.push_back(builder.callExpr(out, op, a, b));
					}

					// return result
					core::StatementPtr body = builder.returnStmt(builder.vectorExpr(resType, fields));

					// construct substitute ...
					core::LambdaExprPtr substitute = builder.lambdaExpr(funType, toVector(v1,v2), body);
					return builder.callExpr(resType, substitute, call->getArguments());
				}
			}

			// decent recursively
			return ptr->substitute(manager, *this);
		}

	};

	core::NodePtr InlinePointwise::process(core::NodeManager& manager, const core::NodePtr& code) {
		// the converter does the magic
		PointwiseReplacer converter(manager);
		return converter.map(code);
	}


	// --------------------------------------------------------------------------------------------------------------
	//      Restore Globals
	// --------------------------------------------------------------------------------------------------------------

	bool isZero(const core::ExpressionPtr& value) {

		const core::lang::BasicGenerator& basic = value->getNodeManager().getLangBasic();

		// if initialization is zero ...
		if (core::analysis::isCallOf(value, basic.getInitZero())) {
			// no initialization required
			return true;
		}

		// ... or a zero literal ..
		if (value->getNodeType() == core::NT_Literal) {
			const string& strValue = static_pointer_cast<const core::Literal>(value)->getStringValue();
			if (strValue == "0" || strValue == "0.0") {
				return true;
			}
		}

		// ... or a call to getNull(...)
		if (core::analysis::isCallOf(value, basic.getGetNull())) {
			return true;
		}

		// ... or a vector initialization with a zero value
		if (core::analysis::isCallOf(value, basic.getVectorInitUniform())) {
			return isZero(core::analysis::getArgument(value, 0));
		}

		// TODO: remove this when frontend is fixed!!
		// => compensate for silly stuff like var(*getNull())
		if (core::analysis::isCallOf(value, basic.getRefVar())) {
			core::ExpressionPtr arg = core::analysis::getArgument(value, 0);
			if (core::analysis::isCallOf(arg, basic.getRefDeref())) {
				return isZero(core::analysis::getArgument(arg, 0));
			}
		}

		// otherwise, it is not zero
		return false;
	}

	namespace {


		struct GlobalDeclarationCollector : public core::IRVisitor<bool, core::Address> {
			vector<core::DeclarationStmtAddress> decls;

			// do not visit types
			GlobalDeclarationCollector() : IRVisitor<bool, core::Address>(false) {}

			bool visitNode(const core::NodeAddress& node) { return true; }	// does not need to decent deeper

			bool visitDeclarationStmt(const core::DeclarationStmtAddress& cur) {
				core::DeclarationStmtPtr decl = cur.getAddressedNode();

				// check the type
				core::TypePtr type = decl->getVariable()->getType();

				// check for references
				if (type->getNodeType() != core::NT_RefType) {
					return true;   // not a global struct
				}

				type = static_pointer_cast<core::RefTypePtr>(type)->getElementType();

				// the element type has to be a struct type
				if (type->getNodeType() != core::NT_StructType) {
					return true;    // also, not a global
				}


				// check initalization
				auto& basic = decl->getNodeManager().getLangBasic();
				core::ExpressionPtr init = decl->getInitialization();
				if (!(core::analysis::isCallOf(init, basic.getRefNew()) || core::analysis::isCallOf(init, basic.getRefVar()))) {
					return true; 	// again, not a global
				}

				init = core::analysis::getArgument(init, 0);

				// check whether the initialization is based on a struct expression
				if (init->getNodeType() != core::NT_StructExpr) {
					return true; 	// guess what, not a global!
				}

				// well, this is a global
				decls.push_back(cur);
				return true;
			}

			bool visitCompoundStmt(const core::CompoundStmtAddress& cmp) {
				return false; // keep descending into those!
			}

		};

		vector<core::DeclarationStmtAddress> getGlobalDeclarations(const core::CompoundStmtPtr& mainBody) {
			GlobalDeclarationCollector collector;
			core::visitDepthFirstPrunable(core::NodeAddress(mainBody), collector);
			return collector.decls;
		}

	}


	core::NodePtr RestoreGlobals::process(core::NodeManager& manager, const core::NodePtr& code) {

		// check for the program - only works on the global level
		if (code->getNodeType() != core::NT_Program) {
			return code;
		}

		// check whether it is a main program ...
		core::NodeAddress root(code);
		const core::ProgramAddress& program = core::static_address_cast<const core::Program>(root);
		if (!(program->getEntryPoints().size() == static_cast<std::size_t>(1))) {
			return code;
		}

		// extract body of main
		const core::ExpressionAddress& mainExpr = program->getEntryPoints()[0];
		if (mainExpr->getNodeType() != core::NT_LambdaExpr) {
			return code;
		}
		const core::LambdaExprAddress& main = core::static_address_cast<const core::LambdaExpr>(mainExpr);
		const core::StatementAddress& bodyStmt = main->getBody();
		if (bodyStmt->getNodeType() != core::NT_CompoundStmt) {
			return code;
		}
		core::CompoundStmtAddress body = core::static_address_cast<const core::CompoundStmt>(bodyStmt);


		// search for global structs
		vector<core::DeclarationStmtAddress> globals = getGlobalDeclarations(body.getAddressedNode());
		if (globals.empty()) {
			return code;
		}

		// create global_literal
		utils::map::PointerMap<core::VariablePtr, core::ExpressionPtr> replacements;

		int i = 0;
		for_each(globals, [&](const core::DeclarationStmtAddress& cur) {
			core::DeclarationStmtPtr decl = cur.getAddressedNode();
			replacements[decl->getVariable()] = core::Literal::get(manager, decl->getVariable()->getType(), IRExtensions::GLOBAL_ID + toString(i++));
		});


		// replace declarations with pure initializations
		core::IRBuilder builder(manager);
		const core::lang::BasicGenerator& basic = manager.getLangBasic();
		const IRExtensions& extensions = manager.getLangExtension<IRExtensions>();
		core::TypePtr unit = basic.getUnit();

		// get some functions used for the initialization
		core::ExpressionPtr initUniform = basic.getVectorInitUniform();
		core::ExpressionPtr initZero = basic.getInitZero();

		// a property used to determine whether an initial value is undefined
		auto isUndefined = [&](const core::NodePtr& cur) {
			return   core::analysis::isCallOf(cur, basic.getUndefined()) ||
					(core::analysis::isCallOf(cur, basic.getRefVar()) &&
					 core::analysis::isCallOf(core::analysis::getArgument(cur, 0), basic.getUndefined())) ||
				    (core::analysis::isCallOf(cur, basic.getRefNew()) &&
					 core::analysis::isCallOf(core::analysis::getArgument(cur, 0), basic.getUndefined()));
		};

		// create an initializing block for each global value
		i = 0;
		std::map<core::NodeAddress, core::NodePtr> initializations;
		for_each(globals, [&](const core::DeclarationStmtAddress& cur) {
			core::DeclarationStmtPtr decl = cur.getAddressedNode();

			vector<core::StatementPtr> initExpressions;

			// start with register global call (initializes code fragment and adds dependencies)
			core::ExpressionPtr nameLiteral = builder.getIdentifierLiteral(IRExtensions::GLOBAL_ID + toString(i++));
			core::ExpressionPtr typeLiteral = builder.getTypeLiteral(decl->getVariable()->getType());
			core::StatementPtr registerGlobal = builder.callExpr(unit, extensions.registerGlobal, nameLiteral, typeLiteral);
			initExpressions.push_back(registerGlobal);

			// initialize remaining fields of global struct
			core::ExpressionPtr initValue = core::analysis::getArgument(decl->getInitialization(), 0);
			assert(initValue->getNodeType() == core::NT_StructExpr);
			core::StructExprPtr initStruct = static_pointer_cast<const core::StructExpr>(initValue);

			for_each(initStruct->getMembers()->getElements(), [&](const core::NamedValuePtr& member) {

				// ignore zero values => default initialization
				if (isZero(member->getValue())) {
					return;
				}

				// ignore initalizations of undefined functions
				if (isUndefined(member->getValue())) {
					return;
				}

				core::ExpressionPtr access = builder.refMember(replacements[decl->getVariable()], member->getName());
				core::ExpressionPtr assign = builder.assign(access, member->getValue());
				initExpressions.push_back(assign);
			});

			// aggregate initializations to compound stmt
			initializations[cur] = builder.compoundStmt(initExpressions);
		});

		// create new body
		core::StatementPtr newBody = static_pointer_cast<core::StatementPtr>(core::transform::replaceAll(manager, initializations));

		// propagate new global variables
		for_each(replacements, [&](const std::pair<core::VariablePtr, core::ExpressionPtr>& cur) {
			newBody = core::transform::fixVariable(manager, newBody, cur.first, cur.second);
		});

		// replace old and new body
		return core::transform::replaceNode(manager, body, newBody);
	}




	// --------------------------------------------------------------------------------------------------------------
	//      Adding explicit Vector to Array casts
	// --------------------------------------------------------------------------------------------------------------

	class VectorToArrayConverter : public core::transform::CachedNodeMapping {

		core::NodeManager& manager;

	public:

		VectorToArrayConverter(core::NodeManager& manager) : manager(manager) {}

		const core::NodePtr resolveElement(const core::NodePtr& ptr) {

			// do not touch types ...
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}

			// apply recursively - bottom up
			core::NodePtr res = ptr->substitute(ptr->getNodeManager(), *this);

			// handle calls
			if (ptr->getNodeType() == core::NT_CallExpr) {
				res = handleCallExpr(core::static_pointer_cast<const core::CallExpr>(res));
			}

			// handle declarations
			if (ptr->getNodeType() == core::NT_DeclarationStmt) {
				res = handleDeclarationStmt(core::static_pointer_cast<const core::DeclarationStmt>(res));
			}

			// handle rest
			return res;
		}

	private:

		/**
		 * This method replaces vector arguments with vector2array conversions whenever necessary.
		 */
		core::CallExprPtr handleCallExpr(core::CallExprPtr call) {

			// extract node manager
			core::IRBuilder builder(manager);
			const core::lang::BasicGenerator& basic = builder.getLangBasic();


			// check whether there is a argument which is a vector but the parameter is not
			const core::TypePtr& type = call->getFunctionExpr()->getType();
			assert(type->getNodeType() == core::NT_FunctionType && "Function should be of a function type!");
			const core::FunctionTypePtr& funType = core::static_pointer_cast<const core::FunctionType>(type);

			const core::TypeList& paramTypes = funType->getParameterTypes()->getElements();
			const core::ExpressionList& args = call->getArguments();

			// check number of arguments
			if (paramTypes.size() != args.size()) {
				// => invalid call, don't touch this
				return call;
			}

			bool conversionRequired = false;
			std::size_t size = args.size();
			for (std::size_t i = 0; !conversionRequired && i < size; i++) {
				conversionRequired = conversionRequired || (args[i]->getType()->getNodeType() == core::NT_VectorType && paramTypes[i]->getNodeType() != core::NT_VectorType);
			}

			// check whether a vector / non-vector argument/parameter pair has been found
			if (!conversionRequired) {
				// => no deduction required
				return call;
			}

			// derive type variable instantiation
			auto instantiation = core::analysis::getTypeVariableInstantiation(manager, call);
			if (!instantiation) {
				// => invalid call, don't touch this
				return call;
			}

			// obtain argument list
			vector<core::TypePtr> argTypes;
			::transform(args, std::back_inserter(argTypes), [](const core::ExpressionPtr& cur) { return cur->getType(); });

			// apply match on parameter list
			core::TypeList newParamTypes = paramTypes;
			for (std::size_t i=0; i<newParamTypes.size(); i++) {
				newParamTypes[i] = instantiation->applyTo(manager, newParamTypes[i]);
			}

			// generate new argument list
			bool changed = false;
			core::ExpressionList newArgs = call->getArguments();
			for (unsigned i=0; i<size; i++) {

				// ignore identical types
				if (*newParamTypes[i] == *argTypes[i]) {
					continue;
				}

				core::TypePtr argType = argTypes[i];
				core::TypePtr paramType = newParamTypes[i];

				// strip references
				bool ref = false;
				if (argType->getNodeType() == core::NT_RefType && paramType->getNodeType() == core::NT_RefType) {
					ref = true;
					argType = core::static_pointer_cast<const core::RefType>(argType)->getElementType();
					paramType = core::static_pointer_cast<const core::RefType>(paramType)->getElementType();
				}

				// handle vector->array
				if (argType->getNodeType() == core::NT_VectorType && paramType->getNodeType() == core::NT_ArrayType) {
					// conversion needed
					newArgs[i] = builder.callExpr((ref)?basic.getRefVectorToRefArray():basic.getVectorToArray(), newArgs[i]);
					changed = true;
				}
			}
			if (!changed) {
				// return original call
				return call;
			}

			// exchange parameters and done
			return core::CallExpr::get(manager, instantiation->applyTo(call->getType()), call->getFunctionExpr(), newArgs);
		}

		/**
		 * This method replaces vector initialization values with vector2array conversions whenever necessary.
		 */
		core::DeclarationStmtPtr handleDeclarationStmt(core::DeclarationStmtPtr declaration) {

			// only important for array types
			core::TypePtr type = declaration->getVariable()->getType();
			if (type->getNodeType() != core::NT_ArrayType) {
				return declaration;
			}

			// get initialization value
			type = declaration->getInitialization()->getType();
			if (type->getNodeType() != core::NT_VectorType) {
				return declaration;
			}

			// extract some values from the declaration statement
			core::NodeManager& manager = declaration->getNodeManager();
			const core::VariablePtr& var = declaration->getVariable();
			const core::ExpressionPtr& oldInit = declaration->getInitialization();

			// construct a new init statement
			core::ExpressionPtr newInit = core::CallExpr::get(manager, var->getType(), manager.getLangBasic().getVectorToArray(), toVector(oldInit));
			return core::DeclarationStmt::get(manager, declaration->getVariable(), newInit);
		}
	};

	core::NodePtr MakeVectorArrayCastsExplicit::process(core::NodeManager& manager, const core::NodePtr& code) {
		// the converter does the magic
		VectorToArrayConverter converter(manager);
		return converter.map(code);
	}

} // end namespace backend
} // end namespace insieme
