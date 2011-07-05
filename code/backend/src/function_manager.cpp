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

#include "insieme/backend/function_manager.h"

#include <set>
#include <functional>

#include "insieme/backend/converter.h"
#include "insieme/backend/type_manager.h"
#include "insieme/backend/statement_converter.h"
#include "insieme/backend/name_manager.h"
#include "insieme/backend/variable_manager.h"

#include "insieme/backend/c_ast/c_ast_utils.h"

#include "insieme/core/expressions.h"
#include "insieme/core/analysis/ir_utils.h"

#include "insieme/utils/map_utils.h"
#include "insieme/utils/logging.h"

namespace insieme {
namespace backend {


	namespace detail {

		template<typename Type> struct info_trait;
		template<> struct info_trait<core::Literal> { typedef FunctionInfo type; };
		template<> struct info_trait<core::LambdaExpr> { typedef LambdaInfo type; };
		template<> struct info_trait<core::BindExpr> { typedef BindInfo type; };

		struct FunctionCodeInfo {
			c_ast::FunctionPtr function;
			std::set<c_ast::CodeFragmentPtr> prototypeDependencies;
			std::set<c_ast::CodeFragmentPtr> definitionDependencies;
		};

		class FunctionInfoStore {

			const Converter& converter;

			utils::map::PointerMap<core::ExpressionPtr, ElementInfo*> funInfos;

		public:

			FunctionInfoStore(const Converter& converter) : converter(converter) {}

			~FunctionInfoStore() {
				// free all stored type information instances
				for_each(funInfos, [](const std::pair<core::ExpressionPtr, ElementInfo*>& cur) {
					delete cur.second;
				});
			}

			template<
				typename T,
				typename result_type = typename info_trait<typename boost::remove_const<typename T::element_type>::type>::type*
			>
			result_type resolve(const T& expression) {
				// lookup information using internal mechanism and cast statically
				ElementInfo* info = resolveInternal(expression);
				assert(dynamic_cast<result_type>(info));
				return static_cast<result_type>(info);
			}

		protected:

			ElementInfo* resolveInternal(const core::ExpressionPtr& expression);

			ElementInfo* resolveLiteral(const core::LiteralPtr& literal);
			ElementInfo* resolveBind(const core::BindExprPtr& bind);
			ElementInfo* resolveLambda(const core::LambdaExprPtr& lambda);
			void resolveLambdaDefinition(const core::LambdaDefinitionPtr& lambdaDefinition);

			// -------- utilities -----------

			FunctionCodeInfo resolveFunction(const c_ast::IdentifierPtr name,
					const core::FunctionTypePtr& funType, const core::LambdaPtr& lambda, bool external);

			std::pair<c_ast::IdentifierPtr, c_ast::CodeFragmentPtr>
			resolveLambdaWrapper(const c_ast::FunctionPtr& function, const core::FunctionTypePtr& funType);

		};

	}


	FunctionManager::FunctionManager(const Converter& converter)
		: converter(converter), store(new detail::FunctionInfoStore(converter)),
		  operatorTable(getBasicOperatorTable(converter.getNodeManager().getBasicGenerator())) {}

	FunctionManager::FunctionManager(const Converter& converter, const OperatorConverterTable& operatorTable)
		: converter(converter), store(new detail::FunctionInfoStore(converter)), operatorTable(operatorTable) {}

	FunctionManager::~FunctionManager() {
		delete store;
	}

	const FunctionInfo& FunctionManager::getInfo(const core::LiteralPtr& literal) {
		return *(store->resolve(literal));
	}

	const LambdaInfo& FunctionManager::getInfo(const core::LambdaExprPtr& lambda) {
		return *(store->resolve(lambda));
	}

	const BindInfo& FunctionManager::getInfo(const core::BindExprPtr& bind) {
		return *(store->resolve(bind));
	}

	namespace {

