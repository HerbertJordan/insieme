/**
 * Copyright (c) 2002-2014 Distributed and Parallel Systems Group,
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

#include "insieme/core/parser2/ir_parser.h"

#include <sstream>

#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_visitor.h"

#include "insieme/core/parser2/detail/parser.h"
#include "insieme/core/annotations/naming.h"

#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/ir++_utils.h"

#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/node_mapper_utils.h"
#include "insieme/core/encoder/lists.h"

namespace insieme {
namespace core {
namespace parser {

	// import namespaces since this was re-factored afterward
	using namespace insieme::core::parser::detail;

	namespace {

		// --- utilities for building rules ---

		NodePtr forward(Context& context) {
			assert(context.getTerms().size() == 1u);
			return context.getTerm(0);
		}

		NodePtr fail(Context& cur, const string& msg) {
			// check whether parser is currently within a speculative mode ..
			if (cur.isSpeculative()) {
				return NodePtr();
			}

			// build message and throw exception
			std::stringstream str;
			str << "Unable to parse '" << join(" ", cur.begin, cur.end, [](std::ostream& out, const Token& token) {
				out << token.getLexeme();
			}) << "' - reason: " << msg;
			throw ParseException(str.str());
		}

		template<typename Target, typename Source>
		const vector<Target>& convertList(const vector<Source>& source) {
			typedef typename std::remove_const<typename Target::element_type>::type element;
			// assert that conversion is save
			assert(all(source, [](const Source& cur) { return dynamic_pointer_cast<Target>(cur); }));
			return core::convertList<element>(source);	// use core converter
		}

		ExpressionPtr getOperand(Context& cur, int index) {
			return cur.tryDeref(cur.getTerm(index).as<ExpressionPtr>());
		};

		template<typename Target>
		TokenIter findNext(const Grammar::TermInfo& info, const TokenIter begin, const TokenIter& end, const Target& token, bool angleBackets = false) {
			vector<Token> parenthese;
			for(TokenIter cur = begin; cur != end; ++cur) {

				// early check to allow searching for open / close tokens
				if (parenthese.empty() && *cur == token) {
					return cur;
				}

				if (info.isLeftParenthese(*cur)) {
					parenthese.push_back(info.getClosingParenthese(*cur));
				}

				// special handling for enabling the parenthese pair < >
				if (angleBackets && *cur == '<') {
					parenthese.push_back(Token::createSymbol('>'));
				}

				if (info.isRightParenthese(*cur) || (angleBackets && *cur == '>' && cur != begin && *(cur-1) != '-' && *(cur-1) != '=')) {
					// if this is not matching => return end (no next token)
					if (parenthese.empty() || parenthese.back() != *cur) {
						return end;
					}
					parenthese.pop_back();
				}
				if (!parenthese.empty()) {
					continue;
				}

				if (*cur == token) {
					return cur;
				}
			}
			return end;
		}


		vector<TokenRange> split(const Grammar::TermInfo& info, const TokenRange& range, char sep, bool angleBackets = false) {
			assert(!info.isLeftParenthese(Token::createSymbol(sep)));
			assert(!info.isRightParenthese(Token::createSymbol(sep)));

			TokenIter start = range.begin();
			TokenIter end = range.end();

			vector<TokenRange> res;

			TokenIter next = findNext(info, start, end, sep, angleBackets);
			while(next != end) {
				res.push_back(TokenRange(start,next));
				start = next+1;
				next = findNext(info, start, end, sep, angleBackets);
			}

			// add final sub-range
			if (start != end) res.push_back(TokenRange(start, end));

			return res;
		}

		bool isType(Context& cur, const TokenIter& begin, const TokenIter& end) {
			try {
				// simple test to determine whether sub-range is a type
				auto backup = cur.backup();
				bool res = cur.grammar.match(cur, begin, end, "T");
				backup.restore(cur);
				return res;
			} catch (const ParseException& pe) {
				// ignore
			}
			return false;
		}

		bool isBind(Context& context, const TokenIter& begin, const TokenIter& end) {

			// it has to start with (, then some types and after the first ) a = and a >
			if (*begin != '(') return false;

			TokenIter cur = findNext(context.grammar.getTermInfo(), begin, end, ')');
			if (cur == end) return false;
			++cur;
			if (cur == end || *cur != '=') return false;
			++cur;
			if (cur == end || *cur != '>') return false;

			// yes, it should be a bind
			return true;
		}

		bool isFunction(Context& context, const TokenIter& begin, const TokenIter& end) {

			// it has to start with (, then some types and after the first ) a - and a >
			// also the definition has to end with } or ;
			if (*begin != '(') return false;

			// it has to have a minimal size
			if (std::distance(begin, end) < 6u) return false;

			// check end
			if (*(end-1) != '}' && *(end-1) != ';') return false;

			TokenIter cur = findNext(context.grammar.getTermInfo(), begin, end, ')');
			if (cur == end) return false;
			++cur;
			if (cur == end || *cur != '-') return false;
			++cur;
			if (cur == end || *cur != '>') return false;

			// yes, it should be a function
			return true;

		}

		bool allFunctions(Context& cur, const vector<TokenRange>& definitions) {
			return all(definitions, [&](const TokenRange& range) { return isFunction(cur, range.begin(), range.end()); });
		}

		bool containsOneOf(const NodePtr& root, const vector<NodePtr>& values) {
			return visitDepthFirstOnceInterruptible(root, [&](const NodePtr& cur) {
				return contains(values, cur);
			});
		}

		FunctionTypePtr getFunctionType(Context& cur, const TokenRange& range) {

			// find type definition
			assert(range.front() == '(');

			// parse argument type list
			const Grammar::TermInfo& info = cur.grammar.getTermInfo();
			TokenRange params_block(range.begin(), findNext(info, range.begin(), range.end(), '-'));

			// remove initial ( and tailing )
			params_block = params_block + 1 - 1 ;

			// process parameters
			TypeList params;
			for(const TokenRange& param : split(info, params_block, ',', true)) {
				// type is one less from the end - by skipping the id
				TokenRange typeRange = param - 1;

				// parse type
				TypePtr type = cur.grammar.match(cur, typeRange.begin(), typeRange.end(), "T").as<TypePtr>();
				assert(type && "Unable to parse parameter type!");

				// add to parameter type list
				params.push_back(type);
			}

			// parse result type
			TokenIter begin = params_block.end() + 3;
			TokenIter resEnd = findNext(info, begin, range.end(), '{');
			if (resEnd == range.end()) {
				resEnd = findNext(info, begin, range.end(), Token::createIdentifier("return"));
			} else if (resEnd->getLexeme() == "{" && (begin->getLexeme() == "struct" || begin->getLexeme() == "union")) {
				resEnd = findNext(info, resEnd+1, range.end(), '}') + 1;
			}
			TypePtr resType = cur.grammar.match(cur, begin, resEnd, "T").as<TypePtr>();
			assert(resType && "Unable to parse result type!");

			// build resulting type
			return cur.functionType(params, resType);
		}



		TermPtr getLetTerm() {

			/**
			 * The let-rule works as follows:
			 * 		let a1,..,an = e1,..,en
			 * where a1 - an are names and e1 - en expressions
			 * or types. The names a1 - an can be used inside
			 * the e1 - en for building recursive types / functions.
			 */

			// an action saving the total token range for the let rule when entering the pattern
			struct let_handler : public detail::actions {
				void enter(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					// for simplicity it is assumed that let is first part of rule
					assert(cur.getTerms().empty());
					assert(cur.getSubRanges().empty());
					cur.push(TokenRange(begin, end));
				}
				void leave(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					// clear term and sub-range lists
					cur.clearTerms();
					cur.clearSubRanges();
				}
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					// convert original bindings in actual bindings
					const auto& ranges = cur.getSubRanges();
					auto names = utils::make_range(ranges.begin()+1, ranges.end());
					vector<NodePtr> values = cur.getTerms();

					if (names.size() != values.size()) { return; }

					// get symbol manager
					ScopeManager& manager = cur.getSymbolManager();

					// get list of bindings
					vector<NodePtr> bindings;
					for(const TokenRange& name : names) {
						bindings.push_back(manager.lookup(name));
					}

					// check whether recursive elements are involved
					bool isRecursive =
							all(bindings, [](const NodePtr& value) { return value; }) &&						// no binding is null
							any(values, [&](const NodePtr& value) { return containsOneOf(value, bindings); });	// any value referes to a binding

					// add mappings ...
					if (!isRecursive) {
						for(std::size_t i=0; i<names.size(); i++) {
							annotations::attachName(values[i], names[i].front().getLexeme());
							manager.add(names[i], values[i]);
						}
						return;
					}

					// convert all values to recursive values
					if (values.front()->getNodeCategory() == NC_Type) {
						// compute block of recursive type definitions
						vector<RecTypeBindingPtr> defs;
						for(std::size_t i=0; i<names.size(); i++) {
							defs.push_back(cur.recTypeBinding(bindings[i].as<TypeVariablePtr>(), values[i].as<TypePtr>()));
						}
						RecTypeDefinitionPtr recDef = cur.recTypeDefinition(defs);

						// replace types with recursive types
						for(std::size_t i=0; i<names.size(); i++) {
							values[i] = cur.recType(bindings[i].as<TypeVariablePtr>(), recDef);
						}
					} else if (values.front()->getNodeType() == NT_LambdaExpr) {

						// build recursive function definition
						vector<LambdaBindingPtr> defs;
						for(std::size_t i=0; i<names.size(); i++) {
							defs.push_back(cur.lambdaBinding(bindings[i].as<VariablePtr>(), values[i].as<LambdaExprPtr>()->getLambda()));
						}

						LambdaDefinitionPtr def = cur.lambdaDefinition(defs);

						// replace bound names with rec functions
						for(std::size_t i=0; i<names.size(); i++) {
							values[i] = cur.lambdaExpr(bindings[i].as<VariablePtr>(), def);
						}

					} else {
						assert(false && "Undefined type encountered!");
					}

					// substitute temporal mappings with real mappings
					for(std::size_t i=0; i<names.size(); i++) {
						annotations::attachName(values[i], names[i].front().getLexeme());
						manager.replace(names[i], values[i]);
					}
				}
			};

			// an action registering all names when being stated
			struct declare_names : public detail::actions {
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {


					// get total range and list of names
					const auto& ranges = cur.getSubRanges();
					assert(ranges.size() >= 2u && "Total range and first name have to be there!");
					TokenRange total = ranges[0];
					auto names = utils::make_range(ranges.begin()+1, ranges.end());

					assert(*names.back().end() == '=');

					// isolate definitions
					TokenRange def(names.back().end()+1, total.end());
					assert(!def.empty());

					// split definitions
					vector<TokenRange> defs;
					if (names.size() > 1u) {
						defs = split(cur.grammar.getTermInfo(), def, ',');
					} else {
						defs.push_back(def);
					}

					// check whether number of names and number of definitions are matching
					if (names.size() != defs.size()) { return; }

					// test whether it is a bind on the right-hand-side
					if (names.size() == 1u && isBind(cur, defs.front().begin(), defs.front().end())) {
						return;		// no name-binding required
					}

					// determine whether definitions are functions or recursive types
					if (isType(cur, defs.front().begin(), defs.front().end())) {
						// define type variables and be done
						for(const TokenRange& name : names) {
							TypeVariablePtr var = cur.typeVariable(name.front().getLexeme());
							cur.getSymbolManager().add(name, var);
						}
						return;
					}

					// those are expressions => test whether it is function declaration
					// Observation: no non-function expressions ends with a } or a ;
					if (allFunctions(cur, defs)) {
						// bind names to variables with types matching the corresponding definitions
						for(std::size_t i = 0; i < names.size(); ++i) {
							cur.getSymbolManager().add(names[i], cur.variable(getFunctionType(cur, defs[i])));
						}
					}

					// neither recursive functions nor potential recursive types => nothing to do here

				}
			};


			auto E = rec("E");
			auto T = rec("T");
			auto id = cap(identifier());

			auto names = std::make_shared<Action<declare_names>>(seq(id, loop(seq(",",id))));
			auto values = seq(E, loop(seq(",", E))) | seq(T, loop(seq(",", T)));
			auto pattern = seq("let", names, "=",  values);
			auto full = std::make_shared<Action<let_handler>>(pattern);

			// that's it
			return full;
		}



		Grammar buildGrammar(const string& start = "N") {

			/**
			 * The Grammar is build as follows:
			 * 	Non-Terminals:
			 * 		P .. int-type parameters
			 * 		T .. types
			 * 		E .. expressions
			 * 		S .. statements
			 * 		A .. application = program (root node)
			 * 		N .. any ( P | T | E | S | A )
			 */

			auto P = rec("P");
			auto T = rec("T");
			auto E = rec("E");
			auto S = rec("S");
			auto N = rec("N");
			auto A = rec("A");

			auto id = identifier();
			auto kw = keyword();

			Grammar g(start);

			vector<RulePtr> rules;

			// the more complex let-term used at several occasions
			auto let = getLetTerm();

			// -------- add int-type parameter rules --------

			g.addRule("P", rule(
					any(Token::Int_Literal),
					[](Context& cur)->NodePtr {
						uint32_t value = utils::numeric_cast<uint32_t>(*cur.begin);
						return cur.concreteIntTypeParam(value);
					}
			));

			g.addRule("P", rule(
					"#inf",
					[](Context& cur)->NodePtr {
						return cur.infiniteIntTypeParam();
					}
			));

			g.addRule("P", rule(
					seq("#", id),
					[](Context& cur)->NodePtr {
						const string& name = *(cur.end - 1);
						if (name.size() != 1u) return fail(cur, "int-type-parameter variable name must not be longer than a single character");
						return cur.variableIntTypeParam(name[0]);
					}
			));


			// --------------- add type rules ---------------

			// add handler for parents
			struct process_parent : public detail::actions {
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					TypePtr type = cur.getTerms().back().as<TypePtr>();
					auto virtualFlag = cur.getSubRanges().back();
					ParentPtr parent = cur.parent(!virtualFlag.empty(), type);
					cur.swap(parent);	// use parent node instead of type
					cur.popRange();     // forget about the current sub-range (the virtual flag)
				}
			};

			static const auto parent = std::make_shared<Action<process_parent>>(seq(cap(opt(lit("virtual"))), T));

			// add type variables
			g.addRule("T", rule(
					seq("'", id),
					[](Context& cur)->NodePtr {
						const string& name = *(cur.end - 1);
						return cur.typeVariable(name);
					}
			));

			// add generic type
			g.addRule("T", rule(
					seq(cap(non_empty_list(id, "::")), opt(seq(":", non_empty_list(parent, ","))), opt(seq("<", list(P|T, ","), ">"))),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();

						// build up generic type name
						std::stringstream name;
						for(auto a : cur.getSubRange(0)) {
							name << a.getLexeme();
						}

						// extract parent types
						ParentList parents;
						auto it = terms.begin();
						while (it != terms.end()) {
							if (it->getNodeType() == NT_Parent) {
								parents.push_back(it->as<ParentPtr>());
							} else {
								break;
							}
							++it;
						}

						// extract type parameters
						TypeList typeParams;
						while (it != terms.end()) {
							if (it->getNodeCategory() == NC_Type) {
								typeParams.push_back(it->as<TypePtr>());
							} else if (it->getNodeCategory() == NC_IntTypeParam) {
								break;
							} else {
								assert(false && "Expecting Types and Parameters only!");
							}
							++it;
						}

						// extract int-type parameters
						IntParamList intTypeParams;
						while(it != terms.end()) {
							if (it->getNodeCategory() == NC_IntTypeParam) {
								intTypeParams.push_back(it->as<IntTypeParamPtr>());
							} else if (it->getNodeCategory() == NC_Type) {
								return fail(cur, "Type and int-parameters must be separated!");
							} else {
								assert(false && "Expecting Types and Parameters only!");
							}
							++it;
						}

						return cur.genericType(name.str(), parents, typeParams, intTypeParams);
					}
			));

			// add tuple type
			g.addRule("T", rule(
					seq("(", list(T, ","), ")"),
					[](Context& cur)->NodePtr {
						return cur.tupleType(convertList<TypePtr>(cur.getTerms()));
					}
			));

			// add function types
			g.addRule("T", rule(
					seq("(", list(T, ","), ")->", T),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();
						assert(!terms.empty());
						TypeList types = convertList<TypePtr>(terms);
						TypePtr resType = types.back();
						types.pop_back();
						return cur.functionType(types, resType, FK_PLAIN);
					}
			));

			g.addRule("T", rule(
					seq("(", list(T, ","), ")=>", T),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();
						assert(!terms.empty());
						TypeList types = convertList<TypePtr>(terms);
						TypePtr resType = types.back();
						types.pop_back();
						return cur.functionType(types, resType, FK_CLOSURE);
					}
			));

			// add constructor type rule
			g.addRule("T", rule(
					seq(T, "::(", list(T, ","), ")"),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();
						assert(!terms.empty());
						TypeList types = convertList<TypePtr>(terms);
						types[0] = cur.refType(types[0]);
						return cur.functionType(types, types[0], FK_CONSTRUCTOR);
					}
			));

			// add destructor type rule
			g.addRule("T", rule(
					seq("~", T, "::()"),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();
						assert(terms.size() == 1u);
						TypePtr classType = cur.refType(convertList<TypePtr>(terms)[0]);
						return cur.functionType(toVector(classType), classType, FK_DESTRUCTOR);
					}
			));

			// add member function type rule
			g.addRule("T", rule(
					seq(T, "::(", list(T, ","), ")->", T),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();
						assert(terms.size() >= 2u);
						TypeList types = convertList<TypePtr>(terms);
						types[0] = cur.refType(types[0]);
						TypePtr resType = types.back();
						types.pop_back();
						return cur.functionType(types, resType, FK_MEMBER_FUNCTION);
					}
			));

			// add struct and union types
			struct process_named_type : public detail::actions {
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					TypePtr type = cur.getTerms().back().as<TypePtr>();
					auto iterName = cur.getSubRanges().back();
					NamedTypePtr member = cur.namedType(iterName[0], type);
					cur.swap(member);
					cur.popRange();
				}
			};

			static const auto member = std::make_shared<Action<process_named_type>>(seq(T, cap(id)));

			g.addRule("T", rule(
					seq("struct", opt(cap(id)), opt(seq(":", non_empty_list(parent, ","))), "{", list(member,";"), opt(";"), "}"),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();

						// get name
						StringValuePtr name = cur.stringValue("");
						if (!cur.getSubRanges().empty()) {
							name = cur.stringValue(cur.getSubRange(0)[0]);
						}

						ParentList parents;
						NamedTypeList members;
						for(auto curNode : terms) {
							if (curNode->getNodeType() == NT_Parent) {
								parents.push_back(curNode.as<ParentPtr>());
							} else {
								members.push_back(curNode.as<NamedTypePtr>());
							}
						}
						return cur.structType(name, parents, members);
					}
			));

			g.addRule("T", rule(
					seq("union {", loop(seq(member, ";")), "}"),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();
						NamedTypeList members = convertList<NamedTypePtr>(terms);
						return cur.unionType(members);
					}
			));


			// add array, vector, ref and channel types
			g.addRule("T", rule(
					seq("array<", T, ",", P, ">"),
					[](Context& cur)->NodePtr {
						TypePtr elementType = cur.getTerm(0).as<TypePtr>();
						IntTypeParamPtr dim = cur.getTerm(1).as<IntTypeParamPtr>();
						return cur.arrayType(elementType, dim);
					},
					1    // higher priority than generic type rule
			));

			g.addRule("T", rule(
					seq("vector<", T, ",", P, ">"),
					[](Context& cur)->NodePtr {
						TypePtr elementType = cur.getTerm(0).as<TypePtr>();
						IntTypeParamPtr size = cur.getTerm(1).as<IntTypeParamPtr>();
						return cur.vectorType(elementType, size);
					},
					1    // higher priority than generic type rule
			));
			g.addRule("T", rule(
					seq("ref<", T, ">"),
					[](Context& cur)->NodePtr {
						TypePtr elementType = cur.getTerm(0).as<TypePtr>();
						return cur.refType(elementType);
					},
					1    // higher priority than generic type rule
			));
			g.addRule("T", rule(
					seq("src<", T, ">"),
					[](Context& cur)->NodePtr {
						TypePtr elementType = cur.getTerm(0).as<TypePtr>();
						return cur.refType(elementType, RK_SOURCE);
					},
					1    // higher priority than generic type rule
			));
			g.addRule("T", rule(
					seq("sink<", T, ">"),
					[](Context& cur)->NodePtr {
						TypePtr elementType = cur.getTerm(0).as<TypePtr>();
						return cur.refType(elementType, RK_SINK);
					},
					1    // higher priority than generic type rule
			));
			g.addRule("T", rule(
					seq("channel<", T, ",", P, ">"),
					[](Context& cur)->NodePtr {
						TypePtr elementType = cur.getTerm(0).as<TypePtr>();
						IntTypeParamPtr size = cur.getTerm(1).as<IntTypeParamPtr>();
						return cur.channelType(elementType, size);
					},
					1    // higher priority than generic type rule
			));

			// add named types
			g.addRule("T", rule(
					cap(id),
					[](Context& cur)->NodePtr {
						// simply lookup name within variable manager
						auto res = cur.getSymbolManager().lookup(cur.getSubRange(0));
						return dynamic_pointer_cast<TypePtr>(res);
					},
					1    // higher priority than generic type rule
			));

			// allow a let to be used with a type
			g.addRule("T", rule(symScop(seq(let,"in",T)), forward));

			// --------------- add literal rules ---------------


			// boolean
			g.addRule("E", rule(
					any(Token::Bool_Literal),
					[](Context& cur)->NodePtr {
						const auto& type = cur.manager.getLangBasic().getBool();
						return cur.literal(type, *cur.begin);
					}
			));

			// integers
			g.addRule("E", rule(
					any(Token::Int_Literal),
					[](Context& cur)->NodePtr {

						// determine type of literal
						const string& lexeme = cur.begin->getLexeme();
						bool lng = lexeme.back() == 'l';
						bool sig = (lng)?(*(lexeme.end()-2) == 'u'):(lexeme.back()=='u');

						auto& basic = cur.manager.getLangBasic();
						TypePtr type = (lng)?
									(sig)?basic.getUInt8():basic.getInt8()
								  : (sig)?basic.getUInt4():basic.getInt4();

						return cur.literal(type, lexeme);
					}
			));

			// floats
			g.addRule("E", rule(
					any(Token::Float_Literal),
					[](Context& cur)->NodePtr {
						const auto& type = cur.manager.getLangBasic().getReal4();
						return cur.literal(type, *cur.begin);
					}
			));

			// doubles
			g.addRule("E", rule(
					any(Token::Double_Literal),
					[](Context& cur)->NodePtr {
						const auto& type = cur.manager.getLangBasic().getReal8();
						return cur.literal(type, *cur.begin);
					}
			));

			// char literal
			g.addRule("E", rule(
					any(Token::Char_Literal),
					[](Context& cur)->NodePtr {
						return cur.literal(cur.manager.getLangBasic().getChar(), *cur.begin);
					}
			));

			// string literal
			g.addRule("E", rule(
					any(Token::String_Literal),
					[](Context& cur)->NodePtr {
						return cur.stringLit(*cur.begin);
					}
			));

			// add user defined literals
			g.addRule("E", rule(
					seq("lit(", cap(any(Token::String_Literal)), ":", T, ")"),
					[](Context& cur)->NodePtr {
						// remove initial and final "" from value
						string value = cur.getSubRange(0).front().getLexeme();
						value = value.substr(1,value.size()-2);
						return cur.literal(cur.getTerm(0).as<TypePtr>(), value);
					}
			));

			// type literals
			g.addRule("E", rule(
					seq("lit(", T, ")"),
					[](Context& cur)->NodePtr {
						// just create corresponding type literal
						return cur.getTypeLiteral(cur.getTerm(0).as<TypePtr>());
					}
			));

			g.addRule("E", rule(
					seq("type(", T, ")"),
					[](Context& cur)->NodePtr {
						// just create corresponding type literal
						return cur.getTypeLiteral(cur.getTerm(0).as<TypePtr>());
					}
			));

			// type parameter literals
			g.addRule("E", rule(
					seq("param(", P, ")"),
					[](Context& cur)->NodePtr {
						// just create corresponding int-type parameter literal
						return cur.getIntTypeParamLiteral(cur.getTerm(0).as<IntTypeParamPtr>());
					}
			));

			// identifier literals
			g.addRule("E", rule(
					seq("lit(", cap(any(Token::String_Literal)), ")"),
					[](Context& cur)->NodePtr {
						// just create an identifier literal
						string value = cur.getSubRange(0).front().getLexeme();
						value = value.substr(1,value.size()-2);
						return cur.getIdentifierLiteral(value);
					}
			));

			// struct expression
			g.addRule("E", rule(
					seq("(", T, ") {", list(E,","), "}"),
					[](Context& cur)->NodePtr {
						// check whether the given type is a struct type
						StructTypePtr structType = cur.getTerm(0).isa<StructTypePtr>();
						if (!structType) return fail(cur, "Not a struct type!");
						if (structType->size() != cur.getTerms().size() - 1) return fail(cur, "Field list does not match type!");

						// build up struct expression
						auto begin = make_paired_iterator(structType->begin(), cur.getTerms().begin()+1);
						auto end = make_paired_iterator(structType->end(), cur.getTerms().end());

						// extract name / value pairs
						NamedValueList values;
						for (auto it = begin; it != end; ++it) {
							values.push_back(cur.namedValue(it->first->getName(), it->second.as<ExpressionPtr>()));
						}

						// build struct expression
						return cur.structExpr(structType, values);
					}
			));

			// union expression
			g.addRule("E", rule(
					seq("(", T, ") {", cap(id), "=", E, "}"),
					[](Context& cur)->NodePtr {
						// check whether the given type is a union type
						UnionTypePtr unionType = cur.getTerm(0).isa<UnionTypePtr>();
						if (!unionType) return fail(cur, "Not a union type!");

						// build union expression
						auto name = cur.stringValue(cur.getSubRange(0).front().getLexeme());
						auto value = cur.getTerm(1).as<ExpressionPtr>();
						return cur.unionExpr(unionType, name, value);
					}
			));

			// tuple expression
			g.addRule("E", rule(
					seq("(", list(E, ","), ")"),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();
						auto elements = convertList<ExpressionPtr>(terms);
						return cur.tupleExpr(elements);
					}
			));

			// add lang-basic literals
			auto part = id | keyword("ref") | keyword("array") | keyword("vector") | keyword("channel");
			g.addRule("E", rule(
					seq(part, opt(seq(".", list(part, ".")))),
					[](Context& cur)->NodePtr {
						// join matched token range and see whether it is a literal!
						std::stringstream name;
						name << join("", cur.begin, cur.end, [](std::ostream& out, const Token& cur) {
							out << cur.getLexeme();
						});

						// exclude special built-in literal print (with variable argument list)
						string builtInName = name.str();
						if (builtInName == "print") {
							return fail(cur, "Print literal can not be referenced directly - use statement instead!");
						}

						// look up literal within lang-basic
						try {
							return cur.getLangBasic().getBuiltIn(builtInName);
						} catch (const lang::LiteralNotFoundException& lnfe) {
							return fail(cur, "Unknown lang-basic literal!");
						}
					}
			));


			// --------------- add expression rules ---------------

			// -- unary minus --

			g.addRule("E", rule(
				seq("-", E),
				[](Context& cur)->NodePtr {
					ExpressionPtr a = getOperand(cur, 0);
					return cur.minus(a);
				}
			));

			// TODO: those extend parsing time too much - figure out why and fix it!
