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

#include "insieme/core/parser2/ir_parser.h"

#include <sstream>

#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/parser2/parser.h"

namespace insieme {
namespace core {
namespace parser {


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
		TokenIter findNext(const Grammar::TermInfo& info, const TokenIter begin, const TokenIter& end, const Target& token) {
			vector<Token> parenthese;
			for(TokenIter cur = begin; cur != end; ++cur) {

				// early check to allow searching for open / close tokens
				if (parenthese.empty() && *cur == token) {
					return cur;
				}

				if (info.isLeftParenthese(*cur)) {
					parenthese.push_back(info.getClosingParenthese(*cur));
				}
				if (info.isRightParenthese(*cur)) {
					assert(!parenthese.empty() && parenthese.back() == *cur);
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


		vector<TokenRange> split(const Grammar::TermInfo& info, const TokenRange& range, char sep) {
			assert(!info.isLeftParenthese(Token::createSymbol(sep)));
			assert(!info.isRightParenthese(Token::createSymbol(sep)));

			TokenIter start = range.begin();
			TokenIter end = range.end();

			vector<TokenRange> res;
			TokenIter next = findNext(info, start, end, sep);
			while(next != end) {
				res.push_back(TokenRange(start,next));
				start = next+1;
				next = findNext(info, start, end, sep);
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
			for(const TokenRange& param : split(info, params_block, ',')) {
				// type is one less from the end - by skipping the identifier
				TokenRange typeRange = param - 1;

				// parse type
				TypePtr type = cur.grammar.match(cur, typeRange.begin(), typeRange.end(), "T").as<TypePtr>();

				// add to parameter type list
				params.push_back(type);
			}

			// parse result type
			TokenIter begin = params_block.end() + 3;
			TokenIter resEnd = findNext(info, begin, range.end(), '{');
			if (resEnd == range.end()) {
				resEnd = findNext(info, begin, range.end(), Token::createIdentifier("return"));
			}
			TypePtr resType = cur.grammar.match(cur, begin, resEnd, "T").as<TypePtr>();

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
					const Token& tail = *(defs.front().end()-1);
					if (tail == '}' || tail == ';') {
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
			auto id = cap(identifier);

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
					seq("#", identifier),
					[](Context& cur)->NodePtr {
						const string& name = *(cur.end - 1);
						if (name.size() != 1u) return fail(cur, "int-type-parameter variable name must not be longer than a single character");
						return cur.variableIntTypeParam(name[0]);
					}
			));


			// --------------- add type rules ---------------

			// add type variables
			g.addRule("T", rule(
					seq("'", identifier),
					[](Context& cur)->NodePtr {
						const string& name = *(cur.end - 1);
						return cur.typeVariable(name);
					}
			));

			// add generic type
			g.addRule("T", rule(
					seq(identifier, opt(seq("<", list(P|T, ","), ">"))),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();

						// extract type parameters
						TypeList typeParams;
						auto it = terms.begin();
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

						return cur.genericType(*cur.begin, typeParams, intTypeParams);
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
						return cur.functionType(types, resType, true);
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
						return cur.functionType(types, resType, false);
					}
			));

			// add struct and union types
			struct process_named_type : public detail::actions {
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					TypePtr type = cur.getTerms().back().as<TypePtr>();
					auto iterName = cur.getSubRange(0);
					NamedTypePtr member = cur.namedType(iterName[0], type);
					cur.swap(member);
					cur.popRange();
				}
			};

			static const auto member = std::make_shared<Action<process_named_type>>(seq(T, cap(identifier)));

			g.addRule("T", rule(
					seq("struct {", loop(seq(member, ";")), "}"),
					[](Context& cur)->NodePtr {
						auto& terms = cur.getTerms();
						NamedTypeList members = convertList<NamedTypePtr>(terms);
						return cur.structType(members);
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
					cap(identifier),
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

			// add usere defined literals
			g.addRule("E", rule(
					seq("lit(", cap(any(Token::String_Literal)), ":", T, ")"),
					[](Context& cur)->NodePtr {
						// remove initial and final "" from value
						string value = cur.getSubRange(0).front().getLexeme();
						value = value.substr(1,value.size()-2);
						return cur.literal(cur.getTerm(0).as<TypePtr>(), value);
					}
			));


			// --------------- add expression rules ---------------

			// -- arithmetic expressions --

			g.addRule("E", rule(
					seq(E, "+", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.add(a,b);
					},
					-12
			));

			g.addRule("E", rule(
					seq(E, "-", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.sub(a,b);
					},
					-12
			));

			g.addRule("E", rule(
					seq(E, "*", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.mul(a,b);
					},
					-13
			));

			g.addRule("E", rule(
					seq(E, "/", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.div(a,b);
					},
					-13
			));

			g.addRule("E", rule(
					seq(E, "%", E),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = getOperand(cur, 0);
						ExpressionPtr b = getOperand(cur, 1);
						return cur.mod(a,b);
					},
					-13
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
						return cur.eq(a,b);
					},
					-9
			));


			// -- ref manipulations --

			g.addRule("E", rule(
					seq("var(",E,")"),
					[](Context& cur)->NodePtr {
						return cur.refVar(cur.getTerm(0).as<ExpressionPtr>());
					}
			));

			g.addRule("E", rule(
					seq("new(",E,")"),
					[](Context& cur)->NodePtr {
						return cur.refNew(cur.getTerm(0).as<ExpressionPtr>());
					}
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
						return cur.deref(cur.getTerm(0).as<ExpressionPtr>());
					},
					-14
			));



			// -- vector / array access --

			g.addRule("E", rule(
					seq(E, "[", E, "]"),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = cur.getTerm(0).as<ExpressionPtr>();
						ExpressionPtr b = getOperand(cur, 1);
						if (a->getType()->getNodeType() == NT_RefType) {
							return cur.arrayRefElem(a, b);
						}
						return cur.arraySubscript(a, b);		// works for arrays and vectors
					},
					-15
			));

			// member access
			g.addRule("E", rule(
					seq(E, ".", cap(identifier)),
					[](Context& cur)->NodePtr {
						ExpressionPtr a = cur.getTerm(0).as<ExpressionPtr>();
						if (a->getType()->getNodeType() == NT_RefType) {
							return cur.refMember(a, cur.getSubRange(0)[0]);
						}
						return cur.accessMember(a, cur.getSubRange(0)[0]);
					},
					-15
			));

			// -- parentheses --

			g.addRule("E", rule(seq("(", E, ")"), forward, 15));

			// -- add Variable --
			g.addRule("E", rule(
					cap(identifier),
					[](Context& cur)->NodePtr {
						// simply lookup name within variable manager
						NodePtr res = cur.getVarScopeManager().lookup(cur.getSubRange(0));
						if (res) return res;
						return cur.getSymbolManager().lookup(cur.getSubRange(0));
					},
					1 // higher priority than other rules
			));

			g.addRule("E", rule(
					cap(identifier),
					[](Context& cur)->NodePtr {
						// simply lookup name within variable manager
						auto res = cur.getSymbolManager().lookup(cur.getSubRange(0));
						return dynamic_pointer_cast<ExpressionPtr>(res);
					},
					1    // higher priority than generic type rule
			));

			// -- call expression --
			g.addRule("E", rule(
					seq(E,"(", list(E,","), ")"),
					[](Context& cur)->NodePtr {
						NodeList terms = cur.getTerms();
						// get function
						ExpressionPtr fun = terms.front().as<ExpressionPtr>();
						terms.erase(terms.begin());
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
					cur.getVarScopeManager().add(paramName, param);
					cur.swap(param);	// exchange type with variable
					cur.popRange();		// drop name from stack
				}
			};

			static const auto param = std::make_shared<Action<register_param>>(seq(T, cap(identifier)));

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

			// --------------- add statement rules ---------------

			// every expression is a statement (if terminated by ;)
			g.addRule("S", rule(seq(E,";"), forward));

			// every declaration is a statement
			g.addRule("S", rule(seq(let, ";"), [](Context& cur)->NodePtr { return cur.getNoOp(); }));

			// declaration statement
			g.addRule("S", rule(
					seq(T, cap(identifier), " = ", E, ";"),
					[](Context& cur)->NodePtr {
						TypePtr type = cur.getTerm(0).as<TypePtr>();
						ExpressionPtr value = cur.getTerm(1).as<ExpressionPtr>();
						auto decl = cur.declarationStmt(type, value);
						// register name within variable manager
						cur.getVarScopeManager().add(cur.getSubRange(0), decl->getVariable());
						return decl;
					}
			));

			// declaration statement
			g.addRule("S", rule(
					seq(T, cap(identifier), ";"),
					[](Context& cur)->NodePtr {
						IRBuilder builder(cur.manager);
						TypePtr type = cur.getTerm(0).as<TypePtr>();
						ExpressionPtr value = builder.undefined(type);
						auto decl = builder.declarationStmt(type, value);
						// register name within variable manager
						cur.getVarScopeManager().add(cur.getSubRange(0), decl->getVariable());
						return decl;
					}
			));

			// auto-declaration statement
			g.addRule("S", rule(
					seq("auto", cap(identifier), " = ", E, ";"),
					[](Context& cur)->NodePtr {
						ExpressionPtr value = cur.getTerm(0).as<ExpressionPtr>();
						auto decl = cur.declarationStmt(value->getType(), value);
						// register name within variable manager
						cur.getVarScopeManager().add(cur.getSubRange(0), decl->getVariable());
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

			struct register_var : public detail::actions {
				void accept(Context& cur, const TokenIter& begin, const TokenIter& end) const {
					TypePtr type = cur.getTerm(0).as<TypePtr>();
					auto iterName = cur.getSubRange(0);
					VariablePtr iter = cur.variable(type);
					cur.getVarScopeManager().add(iterName, iter);
					cur.push(iter);
				}
			};

			static const auto iter = std::make_shared<Action<register_var>>(seq(T, cap(identifier)));

			// for loop without step size
			g.addRule("S", rule(
					varScop(seq("for(", iter, "=", E, "..", E, ")", S)),
					[](Context& cur)->NodePtr {
						const auto& terms = cur.getTerms();
						TypePtr type = terms[0].as<TypePtr>();
						VariablePtr iter = terms[1].as<VariablePtr>();
						ExpressionPtr start = terms[2].as<ExpressionPtr>();
						ExpressionPtr end = terms[3].as<ExpressionPtr>();
						StatementPtr body = terms[4].as<StatementPtr>();

						auto& basic = cur.manager.getLangBasic();
						if (!basic.isInt(type)) return fail(cur, "Iterator has to be of integer type!");

						IRBuilder builder(cur.manager);

						// build loop
						ExpressionPtr step = builder.literal(type, "1");
						return cur.forStmt(iter, start, end, step, body);
					}
			));

			// for loop with step size
			g.addRule("S", rule(
					varScop(seq("for(", iter, "=", E, "..", E, ":", E, ")", S)),
					[](Context& cur)->NodePtr {
						const auto& terms = cur.getTerms();
						TypePtr type = terms[0].as<TypePtr>();
						VariablePtr iter = terms[1].as<VariablePtr>();
						ExpressionPtr start = terms[2].as<ExpressionPtr>();
						ExpressionPtr end = terms[3].as<ExpressionPtr>();
						ExpressionPtr step = terms[4].as<ExpressionPtr>();
						StatementPtr body = terms[5].as<StatementPtr>();

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
						IRBuilder builder(cur.manager);
						TypePtr returnType = (cur.getTerms().end()-2)->as<TypePtr>();
						StatementPtr body = cur.getTerms().back().as<StatementPtr>();
						ExpressionPtr main = builder.lambdaExpr(returnType, body, VariableList());
						return builder.createProgram(toVector(main));
					}
			));


			// add productions for unknown node type N
			g.addRule("N", rule(P, forward));
			g.addRule("N", rule(T, forward));
			g.addRule("N", rule(E, forward));
			g.addRule("N", rule(S, forward));
			g.addRule("N", rule(A, forward));

//			std::cout << g << "\n\n";
//			std::cout << g.getTermInfo() << "\n";

			return g;
		}

	}



	NodePtr parse(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		static const Grammar inspire = buildGrammar();
		try {
			return inspire.match(manager, code, onFailThrow, definitions);
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return NodePtr();
	}

	TypePtr parse_type(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		static const Grammar g = buildGrammar("T");
		try {
			return g.match(manager, code, onFailThrow, definitions).as<TypePtr>();
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return TypePtr();
	}

	ExpressionPtr parse_expr(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		static const Grammar g = buildGrammar("E");
		try {
			return g.match(manager, code, onFailThrow, definitions).as<ExpressionPtr>();
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return ExpressionPtr();
	}

	StatementPtr parse_stmt(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		static const Grammar g = buildGrammar("S");
		try {
			return g.match(manager, code, onFailThrow, definitions).as<StatementPtr>();
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return StatementPtr();
	}

	ProgramPtr parse_program(NodeManager& manager, const string& code, bool onFailThrow, const std::map<string, NodePtr>& definitions) {
		static const Grammar g = buildGrammar("A");
		try {
			return g.match(manager, code, onFailThrow, definitions).as<ProgramPtr>();
		} catch (const ParseException& pe) {
			throw IRParserException(pe.what());
		}
		return ProgramPtr();
	}


} // end namespace parser2
} // end namespace core
} // end namespace insieme
