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

#include "insieme/frontend/tu/ir_translation_unit.h"
#include "insieme/frontend/utils/macros.h"

#include "insieme/utils/assert.h"

#include "insieme/core/ir.h"
#include "insieme/core/types/subtyping.h"

#include "insieme/core/annotations/naming.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_class_info.h"
#include "insieme/core/lang/ir++_extension.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/type_utils.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"

#include "insieme/annotations/c/extern.h"
#include "insieme/annotations/c/include.h"

namespace insieme {
namespace frontend {
namespace tu {

	void IRTranslationUnit::addGlobal(const Global& newGlobal) {
		assert(newGlobal.first && newGlobal.first->getType().isa<core::RefTypePtr>());

		auto git = std::find_if(globals.begin(), globals.end(), [&](const Global& cur)->bool { return *newGlobal.first == *cur.first; });
		if( git == globals.end() ) {
			//global is new >> insert into globalsList
			globals.push_back(Global(mgr->get(newGlobal.first), mgr->get(newGlobal.second)));
		} else {
			//global is already in globalsList, if the "new one" has a initValue update the init
			if(newGlobal.second) {
				git->second = newGlobal.second;
			}
		}
	}

	std::ostream& IRTranslationUnit::printTo(std::ostream& out) const {
		static auto print = [](const core::NodePtr& node) { return core::printer::printInOneLine(node); };
		return out << "TU(\n\tTypes:\n\t\t"
				<< join("\n\t\t", types, [&](std::ostream& out, const std::pair<core::GenericTypePtr, core::TypePtr>& cur) { out << *cur.first << " => " << *cur.second; })
				<< ",\n\tGlobals:\n\t\t"
				<< join("\n\t\t", globals, [&](std::ostream& out, const std::pair<core::LiteralPtr, core::ExpressionPtr>& cur) { out << *cur.first << ":" << *cur.first->getType() << " => "; if (cur.second) out << print(cur.second); else out << "<uninitalized>"; })
				<< ",\n\tInitializer:\n\t\t"
				<< join("\n\t\t", initializer, [&](std::ostream& out, const core::StatementPtr& cur) { out << print(cur); })
				<< ",\n\tFunctions:\n\t\t"
				<< join("\n\t\t", functions, [&](std::ostream& out, const std::pair<core::LiteralPtr, core::ExpressionPtr>& cur) { out << *cur.first << " : " << *cur.first->getType() << " => " << print(cur.second); })
				<< ",\n\tEntry Points:\t{"
				<< join(", ", entryPoints, [&](std::ostream& out, const core::LiteralPtr& cur) { out << *cur; })
				<< "}\n)";
	}

	IRTranslationUnit IRTranslationUnit::toManager(core::NodeManager& manager) const {

		IRTranslationUnit res(manager);

		// copy types
		for(auto cur : getTypes()) {
			res.addType(cur.first, cur.second);
		}

		// copy functions
		for(auto cur : getFunctions()) {
			res.addFunction(cur.first, cur.second);
		}

		// copy globals
		for(auto cur : getGlobals()) {
			res.addGlobal(cur);
		}

		// copy initalizer
		for(auto cur : getInitializer()) {
			res.addInitializer(cur);
		}

		// entry points
		for(auto cur : getEntryPoints()) {
			res.addEntryPoints(cur);
		}

		// done
		return res;
	}

	IRTranslationUnit merge(core::NodeManager& mgr, const IRTranslationUnit& a, const IRTranslationUnit& b) {

		IRTranslationUnit res = a.toManager(mgr);

		// copy types
		for(auto cur : b.getTypes()) {
			if(!res[cur.first]) res.addType(cur.first, cur.second);
		}

		// copy functions
		for(auto cur : b.getFunctions()) {
			if(!res[cur.first]) res.addFunction(cur.first, cur.second);
		}

		// copy globals
		for(auto cur : b.getGlobals()) {
			res.addGlobal(cur);
		}

		// copy initalizer
		for(auto cur : b.getInitializer()) {
			res.addInitializer(cur);
		}

		// entry points
		for(auto cur : b.getEntryPoints()) {
			res.addEntryPoints(cur);
		}

		// done
		return res;
	}