		const c_ast::ExpressionPtr getValueInternal(ConversionContext& context, const core::ExpressionPtr& expr, const FunctionInfo& info) {

			// The value of a literal is created by instantiating a closure covering the literal
			// This looks as follows:
			//		<closure_name>_ctr((closure_name*)alloca(sizeof(closure_name)), &literal_wrapper)

			// collect information regarding result
			const Converter& converter = context.getConverter();
			const FunctionTypeInfo& typeInfo = converter.getTypeManager().getTypeInfo(static_pointer_cast<const core::FunctionType>(expr->getType()));

			c_ast::TypePtr closureType = typeInfo.rValueType;

			// add dependencies
			c_ast::DependencySet& dependencies = context.getDependencies();
			dependencies.insert(typeInfo.definition);
			dependencies.insert(typeInfo.constructor);
			dependencies.insert(info.lambdaWrapper);

			// build the value
			auto manager = context.getCNodeManager();
			c_ast::ExpressionPtr alloc = c_ast::cast(c_ast::ptr(closureType),
					c_ast::call(manager->create("alloca"), c_ast::unaryOp(c_ast::UnaryOperation::SizeOf, closureType)));
			return c_ast::call(typeInfo.constructorName, alloc, c_ast::ref(info.lambdaWrapperName));
		}


		void appendAsArguments(ConversionContext& context, c_ast::CallPtr& call, const vector<core::ExpressionPtr>& arguments, bool external) {

			// collect some manager references
			const Converter& converter = context.getConverter();
			const c_ast::SharedCNodeManager& manager = context.getCNodeManager();
			StmtConverter& stmtConverter = converter.getStmtConverter();
			TypeManager& typeManager = converter.getTypeManager();

			auto varlistPack = converter.getNodeManager().getBasicGenerator().getVarlistPack();

			// create a recursive lambda appending arguments to the caller (descent into varlist-pack calls)
			std::function<void(const core::ExpressionPtr& argument)> append;
			auto recLambda = [&](const core::ExpressionPtr& cur) {

				// test if current argument is a variable argument list
				if (core::analysis::isCallOf(cur, varlistPack)) {
					// inline arguments of varlist-pack call => append arguments directly
					const vector<core::ExpressionPtr>& packed = static_pointer_cast<const core::CallExpr>(cur)->getArguments();

					for_each(static_pointer_cast<const core::TupleExpr>(packed[0])->getExpressions(),
							[&](const core::ExpressionPtr& cur) {
								append(cur);
					});
					return;
				}

				// simply append the argument (externalize if necessary)
				c_ast::ExpressionPtr res = stmtConverter.convertExpression(context, cur);
				call->arguments.push_back((external)?typeManager.getTypeInfo(cur->getType()).externalize(manager, res):res);

			};
			append = recLambda;

			// invoke append for all arguments
			for_each(arguments, [&](const core::ExpressionPtr& cur) {
				append(cur);
			});
		}

	}

	const c_ast::NodePtr FunctionManager::getCall(const core::CallExprPtr& call, ConversionContext& context) {

		// extract target function
		core::ExpressionPtr fun = call->getFunctionExpr();

		// 1) see whether call is call to a known operator
		auto pos = operatorTable.find(fun);
		if (pos != operatorTable.end()) {
			// use operator converter
			return pos->second(context, call);
		}

		// 2) test whether target is a literal => external function, direct call
		if (fun->getNodeType() == core::NT_Literal) {
			// obtain literal information
			const FunctionInfo& info = getInfo(static_pointer_cast<const core::Literal>(fun));

			// produce call to external literal
			c_ast::CallPtr res = c_ast::call(info.function->name);
			appendAsArguments(context, res, call->getArguments(), true);

			// add dependencies
			context.getDependencies().insert(info.prototype);

			// return external function call
			return res;
		}


		// the generic fall-back solution:
		//		get function as a value and call it using the function-type's caller function

		core::FunctionTypePtr funType = static_pointer_cast<const core::FunctionType>(fun->getType());

		c_ast::ExpressionPtr value = getValue(call->getFunctionExpr(), context);

		const FunctionTypeInfo& typeInfo = converter.getTypeManager().getTypeInfo(funType);
		c_ast::CallPtr res = c_ast::call(typeInfo.callerName, value);
		appendAsArguments(context, res, call->getArguments(), false);

		// add dependencies
		context.getDependencies().insert(typeInfo.caller);

		return res;
	}