//			// post-increment
//			g.addRule("E", rule(
//				seq(E,"++"),
//				[](Context& cur)->NodePtr {
//					return cur.postInc(cur.getTerm(0).as<ExpressionPtr>());
//				},
//				-15
//			));
//
//			// post-decrement
//			g.addRule("E", rule(
//				seq(E,"--"),
//				[](Context& cur)->NodePtr {
//					return cur.postDec(cur.getTerm(0).as<ExpressionPtr>());
//				},
//				-15
//			));
//
//			// post-increment
//			g.addRule("E", rule(
//				seq("++", E),
//				[](Context& cur)->NodePtr {
//					return cur.preInc(cur.getTerm(0).as<ExpressionPtr>());
//				},
//				-14
//			));
//
//			// post-decrement
//			g.addRule("E", rule(
//				seq("--", E),
//				[](Context& cur)->NodePtr {
//					return cur.preDec(cur.getTerm(0).as<ExpressionPtr>());
//				},
//				-14
//			));

			// -- arithmetic expressions --

			g.addRule("E", rule(
					seq(E, cap(lit("+") | lit("-")) , E),
					[](Context& cur)->NodePtr {
						// get elements
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);

						const auto& op = cur.getSubRanges().back()[0];
						cur.popRange();		// consume operator

						// Interpret operator
						if (op == "+") return cur.add(a,b);
						if (op == "-") return cur.sub(a,b);
						assert_fail() << "Unsupported operator: " << op << "\n";
						return cur.add(a,b);
					},
					-12
			));

			g.addRule("E", rule(
					seq(E, cap(lit("*") | lit("/") | lit("%")), E),
					[](Context& cur)->NodePtr {
						// get elements
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);

						const auto& op = cur.getSubRanges().back()[0];
						cur.popRange();		// consume operator

						// Interpret operator
						if (op == "*") return cur.mul(a,b);
						if (op == "/") return cur.div(a,b);
						if (op == "%") return cur.mod(a,b);
						assert_fail() << "Unsupported operator: " << op << "\n";
						return cur.add(a,b);
					},
					-13
			));

			// -- bitwise arithmetic expressions --

			g.addRule("E", rule(
					seq(E, "&", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.bitwiseAnd(a,b);
					},
					-8
			));

			g.addRule("E", rule(
					seq(E, "^", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.bitwiseXor(a,b);
					},
					-7
			));

			g.addRule("E", rule(
					seq(E, "|", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.bitwiseOr(a,b);
					},
					-6
			));

			// -- logical expressions --

			g.addRule("E", rule(
					seq("!", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						return cur.logicNeg(a);
					},
					-14
			));

			g.addRule("E", rule(
					seq(E, "&&", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.logicAnd(a,b);
					},
					-5
			));

			g.addRule("E", rule(
					seq(E, "||", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.logicOr(a,b);
					},
					-4
			));

			// -- comparison expressions --

			g.addRule("E", rule(
					seq(E, "<", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.lt(a,b);
					},
					-10
			));

			g.addRule("E", rule(
					seq(E, "<=", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.le(a,b);
					},
					-10
			));

			g.addRule("E", rule(
					seq(E, ">=", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.ge(a,b);
					},
					-10
			));

			g.addRule("E", rule(
					seq(E, ">", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.gt(a,b);
					},
					-10
			));

			g.addRule("E", rule(
					seq(E, "==", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.eq(a,b);
					},
					-9
			));

			g.addRule("E", rule(
					seq(E, "!=", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.ne(a,b);
					},
					-9
			));

			// -- cast --
			g.addRule("E", rule(
					seq("(", T, ")", E),
					[](Context& cur)->NodePtr {
						return cur.castExpr(
								cur.getTerm(0).as<TypePtr>(),
								cur.getTerm(1).as<ExpressionPtr>()
						);
					},
					-14
			));

			// -- ref manipulations --

			g.addRule("E", rule(
					seq("var(",E,")"),
					[](Context& cur)->NodePtr {
						return cur.refVar(cur.getTerm(0).as<ExpressionPtr>());
					},
					0
			));

			g.addRule("E", rule(
					seq("var(",T,")"),
					[](Context& cur)->NodePtr {
						return cur.refVar(cur.undefined(cur.getTerm(0).as<TypePtr>()));
					},
					-1		// less priority than the expression based variant
			));

			g.addRule("E", rule(
					seq("new(",E,")"),
					[](Context& cur)->NodePtr {
						return cur.refNew(cur.getTerm(0).as<ExpressionPtr>());
					},
					0
			));

			g.addRule("E", rule(
					seq("new(",T,")"),
					[](Context& cur)->NodePtr {
						return cur.refNew(cur.undefined(cur.getTerm(0).as<TypePtr>()));
					},
					-1		// less priority than the expression based variant
			));

			g.addRule("E", rule(
					seq("loc(",E,")"),
					[](Context& cur)->NodePtr {
						return cur.refLoc(cur.getTerm(0).as<ExpressionPtr>());
					},
					0
			));

			g.addRule("E", rule(
					seq("loc(",T,")"),
					[](Context& cur)->NodePtr {
						return cur.refLoc(cur.undefined(cur.getTerm(0).as<TypePtr>()));
					},
					-1		// less priority than the expression based variant
			));

			g.addRule("E", rule(
					seq("delete(",E,")"),
					[](Context& cur)->NodePtr {
						return cur.refDelete(cur.getTerm(0).as<ExpressionPtr>());
					}
			));

			// assign
			g.addRule("E", rule(
					seq(E,"=",E),
					[](Context& cur)->NodePtr {
						if (!cur.getTerm(0).as<ExpressionPtr>().getType().isa<RefTypePtr>()) return fail(cur,"Invalid assignment target!");
						return cur.assign(
								cur.getTerm(0).as<ExpressionPtr>(),
								cur.getTerm(1).as<ExpressionPtr>()
						);
					}
			));

			// deref
			g.addRule("E", rule(
					seq("*", E),
					[](Context& cur)->NodePtr {
						if (!cur.getTerm(0).as<ExpressionPtr>().getType().isa<RefTypePtr>()) return fail(cur, "Unable to de-ref non ref value.");
						return cur.deref(cur.getTerm(0).as<ExpressionPtr>());
					},
					-14
			));


			// -- add support for if-then-else operator ---
			g.addRule("E", rule(
					seq(E,"?",E,":",E),
					[](Context& cur)->NodePtr {
						return cur.ite(
							cur.getTerm(0).as<ExpressionPtr>(),
							cur.wrapLazy(cur.getTerm(1).as<ExpressionPtr>()),
							cur.wrapLazy(cur.getTerm(2).as<ExpressionPtr>())
						);
					},
					-3
			));


			// -- vector / array access --

			g.addRule("E", rule(
					seq(E, "[", E, "]"),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = cur.getTerm(0).as<ExpressionPtr>();
						ExpressionPtr b = getOperand(cur, 1);

						// support signed indices (automatic cast to unsigned)
						auto& basic = cur.getLangBasic();
						if (basic.isSignedInt(b->getType())) {
							b = cur.castExpr(basic.getUInt8(), b);
						}

						if (a->getType()->getNodeType() == NT_RefType) {
							return cur.arrayRefElem(a, b);
						}
						return cur.arraySubscript(a, b);		// works for arrays and vectors
					},
					-15
			));

			// member access
			g.addRule("E", rule(
					seq(E, ".", cap(id)),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = cur.getTerm(0).as<ExpressionPtr>();

						// check whether access is valid
						StructTypePtr structType;
						if (a->getType()->getNodeType() == NT_StructType) {
							structType = a->getType().as<StructTypePtr>();
						} else if (a->getType()->getNodeType() == NT_RefType) {
							TypePtr type = a->getType().as<RefTypePtr>()->getElementType();

							if ( type->getNodeType() == core::NT_RecType ) {
								type = core::static_pointer_cast<const core::RecType>(type)->unroll(type.getNodeManager());
							}

							structType = type.isa<StructTypePtr>();
							if (!structType) {
								std::cout << "Accessing element of non-struct type!" << std::endl;
								std::cout << *type << std::endl;
								return fail(cur, "Accessing element of non-struct type!");
							}
						} else {
								std::cout << "Accessing element of non-struct type!" << std::endl;
							return fail(cur, "Accessing element of non-struct type!");
						}

						// check field
						if (!structType->getNamedTypeEntryOf(cur.getSubRange(0)[0])) {
							std::cout << "unknown field" << std::endl;
							return fail(cur, "Accessing unknown field!");
						}

						// create access
						if (a->getType()->getNodeType() == NT_RefType) {
							return cur.refMember(a, cur.getSubRange(0)[0]);
						}
						return cur.accessMember(a, cur.getSubRange(0)[0]);
					},
					-15
			));

			// member access based on the -> operator
			g.addRule("E", rule(
					seq(E, "->", cap(id)),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = cur.getTerm(0).as<ExpressionPtr>();

						// check whether target is a reference
						if (a->getType()->getNodeType() != NT_RefType) {
								std::cout << "Accessing non-pointer elemet!" << std::endl;
							return fail(cur, "Accessing non-pointer element!");
						}

						// extract struct type
						TypePtr type = a->getType().as<RefTypePtr>()->getElementType();
						if ( type->getNodeType() == core::NT_RecType ) {
							type = core::static_pointer_cast<const core::RecType>(type)->unroll(type.getNodeManager());
						}
						StructTypePtr structType = type.isa<StructTypePtr>();
						if (!structType) {
								std::cout << "Accessing element of non-struct type!" << std::endl;
							return fail(cur, "Accessing non-struct element!");
						}

						// check field
						if (!structType->getNamedTypeEntryOf(cur.getSubRange(0)[0])) {
							std::cout << "unknown field" << std::endl;
							return fail(cur, "Accessing unknown field!");
						}

						// create access
						return cur.refMember(a, cur.getSubRange(0)[0]);
					},
					-15
			));

			// parent access / cast
			g.addRule("E", rule(
					seq(E, ".as(", T, ")"),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = cur.getTerm(0).as<ExpressionPtr>();
						TypePtr t = cur.getTerm(1).as<TypePtr>();
						if (!core::analysis::isObjectReferenceType(a->getType()) || !core::analysis::isObjectType(t)) {
							return fail(cur, "No valid object parent access!");
						}
						return cur.refParent(a, t);
					},
					-15
			));

			// member function call
			g.addRule("E", rule(
					seq(E, lit(".") | lit("->"), E, "(", list(E, ","), ")"),
					[](Context& cur)->NodePtr {
						NodeList terms = cur.getTerms();
						assert(terms.size() >= 2u);

						// get the object
						ExpressionPtr obj = terms.front().as<ExpressionPtr>();

						// get member function
						ExpressionPtr fun = terms[1].as<ExpressionPtr>();
						terms.erase(terms.begin()+1);

						// check function type
						TypePtr type = fun->getType();
						if (type->getNodeType()!=NT_FunctionType || !type.as<FunctionTypePtr>()->isMemberFunction()) {
							return fail(cur, "Calling non-member-function type!");
						}

						// check number of arguments
						FunctionTypePtr funType = type.as<FunctionTypePtr>();
						if (funType->getParameterTypes().size() != terms.size()) {
							return fail(cur, "Invalid number of arguments!");
						}

						// build member function call expression
						return cur.callExpr(fun, convertList<ExpressionPtr>(terms));
					},
					-15			// same precedence than member element access
			));

			// -- parentheses --

			g.addRule("E", rule(seq("(", E, ")"), forward, 15));

			// -- add Variable --
			g.addRule("E", rule(
					cap(id),
					[](Context& cur)->NodePtr {
						// simply lookup name within variable manager
						NodePtr res = cur.getVarScopeManager().lookup(cur.getSubRange(0));
						if (res && res->getNodeCategory() == NC_Expression) return res;
						return cur.getSymbolManager().lookup(cur.getSubRange(0)).isa<ExpressionPtr>();
					},
					2 // higher priority than other rules
			));

			g.addRule("E", rule(
					cap(id),
					[](Context& cur)->NodePtr {
						// simply lookup name within variable manager
						auto res = cur.getSymbolManager().lookup(cur.getSubRange(0));
						return dynamic_pointer_cast<ExpressionPtr>(res);
					},
					1    // lower priority than variable resolution
			));

			// -- call expression --
			g.addRule("E", rule(
					seq(E,"(", list(E,","), ")"),
					[](Context& cur)->NodePtr {
						NodeList terms = cur.getTerms();
						// get function
						ExpressionPtr fun = terms.front().as<ExpressionPtr>();
						terms.erase(terms.begin());

						TypePtr type = fun->getType();
						if (analysis::isRefType(type)) {
							fun = cur.deref(fun);
							type = fun->getType();
						}
						if (type->getNodeType()!=NT_FunctionType) {
							return fail(cur, "Calling non-function type!");
						}

						FunctionTypePtr funType = type.as<FunctionTypePtr>();
						if (funType->getParameterTypes().size() != terms.size()) {
							return fail(cur, "Invalid number of arguments!");
						}

						return cur.callExpr(fun, convertList<ExpressionPtr>(terms));
					}
			));


			// -- let expression --
			g.addRule("E", rule(symScop(seq(let,"in",E)), forward));


			struct register_param : public detail::actions {
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					TypePtr type = cur.getTerms().back().as<TypePtr>();
					auto paramName = cur.getSubRange(0);
					VariablePtr param = cur.variable(type);
					annotations::attachName(param, paramName.front());
					cur.getVarScopeManager().add(paramName, param);
					cur.swap(param);	// exchange type with variable
					cur.popRange();		// drop name from stack
				}
			};

			static const auto param = std::make_shared<Action<register_param>>(seq(T, cap(id)));

			// function expressions
			g.addRule("E", rule(
					newScop(seq("(", list(param,","), ")->", T, S)),
					[](Context& cur)->NodePtr {
						// construct the lambda
						NodeList terms = cur.getTerms();
						StatementPtr body = terms.back().as<StatementPtr>();
						terms.pop_back();
						TypePtr resType = terms.back().as<TypePtr>();
						terms.pop_back();
						return cur.lambdaExpr(resType, body, convertList<VariablePtr>(terms));
					}
			));


			// -- bind expression --
			g.addRule("E", rule(
					seq("(", list(param, ","), ")=>", E | S),
					[](Context& cur)->NodePtr {
						// construct
						NodeList terms = cur.getTerms();
						StatementPtr stmt = terms.back().as<StatementPtr>();
						terms.pop_back();		// drop body expression

						// derive call expression
						CallExprPtr call;
						if (stmt->getNodeType() == NT_CallExpr) {
							call = stmt.as<CallExprPtr>();
						} else if (stmt->getNodeCategory() == NC_Expression){
							call = cur.id(stmt.as<ExpressionPtr>());
						} else if (stmt->getNodeCategory() == NC_Statement) {
							// try outlining the statement
							if (transform::isOutlineAble(stmt)) {
								call = transform::outline(cur.getNodeManager(), stmt);
							}
						}

						// check whether call-conversion was successful
						if (!call) {
							return fail(cur, "Not an outline-able context!");
						}

						// build bind expression
						return cur.bindExpr(convertList<VariablePtr>(terms), call);
					}
			));

			// job
			g.addRule("E", rule(
					seq("job([", E, "-", E, "], ", E,")"),
					[](Context& cur)->NodePtr {
						NodeList terms = cur.getTerms();
						ExpressionPtr rangeLowerBound = terms[0].as<ExpressionPtr>();
						ExpressionPtr rangeUpperBound = terms[1].as<ExpressionPtr>();
						CallExprPtr call = terms[2].as<CallExprPtr>();

						if (!call) {
							return fail(cur, "Not a call expression!");
						}

						BindExprPtr bind 	= cur.bindExpr(VariableList(), call);
						JobExprPtr job		= cur.jobExpr(cur.getThreadNumRange(rangeLowerBound, rangeUpperBound),
								vector<core::DeclarationStmtPtr>(), vector<core::GuardedExprPtr>(), bind);

						return job;
					}
			));

			// -- job expressions --
			g.addRule("E", rule(
					seq("task", S),
					[](Context& cur)->NodePtr {
						return cur.jobExpr(cur.getTerm(0).as<StatementPtr>(), 1);
					}
			));

			g.addRule("E", rule(
					seq("job", S),
					[](Context& cur)->NodePtr {
						return cur.jobExpr(cur.getTerm(0).as<StatementPtr>(), -1);		// builds a job for more than a single thread
					}
			));

			// -- this-pointer utilities --
			struct register_this_pointer : public detail::actions {
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {

					// a static token range representing a single this token
					static auto thisString = toVector(Token::createIdentifier("this"));
					static auto thisRange = TokenRange(thisString.begin(), thisString.end());

					TypePtr type = cur.getTerms().back().as<TypePtr>();
					VariablePtr param = cur.variable(cur.refType(type));
					cur.getVarScopeManager().add(thisRange, param);
					cur.swap(param); // exchange type with variable
				}
			};

			static const auto classType = std::make_shared<Action<register_this_pointer>>(seq(T));

			// -- constructors --
			g.addRule("E", rule(
					newScop(seq(classType, "::(", list(param, ","), ") ", S)),
					[](Context& cur)->NodePtr {
						// construct the lambda
						NodeList terms = cur.getTerms();
						StatementPtr body = terms.back().as<StatementPtr>();
						terms.pop_back();

						// build member function type
						auto types = extractTypes(convertList<VariablePtr>(terms));
						auto functionType = cur.functionType(types, types[0], FK_CONSTRUCTOR);
						return cur.lambdaExpr(functionType, convertList<VariablePtr>(terms), body);
					}
			));

			// -- constructors --
			g.addRule("E", rule(
					newScop(seq("~", classType, "::()", S)),
					[](Context& cur)->NodePtr {
						// construct the lambda
						NodeList terms = cur.getTerms();
						StatementPtr body = terms.back().as<StatementPtr>();
						terms.pop_back();

						// build member function type
						auto types = extractTypes(convertList<VariablePtr>(terms));
						auto functionType = cur.functionType(types, types[0], FK_DESTRUCTOR);
						return cur.lambdaExpr(functionType, convertList<VariablePtr>(terms), body);
					}
			));

			// -- member function --
			g.addRule("E", rule(
					newScop(seq(classType, "::(", list(param, ","), ")->", T, S)),
					[](Context& cur)->NodePtr {
						// construct the lambda
						NodeList terms = cur.getTerms();
						StatementPtr body = terms.back().as<StatementPtr>();
						terms.pop_back();
						TypePtr resType = terms.back().as<TypePtr>();
						terms.pop_back();

						// build member function type
						auto functionType = cur.functionType(extractTypes(convertList<VariablePtr>(terms)), resType, FK_MEMBER_FUNCTION);
						return cur.lambdaExpr(functionType, convertList<VariablePtr>(terms), body);
					}
			));


			// ------------- add parallel constructs -------------

			g.addRule("E", rule(
					seq("spawn", E),
					[](Context& cur)->NodePtr {
						return cur.parallel(cur.getTerm(0).as<ExpressionPtr>(), 1);
					}
			));

			g.addRule("E", rule(
					seq("spawn", S),
					[](Context& cur)->NodePtr {
						return cur.parallel(cur.getTerm(0).as<StatementPtr>(), 1);
					}
			));

			g.addRule("S", rule(
					seq("sync", E, ";"),
					[](Context& cur)->NodePtr {
						const auto& basic = cur.getNodeManager().getLangBasic();
						auto group = cur.getTerm(0).as<ExpressionPtr>();
						return cur.callExpr(basic.getUnit(), basic.getMerge(), group);
					},
					8		// higher priority than a variable declaration (of type sync)
			));

			g.addRule("S", rule(
					seq("sync;"),
					[](Context& cur)->NodePtr {
						const auto& basic = cur.getNodeManager().getLangBasic();
						return cur.callExpr(basic.getUnit(), basic.getMergeAll());
					}
			));

			g.addRule("S", rule(
					seq("syncAll;"),
					[](Context& cur)->NodePtr {
						const auto& basic = cur.getNodeManager().getLangBasic();
						return cur.callExpr(basic.getUnit(), basic.getMergeAll());
					}
			));


			// --------------- add statement rules ---------------

			// -- add Symbols --
			g.addRule("S", rule(
					seq(cap(id),";"),
					[](Context& cur)->NodePtr {
						// test whether it is a known symbol
						auto res = cur.getSymbolManager().lookup(cur.getSubRange(0));
						return dynamic_pointer_cast<StatementPtr>(res);
					}
			));

			// every expression is a statement (if terminated by ;)
			g.addRule("S", rule(seq(E,";"), forward, -1));	// lower priority since less likely

			// allow ; at the end of statements
			g.addRule("S", rule(seq(S,";"), forward, -2));	// lower priority since even less likely

			// every declaration is a statement
			g.addRule("S", rule(seq(let, ";"), [](Context& cur)->NodePtr { return cur.getNoOp(); }));

			// declaration statement
			g.addRule("S", rule(
					seq(T, cap(id), " = ", E, ";"),
					[](Context& cur)->NodePtr {
						TypePtr type = cur.getTerm(0).as<TypePtr>();
						ExpressionPtr value = cur.getTerm(1).as<ExpressionPtr>();

						auto decl = cur.declarationStmt(type, value);

						// register name within variable manager
						cur.getVarScopeManager().add(cur.getSubRange(0), decl->getVariable());

						// attach name
						annotations::attachName(decl->getVariable(), cur.getSubRange(0).front());

						return decl;
					}
			));

			// declaration statement
			g.addRule("S", rule(
					seq(T, cap(id), ";"),
					[](Context& cur)->NodePtr {
						IRBuilder builder(cur.manager);
						TypePtr type = cur.getTerm(0).as<TypePtr>();
						ExpressionPtr value = builder.undefined(type);

						// wrap into a ref.var if it is a reference type
						if (type->getNodeType() == core::NT_RefType) {
							value = builder.refVar(builder.undefined(type.as<RefTypePtr>()->getElementType()));
						}

						auto decl = builder.declarationStmt(type, value);

						// register name within variable manager
						cur.getVarScopeManager().add(cur.getSubRange(0), decl->getVariable());

						// attach name
						annotations::attachName(decl->getVariable(), cur.getSubRange(0).front());

						return decl;
					}
			));

			// auto-declaration statement
			g.addRule("S", rule(
					seq("auto", cap(id), " = ", E, ";"),
					[](Context& cur)->NodePtr {
						ExpressionPtr value = cur.getTerm(0).as<ExpressionPtr>();
						auto decl = cur.declarationStmt(value->getType(), value);

						// register name within variable manager
						cur.getVarScopeManager().add(cur.getSubRange(0), decl->getVariable());

						// attach name
						annotations::attachName(decl->getVariable(), cur.getSubRange(0).front());

						return decl;
					},
					1 // higher priority than ordinary declaration
			));

			// compound statement
			g.addRule("S", rule(
					varScop(seq("{", loop(S, Token::createSymbol(';')), "}")),
					[](Context& cur)->NodePtr {
						IRBuilder builder(cur.manager);
						StatementList list;		// filter out no-ops
						for(const StatementPtr& stmt : convertList<StatementPtr>(cur.getTerms())) {
							if (!builder.isNoOp(stmt)) list.push_back(stmt);
						}
						return builder.compoundStmt(list);
					}
			));

			// if-then
			g.addRule("S", rule(
					seq("if(", E, ")", S),
					[](Context& cur)->NodePtr {
						const auto& terms = cur.getTerms();
						ExpressionPtr condition = terms[0].as<ExpressionPtr>();
						StatementPtr thenPart = terms[1].as<StatementPtr>();
						if (!cur.manager.getLangBasic().isBool(condition->getType())) {
							return fail(cur, "Conditional expression is not a boolean expression!");
						}
						return cur.ifStmt(condition, thenPart);
					}
			));

			// if-then-else
			g.addRule("S", rule(
					seq("if(", E, ")", S, "else", S),
					[](Context& cur)->NodePtr {
						const auto& terms = cur.getTerms();
						ExpressionPtr condition = terms[0].as<ExpressionPtr>();
						StatementPtr thenPart = terms[1].as<StatementPtr>();
						StatementPtr elsePart = terms[2].as<StatementPtr>();
						if (!cur.manager.getLangBasic().isBool(condition->getType())) {
							return fail(cur, "Conditional expression is not a boolean expression!");
						}
						return cur.ifStmt(condition, thenPart, elsePart);
					},
					-1		// lower priority for this rule (default=0)
			));

			// switch
			g.addRule("S", rule(
					seq("switch(", E, ") {", loop(seq("case", E, ":", S)), opt(seq("default:", S)),"}"),
					[](Context& cur)->NodePtr {
						const auto& terms = cur.getTerms();

						ExpressionPtr expr = terms[0].as<ExpressionPtr>();

						vector<SwitchCasePtr> cases;
						for(unsigned i = 1; i+1<terms.size(); i+=2) {
							ExpressionPtr caseValue = terms[i].as<ExpressionPtr>();
							StatementPtr caseBody = terms[i+1].as<StatementPtr>();

							// check that value is a literal
							if (!caseValue.isa<LiteralPtr>()) return NodePtr();

							// add case
							cases.push_back(cur.switchCase(caseValue.as<LiteralPtr>(), caseBody));
						}

						// get default body
						StatementPtr defaultBody = (terms.size() % 2) ? cur.getNoOp(): terms.back().as<StatementPtr>();

						// build switch and be done
						return cur.switchStmt(expr, cases, defaultBody);
					}
			));

			// --- a utility to handle local variable declarations ---
			struct register_var : public detail::actions {
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					TypePtr type = cur.getTerms().rbegin()->as<TypePtr>();
					auto iterName = *cur.getSubRanges().rbegin();
					VariablePtr iter = cur.variable(type);
					cur.getVarScopeManager().add(iterName, iter);
					cur.push(iter);
				}
			};

			static const auto varDecl = std::make_shared<Action<register_var>>(seq(T, cap(id)));

			// try-catch
			g.addRule("S", rule(
					seq("try", S, loop(varScop(seq("catch(", varDecl, ")", S)))),
					[](Context& cur)->NodePtr {
						const auto& terms = cur.getTerms();

						StatementPtr body = terms[0].as<StatementPtr>();
						if (!body.isa<CompoundStmtPtr>()) return fail(cur, "Body of try-catch needs to be a compound statement.");

						vector<CatchClausePtr> clauses;
						for(unsigned i = 1; i+2<terms.size(); i+=3) {
							ExpressionPtr var = terms[i+1].as<ExpressionPtr>();
							StatementPtr body = terms[i+2].as<StatementPtr>();

							// check that value is a literal
							if (!var.isa<VariablePtr>()) return fail(cur, "Invalid variable declaration in catch clause.");
							if (!body.isa<CompoundStmtPtr>()) return fail(cur, "Invald body of catch clause.");

							// add case
							clauses.push_back(cur.catchClause(var.as<VariablePtr>(), body.as<CompoundStmtPtr>()));
						}

						// check presence of clauses
						if (clauses.empty()) return fail(cur, "No catch-clause present.");

						// build try-catch stmt
						return cur.tryCatchStmt(body.as<CompoundStmtPtr>(), clauses);
					}
			));


			// while statement
			g.addRule("S", rule(
					seq("while(", E, ")", S),
					[](Context& cur)->NodePtr {
						const auto& terms = cur.getTerms();
						ExpressionPtr condition = terms[0].as<ExpressionPtr>();
						StatementPtr body = terms[1].as<StatementPtr>();
						if (!cur.manager.getLangBasic().isBool(condition->getType())) {
							return fail(cur, "Conditional expression is not a boolean expression!");
						}
						return cur.whileStmt(condition, body);
					}
			));

			// for loop without step size
			g.addRule("S", rule(
					varScop(seq("for(", varDecl, "=", E, "..", E, opt(seq(":",E)), ")", S)),
					[](Context& cur)->NodePtr {
						const auto& terms = cur.getTerms();
						TypePtr type = terms[0].as<TypePtr>();
						VariablePtr iter = terms[1].as<VariablePtr>();
						ExpressionPtr start = terms[2].as<ExpressionPtr>();
						ExpressionPtr end = terms[3].as<ExpressionPtr>();

						// extract step if present
						ExpressionPtr step = (terms.size() == 6u)?terms[4].as<ExpressionPtr>():cur.literal(type, "1");

						StatementPtr body = terms.back().as<StatementPtr>();

						auto& basic = cur.manager.getLangBasic();
						if (!basic.isInt(type)) return fail(cur, "Iterator has to be of integer type!");

						// build loop
						return cur.forStmt(iter, start, end, step, body);
					}
			));

			g.addRule("S", rule(
					seq("return ", E, ";"),
					[](Context& cur)->NodePtr {
						return cur.returnStmt(cur.getTerm(0).as<ExpressionPtr>());
					},
					1		// higher priority than variable declaration (of type return)
			));

			g.addRule("S", rule(
					seq("return;"),
					[](Context& cur)->NodePtr {
						return cur.returnStmt(
								cur.manager.getLangBasic().getUnitConstant()
						);
					}
			));

			g.addRule("S", rule(
					seq("break;"),
					[](Context& cur)->NodePtr {
						return cur.breakStmt();
					}
			));

			g.addRule("S", rule(
					seq("continue;"),
					[](Context& cur)->NodePtr {
						return cur.continueStmt();
					}
			));

			g.addRule("S", rule(
					seq(cap(id), ":"),
					[](Context& cur)->NodePtr {
						return cur.labelStmt(cur.stringValue(cur.getSubRange(0)[0].getLexeme()));
					}
			));

			g.addRule("S", rule(
					seq("goto", cap(id), ";"),
					[](Context& cur)->NodePtr {
						return cur.gotoStmt(cur.stringValue(cur.getSubRange(0)[0].getLexeme()));
					},
					1		// higher priority than variable declaration (of type goto)
			));

			g.addRule("S", rule(
					seq("throw", E, ";"),
					[](Context& cur)->NodePtr {
						return cur.throwStmt(cur.getTerm(0).as<ExpressionPtr>());
					},
					1		// higher priority than variable declaration (of type throw)
			));

			// add print statement
			g.addRule("S", rule(
					seq("print(",cap(any(Token::String_Literal)), loop(seq(",", E)),");"),
					[](Context& cur)->NodePtr {
						ExpressionList params = convertList<ExpressionPtr>(cur.getTerms());
						return cur.print(cur.getSubRange(0)[0].getLexeme(), params);
					}
			));

			// -- top level program code --

			g.addRule("A", rule(
					seq(loop(seq(let, ";"), Token::createSymbol(';')),T,"main()", S),
					[](Context& cur)->NodePtr {
						TypePtr returnType = (cur.getTerms().end()-2)->as<TypePtr>();
						StatementPtr body = cur.getTerms().back().as<StatementPtr>();
						ExpressionPtr main = cur.lambdaExpr(returnType, body, VariableList());
						return cur.createProgram(toVector(main));
					}
			));


			// ------------- syntactic sugar for lists -------------

			g.addRule("E", rule(
					seq("[",E,loop(seq(",",E)),"]"),
					[](Context& cur)->NodePtr {
						return encoder::toIR<ExpressionList,encoder::DirectExprListConverter>(cur.manager, convertList<ExpressionPtr>(cur.getTerms()));
					}
			));


			// add productions for unknown node type N
			g.addRule("N", rule(P, forward, -1));	// type parameter have a lower priority than the rest
			g.addRule("N", rule(T, forward));
			g.addRule("N", rule(E, forward));
			g.addRule("N", rule(S, forward));
			g.addRule("N", rule(A, forward));