	IRTranslationUnit merge(core::NodeManager& mgr, const vector<IRTranslationUnit>& units) {
		IRTranslationUnit res(mgr);
		for(const auto& cur : units) {
			res = merge(mgr, res, cur);
		}
		res.setCXX(any(units, [](const IRTranslationUnit& cur) { return cur.isCXX(); }));
		return res;
	}


	// ------------------------------- to-program conversion ----------------------------


	namespace {

		using namespace core;

		/**
		 * The class converting a IR-translation-unit into an actual IR program by
		 * realizing recursive definitions.
		 */
		class Resolver : private core::NodeMapping {

			NodeManager& mgr;
			IRBuilder builder;

			NodeMap cache;
			NodeMap symbolMap;
			NodeMap recVarMap;
			NodeMap recVarResolutions;
			NodeSet recVars;

			// a map collecting class-meta-info values for lazy integration
			typedef utils::map::PointerMap<TypePtr, core::ClassMetaInfo> MetaInfoMap;
			MetaInfoMap metaInfos;

			// a performance utility recording whether sub-trees contain recursive variables or not
			utils::map::PointerMap<NodePtr, bool> containsRecVars;

		public:

			Resolver(NodeManager& mgr, const IRTranslationUnit& unit)
				: mgr(mgr), builder(mgr), cache(), symbolMap(), recVarMap(), recVarResolutions(), recVars(), metaInfos() {

				// copy type symbols into symbol table
				for(auto cur : unit.getTypes()) {
					symbolMap[mgr.get(cur.first)] = mgr.get(cur.second);
				}

				// copy function symbols into symbol table
				for(auto cur : unit.getFunctions()) {
					symbolMap[mgr.get(cur.first)] = mgr.get(cur.second);
				}
			}

			template<typename T>
			Pointer<T> apply(const Pointer<T>& ptr) {
				return apply(NodePtr(ptr)).as<Pointer<T>>();
			}

			NodePtr apply(const NodePtr& node) {

				// check whether meta-infos are empty
				assert(metaInfos.empty());

				// convert node itself
				auto res = map(node);

				// copy the source list to avoid invalidation of iterator
				auto list =metaInfos;

				// re-add meta information
				for(const auto& cur : list) {

					// encode meta info into pure IR
					auto encoded = core::toIR(mgr, cur.second);

					// resolve meta info
					auto resolved = core::fromIR(map(encoded));

					// restore resolved meta info for resolved type
					setMetaInfo(map(cur.first), resolved);
				}

				// clear meta-infos for next run
				metaInfos.clear();

				// done
				return res;
			}