	const c_ast::ExpressionPtr FunctionManager::getValue(const core::ExpressionPtr& fun, ConversionContext& context) {
		auto manager = converter.getCNodeManager();

		// handle according to node type
		switch(fun->getNodeType()) {
		case core::NT_BindExpr: {
			return getValue(static_pointer_cast<const core::BindExpr>(fun), context);
		}
		case core::NT_Literal: {
			core::LiteralPtr literal = static_pointer_cast<const core::Literal>(fun);
			return getValueInternal(context, fun, getInfo(literal));
		}
		case core::NT_LambdaExpr: {
			core::LambdaExprPtr lambda = static_pointer_cast<const core::LambdaExpr>(fun);
			return getValueInternal(context, fun, getInfo(lambda));
		}
		case core::NT_Variable: {
			// variable is already representing a value
			c_ast::NodePtr res = converter.getStmtConverter().convert(context, fun);
			return static_pointer_cast<c_ast::Expression>(res);
		}
		default:
			assert(false && "Unexpected Node Type!");
			return c_ast::ExpressionPtr();
		}

	}

	const c_ast::ExpressionPtr FunctionManager::getValue(const core::BindExprPtr& bind, ConversionContext& context) {
		auto manager = converter.getCNodeManager();

		// create a value instance by initializing the bind closure using its constructor

		// collect some information
		const BindInfo& info = getInfo(bind);
		const FunctionTypeInfo& typeInfo = converter.getTypeManager().getTypeInfo(static_pointer_cast<const core::FunctionType>(bind->getType()));

		// add dependencies
		c_ast::DependencySet& dependencies = context.getDependencies();
		dependencies.insert(typeInfo.definition);
		dependencies.insert(typeInfo.constructor);
		dependencies.insert(info.definitions);


		// allocate memory for the bind expression
		c_ast::ExpressionPtr alloc = c_ast::cast(c_ast::ptr(info.closureType),
				c_ast::call(manager->create("alloca"), c_ast::unaryOp(c_ast::UnaryOperation::SizeOf, info.closureType)));

		// create nested closure
		c_ast::ExpressionPtr nested = getValue(bind->getCall()->getFunctionExpr(), context);

		//  create constructor call
		c_ast::CallPtr res = c_ast::call(info.constructorName, alloc, nested);

		// add captured expressions
		auto boundExpression = bind->getBoundExpressions();
		appendAsArguments(context, res, boundExpression, false);

		// done
		return res;
	}


	namespace detail {


		ElementInfo* FunctionInfoStore::resolveInternal(const core::ExpressionPtr& expression) {

			// lookup information within store
			auto pos = funInfos.find(expression);
			if (pos != funInfos.end()) {
				return pos->second;
			}

			// obtain function information
			ElementInfo* info;

			// not known yet => requires some resolution
			switch(expression->getNodeType()) {
			case core::NT_Literal:
				info = resolveLiteral(static_pointer_cast<const core::Literal>(expression)); break;
			case core::NT_LambdaExpr:
				info = resolveLambda(static_pointer_cast<const core::LambdaExpr>(expression)); break;
			case core::NT_BindExpr:
				info = resolveBind(static_pointer_cast<const core::BindExpr>(expression)); break;
			default:
				// this should not happen ...
				assert(false && "Unsupported node type encountered!");
				return new ElementInfo();
			}

			// store information
			funInfos.insert(std::make_pair(expression, info));

			// return pointer to obtained information
			return info;
		}

