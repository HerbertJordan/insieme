/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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
#include "insieme/core/frontend_ir_builder.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_class_info.h"
#include "insieme/core/lang/ir++_extension.h"
#include "insieme/core/lang/static_vars.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/type_utils.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/encoder/ir_class_info.h"

#include "insieme/core/ir_statistic.h"

#include "insieme/annotations/c/extern.h"
#include "insieme/annotations/c/include.h"

#include "insieme/utils/graph_utils.h"

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
				<< "},\n\tMetaInfos:\t{"
				<< join(", ", metaInfos, [&](std::ostream& out, const std::pair<core::TypePtr, std::vector<core::ClassMetaInfo>>& cur) { out << *cur.first << " : " << cur.second; })
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

		// copy meta infos
		for(auto cur : getMetaInfos()) {
			res.addMetaInfo(cur.first, cur.second);
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

		// copy initializer
		for(auto cur : b.getInitializer()) {
			res.addInitializer(cur);
		}

		// entry points
		for(auto cur : b.getEntryPoints()) {
			res.addEntryPoints(cur);
		}

		// copy meta infos
		for(auto cur : b.getMetaInfos()) {
			res.addMetaInfo(cur.first, cur.second);
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
		class Resolver : private core::SimpleNodeMapping {

			typedef utils::graph::PointerGraph<NodePtr> SymbolDependencyGraph;
			typedef utils::graph::Graph<std::set<NodePtr>> RecComponentGraph;

			NodeManager& mgr;

			FrontendIRBuilder builder;

			NodeMap symbolMap;

		public:

			Resolver(NodeManager& mgr, const IRTranslationUnit& unit)
				: mgr(mgr), builder(mgr), symbolMap() {

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

				// 1. get set of contained symbols
				const NodeSet& init = getContainedSymbols(node);

				// 2. build dependency graph
				auto depGraph = getDependencyGraph(init);

				// 3. compute SCCs graph
				auto components = computeSCCGraph(depGraph);

				// 4. resolve SCCs components bottom up
				resolveComponents(components);

				// 5. resolve input
				return map(node);
			}

		private:

			// --- Utilities ---

			bool isSymbol(const NodePtr& node) const {
				return symbolMap.find(node) != symbolMap.end();
			}

			NodePtr getDefinition(const NodePtr& symbol) const {
				assert_true(isSymbol(symbol));
				return symbolMap.find(symbol)->second;
			}

			// --- Step 1: Symbol extraction ---

			mutable utils::map::PointerMap<NodePtr, bool> containsSymbolsCache;

			bool containsSymbols(const NodePtr& node) const {
				// check cache
				auto pos = containsSymbolsCache.find(node);
				if (pos != containsSymbolsCache.end()) return pos->second;

				// compute result recursively
				bool res = isSymbol(node) || any(node->getChildList(), [&](const NodePtr& cur) { return containsSymbols(cur); });

				// cache and return result
				return containsSymbolsCache[node] = res;
			}

			mutable utils::map::PointerMap<NodePtr, NodeSet> containedSymbols;

			const NodeSet& getContainedSymbols(const NodePtr& node) const {

				// if the node is known to not contain symbols => done
				static const NodeSet empty;
				if (!containsSymbols(node)) return empty;

				// first check cache
				auto pos = containedSymbols.find(node);
				if (pos != containedSymbols.end()) return pos->second;

				// collect contained symbols
				NodeSet res;
				core::visitDepthFirstOncePrunable(node, [&](const NodePtr& cur) {
					if (isSymbol(cur)) res.insert(cur);
					return !containsSymbols(cur);
				}, true);

				// add result to cache and return it
				return containedSymbols[node] = res;
			}

			// --- Step 2: Dependency Graph ---

			SymbolDependencyGraph getDependencyGraph(const NodeSet& seed) const {

				SymbolDependencyGraph res;

				NodeList open;
				for (const auto& cur : seed) {
					if (!isResolved(cur)) open.push_back(cur);
				}

				NodeSet processed;
				while(!open.empty()) {

					// get current element
					NodePtr cur = open.back();
					open.pop_back();

					// skip elements already processed
					if (processed.contains(cur)) continue;
					processed.insert(cur);

					// skip resolved symbols
					if (isResolved(cur)) continue;

					// add vertex (even if there is no edge)
					res.addVertex(cur);

					// add dependencies to graph
					for(const auto& other : getContainedSymbols(getDefinition(cur))) {

						// skip already resolved nodes
						if (isResolved(other)) continue;

						// add dependency to other
						res.addEdge(cur, other);

						// add other to list of open nodes
						open.push_back(other);
					}
				}

				// done
				return res;
			}

			// --- Step 3: SCC computation ---

			RecComponentGraph computeSCCGraph(const SymbolDependencyGraph& graph) {
				// there is a utility for that
				return utils::graph::computeSCCGraph(graph.asBoostGraph());
			}

			// --- Step 4: Component resolution ---

			std::map<NodePtr,NodePtr> resolutionCache;
			bool cachingEnabled;

			bool isResolved(const NodePtr& symbol) const {
				assert_true(isSymbol(symbol)) << "Should only be queried for symbols!";
				return resolutionCache.find(symbol) != resolutionCache.end();
			}

			NodePtr getRecVar(const NodePtr& symbol) {
				assert_true(isSymbol(symbol));

				// create a fresh recursive variable
				if (const GenericTypePtr& type = symbol.isa<GenericTypePtr>()) {
					return builder.typeVariable(type->getFamilyName());
				} else if (const LiteralPtr& fun = symbol.isa<LiteralPtr>()) {
					return builder.variable(map(fun->getType()));
				}

				// otherwise fail!
				assert_fail() << "Unsupported symbol encountered!";
				return symbol;
			}

			bool isDirectRecursive(const NodePtr& symbol) {
				return getContainedSymbols(getDefinition(symbol)).contains(symbol);
			}

			void resolveComponents(const RecComponentGraph& graph) {

				// compute topological order -- there is a utility for this too
				auto components = utils::graph::getTopologicalOrder(graph);

				// reverse (since dependencies have to be resolved bottom up)
				components = reverse(components);

				// process in reverse order
				for (const auto& cur : components) {

					// sort variables
					vector<NodePtr> vars(cur.begin(), cur.end());
					std::sort(vars.begin(), vars.end(), compare_target<NodePtr>());

					// get first element
					auto first = vars.front();

					// skip this component if it has already been resolved as a side effect of another operation
					if (isResolved(first)) continue;

					// add variables for current components to resolution Cache
					for(const auto& s : vars) {
						resolutionCache[s] = getRecVar(s);
					}

					assert_true(isResolved(first));

					// resolve definitions (without caching temporal values)
					cachingEnabled = false;

					std::map<NodePtr,NodePtr> resolved;
					for(const auto& s : vars) {
						resolved[s] = map(getDefinition(s));
					}
					// re-enable caching again
					cachingEnabled = true;

					// close recursion if necessary
					if (vars.size() > 1 || isDirectRecursive(first)) {

						// close recursive types
						if (first.isa<GenericTypePtr>()) {

							// build recursive type definition
							vector<RecTypeBindingPtr> bindings;
							for(const auto& cur : vars) {
								bindings.push_back(
										builder.recTypeBinding(
												resolutionCache[cur].as<TypeVariablePtr>(),
												resolved[cur].as<TypePtr>()
										));
							}

							// build recursive type definition
							auto def = builder.recTypeDefinition(bindings);

							// simply construct a recursive type
							for(auto& cur : resolved) {
								auto newType = builder.recType(resolutionCache[cur.first].as<TypeVariablePtr>(), def);
								core::transform::utils::migrateAnnotations(cur.second, newType);
								cur.second = newType;
							}

						} else if (first.isa<LiteralPtr>()) {

							// build recursive lambda definition
							vector<LambdaBindingPtr> bindings;
							for(const auto& cur : vars) {
								bindings.push_back(
										builder.lambdaBinding(
												resolutionCache[cur].as<VariablePtr>(),
												resolved[cur].as<LambdaExprPtr>()->getLambda()
										));
							}

							// build recursive type definition
							auto def = builder.lambdaDefinition(bindings);

							// simply construct a recursive type
							for(auto& cur : resolved) {
								auto newFun = builder.lambdaExpr(resolutionCache[cur.first].as<VariablePtr>(), def);
								core::transform::utils::migrateAnnotations(cur.second, newFun);
								cur.second = newFun;
							}

						} else {
							assert_fail() << "Unsupported Symbol encountered: " << first << " - " << first.getNodeType() << "\n";
						}
					}

					// replace bindings with resolved definitions
					for(const auto& s : vars) {

						// we drop meta infos for now ... TODO: fix this
						if (s.isa<TypePtr>()) core::removeMetaInfo(s.as<TypePtr>());

						// migrate annotations
						core::transform::utils::migrateAnnotations(s, resolved[s]);

						// and add to cache
						resolutionCache[s] = resolved[s];
					}

				}

			}

			virtual const NodePtr mapElement(unsigned, const NodePtr& ptr) {

				// check cache first
				auto pos = resolutionCache.find(ptr);
				if (pos != resolutionCache.end()) return pos->second;

				// compute resolved type recursively
				auto res = ptr->substitute(mgr, *this);

				// Cleanups:
				if (res != ptr) {
					// special service: get rid of unnecessary casts (which might be introduced due to opaque generic types)
					if (const CastExprPtr& cast = res.isa<CastExprPtr>()) {
						// check whether cast can be skipped
						if (types::isSubTypeOf(cast->getSubExpression()->getType(), cast->getType())) {
							res = cast->getSubExpression();
						}
					}

					// if this is a call to ref member access we rebuild the whole expression to return a correct reference type
					if (core::analysis::isCallOf(res, mgr.getLangBasic().getCompositeRefElem())){
						auto call = res.as<CallExprPtr>();

						if (call[0]->getType().as<RefTypePtr>()->getElementType().isa<StructTypePtr>()){

							assert_true(call[0]);
							assert_true(call[1]);

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

					// and also fix generic-zero constructor
					if (core::analysis::isCallOf(res, mgr.getLangBasic().getZero())) {
						res = builder.getZero(core::analysis::getRepresentedType(res.as<CallExprPtr>()[0]));
					}
				}

				// if nothing has changed ..
				if (ptr == res) {
					// .. the result can always be cached (even if caching is disabled)
					return resolutionCache[ptr] = res;
				}

				// migrate annotations
				core::transform::utils::migrateAnnotations(ptr, res);

				// do not save result in cache if caching is disabled
				if (!cachingEnabled) return res;		// this is for temporaries

				// done
				return resolutionCache[ptr] = res;
			}

		};


		core::LambdaExprPtr addGlobalsInitialization(const IRTranslationUnit& unit, const core::LambdaExprPtr& mainFunc, Resolver& resolver){

			core::LambdaExprPtr internalMainFunc = mainFunc;

			// we only want to init what we use, so we check it
			core::NodeSet usedLiterals;
			core::visitDepthFirstOnce (internalMainFunc, [&] (const core::LiteralPtr& literal){
				usedLiterals.insert(literal);
			});

            // we check if the global var is used as initializer for a global var inserted in the previous step
			core::NodeSet prevAddedLiterals = usedLiterals;
			core::NodeSet currAddedLiterals;
            while(!prevAddedLiterals.empty()) {
			    for (auto cur : unit.getGlobals()) {
                    if(contains(prevAddedLiterals, cur.first)) {
			            core::visitDepthFirstOnce (cur.second, [&] (const core::LiteralPtr& literal){
                            if(literal->getType().isa<RefTypePtr>()) {
                                currAddedLiterals.insert(literal);
                                usedLiterals.insert(literal);
                            }
			            });
                    }
                }
                prevAddedLiterals = currAddedLiterals;
                currAddedLiterals.clear();
            };

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

			core::FrontendIRBuilder builder(internalMainFunc->getNodeManager());
			core::StatementList inits;

			// check all used literals if they are used as global and the global type is vector
			// and the usedLiteral type is array, if so replace the used literal type to vector and
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

	core::ClassMetaInfo IRTranslationUnit::getMetaInfo(const core::TypePtr& classType, bool symbolic) const {
		core::ClassMetaInfo metaInfo;
		auto mm = metaInfos.find(classType);
		if(mm != metaInfos.end()) {
			auto metaInfoList = mm->second;

			//merge metaInfos into one
			for(auto m : metaInfoList) {
				metaInfo = core::merge(metaInfo, m);
			}
		}

		if(symbolic)
			return metaInfo;

		auto resolvedClassType = this->resolve(classType).as<core::TypePtr>();

		// encode meta info into pure IR
		auto encoded = core::encoder::toIR(getNodeManager(), metaInfo);

		// resolve meta info
		auto resolved = core::encoder::toValue<ClassMetaInfo>(this->resolve(encoded).as<core::ExpressionPtr>());
		return resolved;
	}

	void IRTranslationUnit::addMetaInfo(const core::TypePtr& classType, const core::ClassMetaInfo& metaInfo) {
		//TODO: optimization/simplification: store the metainfos already merged?
		metaInfos[classType].push_back(metaInfo);
	}

	void IRTranslationUnit::addMetaInfo(const core::TypePtr& classType, const std::vector<core::ClassMetaInfo>& metaInfoList) {
		//TODO:  optimization/simplification: store the metainfos already merged?
		for(auto m : metaInfoList) {
			metaInfos[classType].push_back(m);
		}
	}

	namespace {

		void resolveMetaInfos(const IRTranslationUnit& tu, Resolver& resolver) {
			// attach all the meta infos of the TU to their types
			for(const auto& m : tu.getMetaInfos()) {
				auto classType = m.first;
				auto metaInfoList = m.second;  //metaInfos per classType

				// resolve targeted type
				auto resolvedClassType = resolver.apply(classType);

				//merge metaInfos into one
				core::ClassMetaInfo metaInfo;
				for(auto m : metaInfoList) {
					metaInfo = core::merge(metaInfo, m);
				}

				// encode meta info into pure IR
				auto encoded = core::encoder::toIR(tu.getNodeManager(), metaInfo);

				// resolve meta info
				auto resolved = core::encoder::toValue<ClassMetaInfo>(resolver.apply(encoded));

				// attach meta-info to type
				core::setMetaInfo(resolvedClassType, resolved);
			}
		}

	}

	core::ProgramPtr toProgram(core::NodeManager& mgr, const IRTranslationUnit& a, const string& entryPoint) {

		// search for entry point
		Resolver resolver(mgr, a);
		core::IRBuilder builder(mgr);
		for (auto cur : a.getFunctions()) {

			if (cur.first->getStringValue() == entryPoint) {

				// get the symbol
				core::NodePtr symbol = cur.first;
//				std::cout << "Starting resolving symbol " << symbol << " ...\n";

				// extract lambda expression
				core::LambdaExprPtr lambda = resolver.apply(symbol).as<core::LambdaExprPtr>();

				// add meta information
//				std::cout << "Resolving Meta-Infos ...\n";
				resolveMetaInfos(a, resolver);

				// add initializer
//				std::cout << "Adding initializers ...\n";
				lambda = addInitializer(a, lambda);

				// add global initializers
//				std::cout << "Adding globals ...\n";
				lambda = addGlobalsInitialization(a, lambda, resolver);

				// wrap into program
				return builder.program(toVector<core::ExpressionPtr>(lambda));
			}
		}

		assert_fail() << "No such entry point!\n"
				<< "Searching for: " << entryPoint << "\n";
		return core::ProgramPtr();
	}


	core::ProgramPtr resolveEntryPoints(core::NodeManager& mgr, const IRTranslationUnit& a) {

		// convert entry points stored within TU int a program
		core::ExpressionList entryPoints;
		Resolver resolver(mgr, a);
		for(auto cur : a.getEntryPoints()) {
			entryPoints.push_back(resolver.apply(cur.as<core::ExpressionPtr>()));
		}

		// resolve meta infos
		resolveMetaInfos(a, resolver);

		// built complete program
		return core::IRBuilder(mgr).program(entryPoints);
	}


} // end namespace tu
} // end namespace frontend
} // end namespace insieme