			virtual const NodePtr mapElement(unsigned, const NodePtr& ptr) {

				// check whether value is already cached
				{
					auto pos = cache.find(ptr);
					if (pos != cache.end()) {
						return pos->second;
					}
				}

				// strip off class-meta-information
				if (auto type = ptr.isa<TypePtr>()) {
					if (hasMetaInfo(type)) {
						metaInfos[type] = getMetaInfo(type);
						removeMetaInfo(type);
					}
				}

				// init result
				NodePtr res = ptr;

				// check whether the current node is in the symbol table
				NodePtr recVar;
				auto pos = symbolMap.find(ptr);
				if (pos != symbolMap.end()) {

					// enter a recursive substitute into the cache

					// first check whether this ptr has already been mapped to a recursive variable
					recVar = recVarMap[ptr];

					// if not, create one of the proper type
					if (!recVar) {
						// create a fresh recursive variable
						if (const GenericTypePtr& symbol = ptr.isa<GenericTypePtr>()) {
							recVar = builder.typeVariable(symbol->getFamilyName());
						} else if (const LiteralPtr& symbol = ptr.isa<LiteralPtr>()) {
							recVar = builder.variable(map(symbol->getType()));
						} else {
							assert(false && "Unsupported symbol encountered!");
						}
						recVarMap[ptr] = recVar;
						recVars.insert(recVar);
					}

					// now the recursive variable should be fixed
					assert(recVar);

					// check whether the recursive variable has already been completely resolved
					{
						auto pos = recVarResolutions.find(recVar);
						if (pos != recVarResolutions.end()) {
							// migrate annotations
							core::transform::utils::migrateAnnotations(ptr, pos->second);
							cache[ptr] = pos->second;				// for future references
							return pos->second;
						}
					}

					// update cache for current element
					cache[ptr] = recVar;

					// result will be the substitution
					res = map(pos->second);

				} else {

					// resolve result recursively
					res = res->substitute(mgr, *this);

				}


				// fix recursions
				if (recVar) {

					// check whether nobody has been messing with the cache!
					assert_eq(cache[ptr], recVar) << "Don't touch this!";

					// remove recursive variable from cache
					cache.erase(ptr);

					if (TypePtr type = res.isa<TypePtr>()) {

						// fix type recursion
						res = fixRecursion(type, recVar.as<core::TypeVariablePtr>());

						// migrate annotations
						core::transform::utils::migrateAnnotations(pos->second, res);
						if (!hasFreeTypeVariables(res)) {
							cache[ptr] = res;

							// also, register results in recursive variable resolution map
							if (const auto& recType = res.isa<RecTypePtr>()) {
								auto definition = recType->getDefinition();
								if (definition.size() > 1) {
									for(auto cur : definition) {
										auto recType = builder.recType(cur->getVariable(), definition);
										recVarResolutions[cur->getVariable()] = recType;
										containsRecVars[recType] = false;
									}
								}
							}
						}

					} else if (LambdaExprPtr lambda = res.isa<LambdaExprPtr>()) {

						// re-build current lambda with correct recursive variable
						assert_eq(1u, lambda->getDefinition().size());
						auto var = recVar.as<VariablePtr>();
						auto binding = builder.lambdaBinding(var, lambda->getLambda());
						lambda = builder.lambdaExpr(var, builder.lambdaDefinition(toVector(binding)));

						// fix recursions
						res = fixRecursion(lambda);

						// migrate annotations
						core::transform::utils::migrateAnnotations(pos->second, res);

						// add final results to cache
						if (!analysis::hasFreeVariables(res)) {
							cache[ptr] = res;

							// also, register results in recursive variable resolution map
							auto definition = res.as<LambdaExprPtr>()->getDefinition();
							if (definition.size() > 1) {
								for(auto cur : definition) {
									auto recFun = builder.lambdaExpr(cur->getVariable(), definition);
									recVarResolutions[cur->getVariable()] = recFun;
									containsRecVars[recFun] = false;
								}
							}
						}

					} else {
						assert(false && "Unsupported recursive structure encountered!");
					}
				}

				// special service: get rid of unnecessary casts (which might be introduced due to opaque generic types)
				if (const CastExprPtr& cast = res.isa<CastExprPtr>()) {
					// check whether cast can be skipped
					if (types::isSubTypeOf(cast->getSubExpression()->getType(), cast->getType())) {
						res = cast->getSubExpression();
					}
				}

				// if this is a call to ref member access we rebuild the whole expression to return
				// right ref type
				if (core::analysis::isCallOf(res, mgr.getLangBasic().getCompositeRefElem())){
					auto call = res.as<CallExprPtr>();

					if (!call[0]->getType().isa<RefTypePtr>()){
						std::cout << "composite without ref" << std::endl;
						dumpPretty(res);
						std::cout << " ================================================================= " << std::endl;
						std::cout << " ================================================================= " << std::endl;
						std::cout << " ================================================================= " << std::endl;
						std::cout << " the element is:" << std::endl;
						dumpPretty(call[0]);
						std::cout << " ================================================================= " << std::endl;
						std::cout << " ================================================================= " << std::endl;
						std::cout << " ================================================================= " << std::endl;
						std::cout << " the element type is:" << std::endl;
						dumpPretty(call[0]->getType());
					}
					if (call[0]->getType().as<RefTypePtr>()->getElementType().isa<StructTypePtr>()){
						auto tmp = builder.refMember(call[0], call[1].as<LiteralPtr>()->getValue());
							// type changed... do we have any cppRef to unwrap?
						if (*(tmp->getType()) != *(call->getType())  &&
							core::analysis::isAnyCppRef(tmp->getType().as<RefTypePtr>()->getElementType()))
							res = builder.toIRRef(builder.deref(tmp));
						else
							res = tmp;
					}
				}

				// also if this is a call to member access we rebuild the whole expression to return
				// NON ref type
				if (core::analysis::isCallOf(res, mgr.getLangBasic().getCompositeMemberAccess())){
					auto call = res.as<CallExprPtr>();
					if (call[0]->getType().isa<StructTypePtr>()){
						auto tmp = builder.accessMember(call[0], call[1].as<LiteralPtr>()->getValue());
							// type might changed, we have to unwrap it
						if (core::analysis::isAnyCppRef(tmp->getType()))
							res = builder.deref(builder.toIRRef(tmp));
						else
							res = tmp;
					}
				}

				// also fix type literals
				if (core::analysis::isTypeLiteral(res)) {
					res = builder.getTypeLiteral(core::analysis::getRepresentedType(res.as<ExpressionPtr>()));
				}

				// add result to cache if it does not contain recursive parts (hence hasn't changed at all)
				if (*ptr == *res) {
					cache[ptr] = res;
				} else if (!containsRecursiveVariable(res)) {
					cache[ptr] = res;
				}

				// simply migrate annotations
				core::transform::utils::migrateAnnotations(ptr, res);

				// done
				return res;
			}