		ElementInfo* FunctionInfoStore::resolveLiteral(const core::LiteralPtr& literal) {

			assert(literal->getType()->getNodeType() == core::NT_FunctionType && "Only supporting literals with a function type!");

			// some preparation
			auto manager = converter.getCNodeManager();
			core::FunctionTypePtr funType = static_pointer_cast<const core::FunctionType>(literal->getType());
			FunctionInfo* res = new FunctionInfo();

			// ------------------------ resolve function ---------------------

			FunctionCodeInfo fun = resolveFunction(
					manager->create(literal->getValue()),
					funType, core::LambdaPtr(), true);

			res->function = fun.function;

			// ------------------------ add prototype -------------------------

			c_ast::FunctionPrototypePtr code = manager->create<c_ast::FunctionPrototype>(fun.function);
			res->prototype = c_ast::CCodeFragment::createNew(manager, code);
			res->prototype->addDependencies(fun.prototypeDependencies);

			// -------------------------- add lambda wrapper ---------------------------

			auto wrapper = resolveLambdaWrapper(fun.function, funType);
			res->lambdaWrapperName = wrapper.first;
			res->lambdaWrapper = wrapper.second;
			res->lambdaWrapper->addDependencies(fun.prototypeDependencies);
			res->lambdaWrapper->addDependency(res->prototype);

			// done
			return res;
		}