//			std::cout << g << "\n\n";
//			std::cout << g.getTermInfo() << "\n";

			// initialize term information
			g.getTermInfo();

			return g;
		}

		/**
		 * The mark used to annotate selected regions of the code.
		 */
		class AddressMark {};

		Grammar buildGrammarForAddresses() {
			// start with full grammar
			Grammar g = buildGrammar();

			auto E = rec("E");
			auto S = rec("S");

			// add rules marking addresses
			g.addRule("E", rule(seq("$", E, "$"), [](Context& context)->NodePtr {
				assert(context.getTerms().size() == 1u);
				NodePtr res = context.markerExpr(context.getTerm(0).as<ExpressionPtr>());
				res->attachValue<AddressMark>();
				return res;
			}));

			g.addRule("S", rule(seq("$", S, "$"), [](Context& context)->NodePtr {
				assert(context.getTerms().size() == 1u);
				NodePtr res = context.markerStmt(context.getTerm(0).as<StatementPtr>());
				res->attachValue<AddressMark>();
				return res;
			}));

			// return modified grammar
			return g;
		}

	}

	// The various IR Grammer derivations	(initialized globals to avoid race conditions)
	const Grammar grammar_full 		= buildGrammar();
	const Grammar grammar_types		= buildGrammar("T");
	const Grammar grammar_exprs		= buildGrammar("E");
	const Grammar grammar_stmts		= buildGrammar("S");
	const Grammar grammar_prog		= buildGrammar("A");
	const Grammar grammar_addr		= buildGrammarForAddresses();

	NodePtr parse(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		try {
			return grammar_full.match(manager, code, onFailThrow, definitions);
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return NodePtr();
	}

	TypePtr parse_type(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		try {
			return grammar_types.match(manager, code, onFailThrow, definitions).as<TypePtr>();
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return TypePtr();
	}

	ExpressionPtr parse_expr(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		try {
			return grammar_exprs.match(manager, code, onFailThrow, definitions).as<ExpressionPtr>();
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return ExpressionPtr();
	}

	StatementPtr parse_stmt(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		try {
			return grammar_stmts.match(manager, code, onFailThrow, definitions).as<StatementPtr>();
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return StatementPtr();
	}

	ProgramPtr parse_program(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		try {
			return grammar_prog.match(manager, code, onFailThrow, definitions).as<ProgramPtr>();
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return ProgramPtr();
	}

	namespace {

		struct MarkEliminator : public core::transform::CachedNodeMapping {
			virtual const NodePtr resolveElement(const NodePtr& ptr) {

				// replace recursively
				NodePtr res = ptr->substitute(ptr->getNodeManager(), *this);

				// eliminate marked nodes
				if (ptr->hasAttachedValue<AddressMark>()) {
					// strip off marker expression (also drops annotation)
					if(res->getNodeType() == core::NT_MarkerExpr) {
						return res.as<core::MarkerExprPtr>()->getSubExpression();
					}
					if (res->getNodeType() == core::NT_MarkerStmt) {
						return res.as<core::MarkerStmtPtr>()->getSubStatement();
					}
					assert(false && "Only marker expressions and statements should be marked.");
				}

				// return result
				return res;
			}
		};

		NodePtr removeMarks(const core::NodePtr& cur) {
			return MarkEliminator().map(cur);
		}

		NodeAddress removeMarks(const core::NodePtr& newRoot, const core::NodeAddress& cur) {

			// handle terminal case => address only references a root node
			if (cur.isRoot()) {

				// if root is marked => skip
				if (cur->hasAttachedValue<AddressMark>()) {
					return core::NodeAddress();
				}

				// return new root
				return core::NodeAddress(newRoot);
			}

			// get cleaned path to parent node
			NodeAddress parent = removeMarks(newRoot, cur.getParentAddress());

			// skip marked nodes
			if (cur->hasAttachedValue<AddressMark>()) {
				return parent;
			}

			// see whether this is the first non-marked node along the path
			if (!parent) {
				return core::NodeAddress(newRoot);
			}

			// also fix child of marker node
			if (cur.getDepth() >= 2 && cur.getParentNode()->hasAttachedValue<AddressMark>()) {
				return parent.getAddressOfChild(cur.getParentAddress().getIndex());
			}

			// in all other cases just restore same address path
			return parent.getAddressOfChild(cur.getIndex());
		}

	}


	std::vector<NodeAddress> parse_addresses(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		try {
			// parse the input code (all resulting addresses will be marked)
			NodePtr root = grammar_addr.match(manager, code, onFailThrow, definitions);

			// check the result
			if (!root) return std::vector<NodeAddress>();

			// search all marked locations within the parsed code fragment
			std::vector<NodeAddress> res;
			core::visitDepthFirst(NodeAddress(root), [&](const NodeAddress& cur) {
				if (cur->hasAttachedValue<AddressMark>()) {
					// get address to marked sub-construct
					if(cur->getNodeType() == core::NT_MarkerExpr) {
						res.push_back(cur.as<core::MarkerExprAddress>()->getSubExpression());
					} else if (cur->getNodeType() == core::NT_MarkerStmt) {
						res.push_back(cur.as<core::MarkerStmtAddress>()->getSubStatement());
					} else {
						assert(false && "Only marker expressions and statements should be marked.");
					}
				}
			});

			// remove marks from code
			root = removeMarks(root);

			// remove marker nodes from addresses
			for(NodeAddress& cur : res) {
				cur = removeMarks(root, cur);
			}

			// return list of addresses
			return res;

		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return std::vector<NodeAddress>();
	}


	/**
	 * Builds an instance of the full IR Grammar to be customized
	 * by the user for specific purposes.
	 */
	Grammar createGrammar() {
		return buildGrammar();
	}


} // end namespace parser2
} // end namespace core
} // end namespace insieme