		private:

			bool hasFreeTypeVariables(const NodePtr& node) {
				return analysis::hasFreeTypeVariables(node.isa<TypePtr>());
			}

			bool hasFreeTypeVariables(const TypePtr& type) {
				return analysis::hasFreeTypeVariables(type);
			}

			TypePtr fixRecursion(const TypePtr type, const TypeVariablePtr var) {
				// if it is a direct recursion, be done
				NodeManager& mgr = type.getNodeManager();
				IRBuilder builder(mgr);

				// make sure it is handling a struct or union type
				if(!type.isa<StructTypePtr>() && !type.isa<UnionTypePtr>())return type;

				// see whether there is any free type variable
				if (!hasFreeTypeVariables(type)) return type;

				// 1) check nested recursive types - those include this type

				// check whether there is nested recursive type specification that equals the current type
				std::vector<RecTypePtr> recTypes;
				visitDepthFirstOnce(type, [&](const RecTypePtr& cur) {
					if (cur->getDefinition()->getDefinitionOf(var)) recTypes.push_back(cur);
				}, true, true);

				// see whether one of these is matching
				for(auto cur : recTypes) {
					// TODO: here it should actually be checked whether the inner one is structurally identical
					//		 at the moment we relay on the fact that it has the same name
					return builder.recType(var, cur->getDefinition());
				}


				// 2) normalize recursive type

				// collect all struct types within the given type
				TypeList structs;
				visitDepthFirstOncePrunable(type, [&](const TypePtr& cur) {
					//if (containsVarFree(cur)) ;
					if (cur.isa<RecTypePtr>()) return !hasFreeTypeVariables(cur);
					if (cur.isa<NamedCompositeTypePtr>() && hasFreeTypeVariables(cur)) {
						structs.push_back(cur.as<TypePtr>());
					}
					return false;
				}, true);

				// check whether there is a recursion at all
				if (structs.empty()) return type;

				// create de-normalized recursive bindings
				vector<RecTypeBindingPtr> bindings;
				for(auto cur : structs) {
					bindings.push_back(builder.recTypeBinding(builder.typeVariable(cur.as<core::StructTypePtr>()->getName()), cur));
								//builder.typeVariable(insieme::core::annotations::getAttachedName(cur)), cur));
				}

				// sort according to variable names
				std::sort(bindings.begin(), bindings.end(), [](const RecTypeBindingPtr& a, const RecTypeBindingPtr& b) {
					return a->getVariable()->getVarName()->getValue() < b->getVariable()->getVarName()->getValue();
				});

				// create definitions
				RecTypeDefinitionPtr def = builder.recTypeDefinition(bindings);

				// test whether this is actually a closed type ..
				if(hasFreeTypeVariables(def.as<NodePtr>())) return type;

				// normalize recursive representation
				RecTypeDefinitionPtr old;
				while(old != def) {
					old = def;

					// set up current variable -> struct definition replacement map
					NodeMap replacements;
					for (auto cur : def) {
						replacements[cur->getType()] = cur->getVariable();
					}

					// wrap into node mapper
					auto mapper = makeLambdaMapper([&](int, const NodePtr& cur) {
						return transform::replaceAllGen(mgr, cur, replacements);
					});

					// apply mapper to defintions
					vector<RecTypeBindingPtr> newBindings;
					for (RecTypeBindingPtr& cur : bindings) {
						auto newBinding = builder.recTypeBinding(cur->getVariable(), cur->getType()->substitute(mgr, mapper));
						if (!contains(newBindings, newBinding)) newBindings.push_back(newBinding);
					}
					bindings = newBindings;

					// update definitions
					def = builder.recTypeDefinition(bindings);
				}

				// convert structs into list of definitions

				// build up new recursive type (only if it is closed)
				auto res = builder.recType(var, def);
				return hasFreeTypeVariables(res.as<TypePtr>())?type:res;
			}