		ElementInfo* FunctionInfoStore::resolveBind(const core::BindExprPtr& bind) {

			// prepare some manager
			NameManager& nameManager = converter.getNameManager();
			TypeManager& typeManager = converter.getTypeManager();
			auto manager = converter.getCNodeManager();

			// create resulting data container
			BindInfo* res = new BindInfo();

			// set up names
			string name = nameManager.getName(bind, "bind");
			res->closureName = manager->create(name + "_closure");
			res->mapperName = manager->create(name + "_mapper");
			res->constructorName = manager->create(name + "_ctr");


			// create a map between expressions in the IR and parameter / captured variable names in C
			utils::map::PointerMap<core::ExpressionPtr, c_ast::VariablePtr> variableMap;

			// add parameters
			int paramCounter = 0;
			const vector<core::VariablePtr>& parameter = bind->getParameters();
			for_each(parameter, [&](const core::VariablePtr& cur) {
				variableMap[cur] = var(typeManager.getTypeInfo(cur->getType()).rValueType, format("p%d", ++paramCounter));
			});

			// add arguments of call
			int argumentCounter = 0;
			const vector<core::ExpressionPtr>& args = bind->getCall()->getArguments();
			for_each(args, [&](const core::ExpressionPtr& cur) {
				variableMap[cur] = var(typeManager.getTypeInfo(cur->getType()).rValueType, format("c%d", ++argumentCounter));
			});

			// extract captured variables
			vector<core::ExpressionPtr> captured = bind->getBoundExpressions();

			vector<c_ast::VariablePtr> varsCaptured;
			::transform(captured, std::back_inserter(varsCaptured), [&](const core::ExpressionPtr& cur){
				return variableMap[cur];
			});


			// ----------- define closure type ---------------

			// get function type of mapper
			core::FunctionTypePtr funType = static_pointer_cast<const core::FunctionType>(bind->getType());
			const FunctionTypeInfo& funInfo = typeManager.getTypeInfo(funType);

			// construct variable / struct entry pointing to the function to be called when processing the closure
			c_ast::FunctionTypePtr mapperType = manager->create<c_ast::FunctionType>(typeManager.getTypeInfo(funType->getReturnType()).rValueType);
			mapperType->parameterTypes.push_back(manager->create<c_ast::PointerType>(funInfo.rValueType));
			for_each(parameter, [&](const core::VariablePtr& var) {
				mapperType->parameterTypes.push_back(typeManager.getTypeInfo(var->getType()).rValueType);
			});
			c_ast::VariablePtr varCall = c_ast::var(manager->create<c_ast::PointerType>(mapperType), "call");

			// get generic type of nested closure
			core::FunctionTypePtr nestedFunType = static_pointer_cast<const core::FunctionType>(bind->getCall()->getFunctionExpr()->getType());
			const FunctionTypeInfo& nestedClosureInfo = typeManager.getTypeInfo(nestedFunType);
			c_ast::TypePtr nestedClosureType = manager->create<c_ast::PointerType>(nestedClosureInfo.rValueType);

			// define variable / struct entry pointing to the nested closure variable
			c_ast::VariablePtr varNested = c_ast::var(nestedClosureType, "nested");

			// create closure struct
			c_ast::StructTypePtr closureStruct = manager->create<c_ast::StructType>(manager->create());
			closureStruct->elements.push_back(varCall);
			closureStruct->elements.push_back(varNested);
			addAll(closureStruct->elements, varsCaptured);

			c_ast::NodePtr closure = manager->create<c_ast::TypeDefinition>(closureStruct, res->closureName);
			res->closureType = manager->create<c_ast::NamedType>(res->closureName);


			// --------------------------------- define mapper -------------------------------------
			c_ast::VariablePtr varClosure = var(manager->create<c_ast::PointerType>(res->closureType), "closure");

			c_ast::FunctionPtr mapper;
			{
				c_ast::TypePtr returnType = mapperType->returnType;

				vector<c_ast::VariablePtr> params;
				params.push_back(varClosure);
				::transform(bind->getParameters(), std::back_inserter(params), [&](const core::VariablePtr& cur) {
					return variableMap[cur];
				});

				auto fun = indirectAccess(indirectAccess(varClosure, "nested"), "call");

				c_ast::CallPtr call = manager->create<c_ast::Call>(fun);
				call->arguments.push_back(indirectAccess(varClosure, "nested"));

				for_each(args, [&](const core::ExpressionPtr& cur) {
					c_ast::VariablePtr var = variableMap[cur];
					c_ast::ExpressionPtr param = var;
					if (contains(captured, cur, equal_target<core::ExpressionPtr>())) {
						param = indirectAccess(varClosure, var->name);
					}
					call->arguments.push_back(param);
				});

				c_ast::StatementPtr body = call;
				if (!isVoid(returnType)) {
					body = manager->create<c_ast::Return>(call);
				}

				mapper = manager->create<c_ast::Function>(returnType, res->mapperName, params, body);
			}

			// --------------------------------- define constructor -------------------------------------

			c_ast::NodePtr constructor;
			{
				// the constructor collects captured variables and a pointer to a pre-allocated closure struct
				// and initializes all the closure's fields.

				// create return type
				c_ast::TypePtr returnType = varClosure->type;

				// assemble parameters
				vector<c_ast::VariablePtr> params;
				params.push_back(varClosure);
				params.push_back(varNested);
				addAll(params, varsCaptured);

				// create the body
				c_ast::InitializerPtr init = c_ast::init(res->closureType, c_ast::ref(res->mapperName), varNested);
				addAll(init->values, varsCaptured);
				c_ast::ExpressionPtr assign = c_ast::assign(c_ast::deref(varClosure), init);
				c_ast::StatementPtr body = compound(assign, c_ast::ret(varClosure));

				// assemble constructor
				constructor = manager->create<c_ast::Function>(
						c_ast::Function::STATIC | c_ast::Function::INLINE,
						returnType, res->constructorName, params, body);
			}

			// attach definitions of closure, mapper and constructor
			res->definitions = c_ast::CCodeFragment::createNew(manager,
					manager->create<c_ast::Comment>("-- Begin - Bind Constructs ------------------------------------------------------------"),
					closure, mapper, constructor,
					manager->create<c_ast::Comment>("--  End  - Bind Constructs ------------------------------------------------------------"));

			res->definitions->addDependency(nestedClosureInfo.definition);
			res->definitions->addDependency(nestedClosureInfo.caller);

			// done
			return res;
		}

		ElementInfo* FunctionInfoStore::resolveLambda(const core::LambdaExprPtr& lambda) {

			// resolve lambda definitions
			resolveLambdaDefinition(lambda->getDefinition());

			// look up lambda again
			return resolveInternal(lambda);
		}