			/**
			 * The conversion of recursive function is conducted lazily - first recursive
			 * functions are build in an unrolled way before they are closed (by combining
			 * multiple recursive definitions into a single one) by this function.
			 *
			 * @param lambda the unrolled recursive definition to be collapsed into a proper format
			 * @return the proper format
			 */
			LambdaExprPtr fixRecursion(const LambdaExprPtr& lambda) {

				// check whether this is the last free variable to be defined
				VariablePtr recVar = lambda->getVariable();
				auto freeVars = analysis::getFreeVariables(lambda->getLambda());
				if (freeVars != toVector(recVar)) {
					// it is not, delay closing recursion
					return lambda;
				}

				// search all directly nested lambdas
				vector<LambdaExprAddress> inner;
				visitDepthFirstOncePrunable(NodeAddress(lambda), [&](const LambdaExprAddress& cur) {
					if (cur.isRoot()) return false;
					if (!analysis::hasFreeVariables(cur)) return true;
					if (!recVars.contains(cur.as<LambdaExprPtr>()->getVariable())) return false;
					inner.push_back(cur);
					return false;
				});

				// if there is no inner lambda with free variables it is a simple recursion
				if (inner.empty()) return lambda;		// => done

				// ---------- build new recursive function ------------

				auto& mgr = lambda.getNodeManager();
				IRBuilder builder(mgr);

				// build up resulting lambda
				vector<LambdaBindingPtr> bindings;
				bindings.push_back(builder.lambdaBinding(recVar, lambda->getLambda()));
				for(auto cur : inner) {
					assert(cur->getDefinition().size() == 1u);
					auto def = cur->getDefinition()[0];

					// only add every variable once
					if (!any(bindings, [&](const LambdaBindingPtr& binding)->bool { return binding->getVariable() == def.getAddressedNode()->getVariable(); })) {
						bindings.push_back(def);
					}
				}

				LambdaExprPtr res = builder.lambdaExpr(recVar, builder.lambdaDefinition(bindings));

				// collapse recursive definitions
				while (true) {
					// search for reductions (lambda => rec_variable)
					std::map<NodeAddress, NodePtr> replacements;
					visitDepthFirstPrunable(NodeAddress(res), [&](const LambdaExprAddress& cur) {
						if (cur.isRoot()) return false;
						if (!analysis::hasFreeVariables(cur)) return true;

						// only focus on inner lambdas referencing recursive variables
						auto var = cur.as<LambdaExprPtr>()->getVariable();
						if (!res->getDefinition()->getBindingOf(var)) return false;
						replacements[cur] = var;
						return false;
					});

					// check whether the job is done
					if (replacements.empty()) break;

					// apply reductions
					res = transform::replaceAll(mgr, replacements).as<LambdaExprPtr>();
				}

				// that's it
				return res;
			}


			bool containsRecursiveVariable(const NodePtr& node) {

				// check cached values
				auto pos = containsRecVars.find(node);
				if (pos != containsRecVars.end()) return pos->second;

				// determine whether result contains a recursive variable
				bool res = recVars.contains(node) ||
						any(node->getChildList(), [&](const NodePtr& cur)->bool { return containsRecursiveVariable(cur); });

				// save result
				containsRecVars[node] = res;

				// and return it
				return res;
			}

		};


		core::LambdaExprPtr addGlobalsInitialization(const IRTranslationUnit& unit, const core::LambdaExprPtr& mainFunc, Resolver& resolver){

			core::LambdaExprPtr internalMainFunc = mainFunc;

			// we only want to init what we use, so we check it
			core::NodeSet usedLiterals;
			core::visitDepthFirstOnce (internalMainFunc, [&] (const core::LiteralPtr& literal){
				usedLiterals.insert(literal);
			});

			// check all types for dtors which use literals
			for( auto cur : unit.getTypes()) {
				TypePtr def = cur.second;
				if (core::hasMetaInfo(def.isa<TypePtr>())) {
					auto dtor = core::getMetaInfo(def).getDestructor();
					core::visitDepthFirstOnce (dtor, [&] (const core::LiteralPtr& literal){
						usedLiterals.insert(literal);
					});
				}
			}

			core::IRBuilder builder(internalMainFunc->getNodeManager());
			core::StatementList inits;

			// check all usedliterals if they are used as global and the global type is vector
			// and the usedLiteral type is array, if so replace the usedliteral type to vector and
			// us ref.vector.to.ref.array
			core::NodeMap replacements;
			for (auto cur : unit.getGlobals()) {

				const LiteralPtr& global = resolver.apply(cur.first).as<LiteralPtr>();
				const TypePtr& globalTy= global->getType();

				if (!globalTy.isa<RefTypePtr>() || !globalTy.as<RefTypePtr>()->getElementType().isa<VectorTypePtr>()) continue;

				auto findLit = [&](const NodePtr& node) {
					const LiteralPtr& usedLit = resolver.apply(node).as<LiteralPtr>();
					const TypePtr& usedLitTy = usedLit->getType();

					if (!usedLitTy.isa<RefTypePtr>()) return false;

					return usedLit->getStringValue() == global->getStringValue() &&
						usedLitTy.as<RefTypePtr>()->getElementType().isa<ArrayTypePtr>() &&
						types::isSubTypeOf(globalTy, usedLitTy);
				};

				if(any(usedLiterals,findLit)) {
					// get the literal
					LiteralPtr toReplace = resolver.apply((*std::find_if(usedLiterals.begin(), usedLiterals.end(), findLit)).as<LiteralPtr>());
					LiteralPtr global = resolver.apply(cur.first);

					//update usedLiterals to the "new" literal
					usedLiterals.erase(toReplace);
					usedLiterals.insert(global);

					//fix the access
					ExpressionPtr replacement = builder.callExpr( toReplace.getType(), builder.getLangBasic().getRefVectorToRefArray(), global);

					replacements.insert( {toReplace, replacement} );
				}
			}
			internalMainFunc = transform::replaceAll(internalMainFunc->getNodeManager(), internalMainFunc, replacements, false).as<LambdaExprPtr>();

			// ~~~~~~~~~~~~~~~~~~ INITIALIZE GLOBALS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			for (auto cur : unit.getGlobals()) {

				// only consider having an initialization value
				if (!cur.second) continue;

				core::LiteralPtr newLit = resolver.apply(cur.first);

				if (!contains(usedLiterals, newLit)) continue;

				inits.push_back(builder.assign(resolver.apply(newLit), resolver.apply(cur.second)));
			}