		void FunctionInfoStore::resolveLambdaDefinition(const core::LambdaDefinitionPtr& lambdaDefinition) {

			// prepare some manager
			NameManager& nameManager = converter.getNameManager();
			core::NodeManager& manager = converter.getNodeManager();
			auto cManager = converter.getCNodeManager();

			// create definition and declaration block
			c_ast::CCodeFragmentPtr declarations = c_ast::CCodeFragment::createNew(cManager);
			c_ast::CCodeFragmentPtr definitions = c_ast::CCodeFragment::createNew(cManager);

			declarations->getCode().push_back(cManager->create<c_ast::Comment>("------- Function Prototypes ----------"));
			definitions->getCode().push_back(cManager->create<c_ast::Comment>("------- Function Definitions ---------"));

			// A) get list of all lambdas within this recursive group
			vector<std::pair<c_ast::IdentifierPtr,core::LambdaExprPtr>> lambdas;
			::transform(lambdaDefinition->getDefinitions(), std::back_inserter(lambdas),
					[&](const std::pair<core::VariablePtr, core::LambdaPtr>& cur)->std::pair<c_ast::IdentifierPtr,core::LambdaExprPtr> {
						auto lambda = core::LambdaExpr::get(manager, cur.first, lambdaDefinition);
						return std::make_pair(cManager->create(nameManager.getName(lambda)), lambda);
			});

			// B) create an entries within info table containing code fragments, wrappers and prototypes
			for_each(lambdas, [&](const std::pair<c_ast::IdentifierPtr,core::LambdaExprPtr>& pair) {

				const c_ast::IdentifierPtr& name = pair.first;
				const core::LambdaExprPtr& lambda = pair.second;

				// create information
				LambdaInfo* info = new LambdaInfo();
				info->prototype = declarations;
				info->definition = definitions;

				// if not recursive, skip prototype
				if (!lambda->isRecursive()) {
					info->prototype = definitions;
				} else {
					definitions->addDependency(declarations);
				}

				// create dummy function ... no body
				core::LambdaPtr body;
				const core::FunctionTypePtr& funType = static_pointer_cast<const core::FunctionType>(lambda->getType());
				FunctionCodeInfo codeInfo = resolveFunction(name, funType, body, false);

				auto wrapper = resolveLambdaWrapper(codeInfo.function, funType);
				info->lambdaWrapperName = wrapper.first;
				info->lambdaWrapper = wrapper.second;
				info->lambdaWrapper->addDependency(info->prototype);

				// obtain current lambda and add lambda info
				auto res = funInfos.insert(std::make_pair(lambda, info));
				assert(res.second && "Entry should not be already present!");

				// add prototype to prototype block
				declarations->getCode().push_back(cManager->create<c_ast::FunctionPrototype>(codeInfo.function));
				declarations->addDependencies(codeInfo.prototypeDependencies);
			});


			// C) create function definitions
			for_each(lambdas, [&](const std::pair<c_ast::IdentifierPtr,core::LambdaExprPtr>& pair) {

				const c_ast::IdentifierPtr& name = pair.first;
				const core::LambdaExprPtr& lambda = pair.second;

				// unrolle function and create function defintion
				core::LambdaExprPtr unrolled = lambdaDefinition->unrollOnce(manager, lambda->getVariable());

				// create dummy function ... no body
				const core::FunctionTypePtr& funType = static_pointer_cast<const core::FunctionType>(lambda->getType());
				FunctionCodeInfo codeInfo = resolveFunction(name, funType, unrolled->getLambda(), false);

				// add function
				LambdaInfo* info = static_cast<LambdaInfo*>(funInfos[lambda]);
				info->function = codeInfo.function;

				// add definition to definition block
				definitions->getCode().push_back(codeInfo.function);

				// add code dependencies
				definitions->addDependencies(codeInfo.definitionDependencies);
			});

		}


		FunctionCodeInfo FunctionInfoStore::resolveFunction(const c_ast::IdentifierPtr name,
							const core::FunctionTypePtr& funType, const core::LambdaPtr& lambda, bool external) {

			FunctionCodeInfo res;

			// get C node manager
			auto manager = converter.getCNodeManager();

			// get other managers
			TypeManager& typeManager = converter.getTypeManager();
			NameManager& nameManager = converter.getNameManager();

			// resolve return type
			const TypeInfo& returnTypeInfo = typeManager.getTypeInfo(funType->getReturnType());
			res.prototypeDependencies.insert(returnTypeInfo.definition);
			c_ast::TypePtr returnType = (external)?returnTypeInfo.externalType:returnTypeInfo.rValueType;


			// resolve parameters
			int counter = 0;
			vector<c_ast::VariablePtr> parameter;
			for_each(funType->getParameterTypes(), [&](const core::TypePtr& cur) {
				const TypeInfo& paramTypeInfo = typeManager.getTypeInfo(cur);
				res.prototypeDependencies.insert(paramTypeInfo.definition);

				c_ast::TypePtr paramType = (external)?paramTypeInfo.externalType:paramTypeInfo.rValueType;

				string paramName;
				if (lambda) {
					paramName = nameManager.getName(lambda->getParameterList()[counter]);
				} else {
					paramName = format("p%d", counter+1);
				}
				parameter.push_back(c_ast::var(paramType, manager->create(paramName)));

				counter++;
			});

			// resolve body
			c_ast::StatementPtr cBody;
			res.definitionDependencies.insert(res.prototypeDependencies.begin(), res.prototypeDependencies.end());
			if (lambda) {

				// set up variable manager
				VariableManager varManager;

				for_each(lambda->getParameterList(), [&](const core::VariablePtr& cur) {
					varManager.addInfo(converter, cur, (cur->getType()->getNodeType() == core::NT_RefType)?VariableInfo::INDIRECT:VariableInfo::NONE);
				});


				// convert the body code fragment and collect dependencies
				ConversionContext context(converter, res.definitionDependencies, varManager);
				c_ast::NodePtr code = converter.getStmtConverter().convert(context, lambda->getBody());
				cBody = static_pointer_cast<c_ast::Statement>(code);
			}

			// create function
			res.function = manager->create<c_ast::Function>(returnType, name, parameter, cBody);
			return res;
		}


		std::pair<c_ast::IdentifierPtr, c_ast::CodeFragmentPtr>
		FunctionInfoStore::resolveLambdaWrapper(const c_ast::FunctionPtr& function, const core::FunctionTypePtr& funType) {

			// get C node manager
			auto manager = converter.getCNodeManager();

			// obtain function type information
			TypeManager& typeManager = converter.getTypeManager();
			const FunctionTypeInfo& funTypeInfo = typeManager.getTypeInfo(funType);

			// create a new function representing the wrapper

			// create list of parameters for wrapper
			vector<c_ast::VariablePtr> parameter;

			// first parameter is the closure
			parameter.push_back(manager->create<c_ast::Variable>(
					manager->create<c_ast::PointerType>(funTypeInfo.rValueType),
					manager->create("closure"))
			);

			// resolve parameters
			int counter = 1;
			::transform(funType->getParameterTypes(), std::back_inserter(parameter),
					[&](const core::TypePtr& cur) {
						const TypeInfo& paramTypeInfo = typeManager.getTypeInfo(cur);
						return c_ast::var(paramTypeInfo.rValueType, manager->create(format("p%d", counter++)));
			});

			// pick a name for the wrapper
			c_ast::IdentifierPtr name = manager->create(function->name->name + "_wrap");

			// create a function body (call to the function including wrappers)
			c_ast::CallPtr call = manager->create<c_ast::Call>(function->name);
			::transform_range(make_paired_range(funType->getParameterTypes(), function->parameter), std::back_inserter(call->arguments),
					[&](const std::pair<core::TypePtr, c_ast::VariablePtr>& cur) {
						return typeManager.getTypeInfo(cur.first).externalize(manager, cur.second);
			});

			c_ast::StatementPtr body = typeManager.getTypeInfo(funType->getReturnType()).internalize(manager, call);
			if (!c_ast::isVoid(function->returnType)) {
				body = manager->create<c_ast::Return>(call);
			}

			c_ast::FunctionPtr wrapper = manager->create<c_ast::Function>(
					function->returnType, name, parameter, body
			);

			c_ast::CodeFragmentPtr res = c_ast::CCodeFragment::createNew(manager, wrapper);
			res->addDependency(funTypeInfo.definition);

			return std::make_pair(name, res);
		}


	}

} // end namespace backend
} // end namespace insieme