			// ~~~~~~~~~~~~~~~~~~ PREPARE STATICS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			const lang::StaticVariableExtension& ext = internalMainFunc->getNodeManager().getLangExtension<lang::StaticVariableExtension>();
			for (auto cur : usedLiterals) {
				auto lit = cur.as<LiteralPtr>();
				// only consider static variables
				auto type = lit->getType();
				if (!type.isa<RefTypePtr>() || !ext.isStaticType(type.as<RefTypePtr>()->getElementType())) continue;
				// add creation statement
				inits.push_back(builder.createStaticVariable(lit));
			}

			// fix the external markings
			for(auto cur : usedLiterals) {
				auto type = cur.as<LiteralPtr>()->getType();
				insieme::annotations::c::markExtern(cur.as<LiteralPtr>(),
						type.isa<RefTypePtr>() &&
						cur.as<LiteralPtr>()->getStringValue()[0]!='\"' &&  // not an string literal -> "hello world\n"
						!insieme::annotations::c::hasIncludeAttached(cur) &&
						!ext.isStaticType(type.as<RefTypePtr>()->getElementType()) &&
						!any(unit.getGlobals(), [&](const IRTranslationUnit::Global& global) { return *resolver.apply(global.first) == *cur; })
				);
			}

			// build resulting lambda
			if (inits.empty()) return internalMainFunc;

			return core::transform::insert ( internalMainFunc->getNodeManager(), core::LambdaExprAddress(internalMainFunc)->getBody(), inits, 0).as<core::LambdaExprPtr>();
		}

		core::LambdaExprPtr addInitializer(const IRTranslationUnit& unit, const core::LambdaExprPtr& mainFunc) {

			// check whether there are any initializer expressions
			if (unit.getInitializer().empty()) return mainFunc;

			// insert init statements
			auto initStmts = ::transform(unit.getInitializer(), [](const ExpressionPtr& cur)->StatementPtr { return cur; });
			return core::transform::insert( mainFunc->getNodeManager(), core::LambdaExprAddress(mainFunc)->getBody(), initStmts, 0).as<core::LambdaExprPtr>();
		}

	} // end anonymous namespace


	core::NodePtr IRTranslationUnit::resolve(const core::NodePtr& node) const {
		return Resolver(getNodeManager(), *this).apply(node);
	}

	core::ProgramPtr toProgram(core::NodeManager& mgr, const IRTranslationUnit& a, const string& entryPoint) {

		// search for entry point
		core::IRBuilder builder(mgr);
		for (auto cur : a.getFunctions()) {

			if (cur.first->getStringValue() == entryPoint) {

				// get the symbol
				core::NodePtr symbol = cur.first;

				// extract lambda expression
				Resolver resolver(mgr, a);
				core::LambdaExprPtr lambda = resolver.apply(symbol).as<core::LambdaExprPtr>();

				// add initializer
				lambda = addInitializer(a, lambda);

				// add global initializers
				lambda = addGlobalsInitialization(a, lambda, resolver);

				// wrap into program
				return builder.program(toVector<core::ExpressionPtr>(lambda));
			}
		}

		assert(false && "No such entry point!");
		return core::ProgramPtr();
	}


	core::ProgramPtr resolveEntryPoints(core::NodeManager& mgr, const IRTranslationUnit& a) {
		// convert entry points stored within TU int a program
		core::ExpressionList entryPoints;
		Resolver creator(mgr, a);
		for(auto cur : a.getEntryPoints()) {
			entryPoints.push_back(creator.apply(cur.as<core::ExpressionPtr>()));
		}
		return core::IRBuilder(mgr).program(entryPoints);
	}


} // end namespace tu
} // end namespace frontend
} // end namespace insieme
