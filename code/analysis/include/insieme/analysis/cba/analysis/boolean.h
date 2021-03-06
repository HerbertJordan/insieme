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

#pragma once

#include "insieme/analysis/cba/framework/analysis_type.h"
#include "insieme/analysis/cba/framework/generator/basic_data_flow.h"

#include "insieme/analysis/cba/analysis/arithmetic.h"

#include "insieme/core/lang/basic.h"
#include "insieme/core/arithmetic/arithmetic.h"

namespace insieme {
namespace analysis {
namespace cba {

	using std::set;

	template<typename A, typename B, typename C> class DataFlowConstraintGenerator;

	// ----------------- booleans analysis ---------------

	template<typename C> class BooleanConstraintGenerator;

	struct boolean_analysis_data : public data_analysis<bool, BooleanConstraintGenerator> {};
	struct boolean_analysis_var  : public data_analysis<bool, BooleanConstraintGenerator> {};

	extern const boolean_analysis_data B;
	extern const boolean_analysis_var  b;

	namespace {

		using namespace insieme::core;

		template<
			typename F,
			typename A = typename std::remove_cv<typename std::remove_reference<typename lambda_traits<F>::arg1_type>::type>::type,
			typename B = typename std::remove_cv<typename std::remove_reference<typename lambda_traits<F>::arg2_type>::type>::type,
			typename R = typename lambda_traits<F>::result_type
		>
		struct pair_wise {
			F f;
			pair_wise(const F& f) : f(f) {}
			set<R> operator()(const set<A>& a, const set<B>& b) const {
				set<R> res;
				for(auto& x : a) {
					for(auto& y : b) {
						res.insert(f(x,y));
					}
				}
				return res;
			}
		};

		template<typename F>
		pair_wise<F> pairwise(const F& f) {
			return pair_wise<F>(f);
		}

		template<typename Comparator>
		std::function<set<bool>(const set<Formula>&,const set<Formula>&)> compareFormula(const Comparator& fun) {
			return [=](const set<Formula>& a, const set<Formula>& b)->set<bool> {
				static const set<bool> unknown({true, false});

				set<bool> res;

				// quick check
				for(auto& x : a) if (!x) return unknown;
				for(auto& x : b) if (!x) return unknown;

				// check out pairs
				bool containsTrue = false;
				bool containsFalse = false;
				for(auto& x : a) {
					for(auto& y : b) {
						if (containsTrue && containsFalse) {
							return res;
						}

						// pair< valid, unsatisfiable >
						pair<bool,bool> validity = fun(*x.formula, *y.formula);
						if (!containsTrue && !validity.second) {
							res.insert(true);
							containsTrue = true;
						}

						if (!containsFalse && !validity.first) {
							res.insert(false);
							containsFalse = true;
						}
					}
				}
				return res;
			};
		}

		bool isBooleanSymbol(const ExpressionPtr& expr) {
			auto& gen = expr->getNodeManager().getLangBasic();
			return expr.isa<LiteralPtr>() && !gen.isTrue(expr) && !gen.isFalse(expr);
		}

	}

	template<typename Context>
	class BooleanConstraintGenerator : public DataFlowConstraintGenerator<boolean_analysis_data, boolean_analysis_var, Context> {

		typedef DataFlowConstraintGenerator<boolean_analysis_data, boolean_analysis_var, Context> super;

		const core::lang::BasicGenerator& base;

		CBA& cba;

	public:

		BooleanConstraintGenerator(CBA& cba)
			: super(cba, cba::B, cba::b, cba.getDataManager<lattice<boolean_analysis_data>::type>().atomic(utils::set::toSet<set<bool>>(true, false))),
			  base(cba.getRoot()->getNodeManager().getLangBasic()),
			  cba(cba)
		{ };

		using super::atomic;
		using super::elem;
		using super::pack;

		void visitLiteral(const LiteralInstance& literal, const Context& ctxt, Constraints& constraints) {

			// only interested in boolean literals
			if (!base.isBool(literal->getType())) {
				// default handling for the rest
				super::visitLiteral(literal, ctxt, constraints);
				return;
			}

			// add constraint literal \in A(lit)
			bool isTrue = base.isTrue(literal);
			bool isFalse = base.isFalse(literal);

			auto l_lit = cba.getLabel(literal);

			if (isTrue  || (!isTrue && !isFalse)) constraints.add(elem(true, cba.getVar(B, l_lit, ctxt)));
			if (isFalse || (!isTrue && !isFalse)) constraints.add(elem(false, cba.getVar(B, l_lit, ctxt)));

		}


		void visitCallExpr(const CallExprInstance& call, const Context& ctxt, Constraints& constraints) {

			// check whether it is a literal => otherwise basic data flow is handling it
			auto fun = call->getFunctionExpr();

			// get some labels / ids
			auto B_res = cba.getVar(B, cba.getLabel(call), ctxt);

			// handle unary literals
			if (call.size() == 1u) {

				// support negation
				if (base.isBoolLNot(fun)) {
					auto B_arg = cba.getVar(B, cba.getLabel(call[0]), ctxt);
					constraints.add(subsetUnary(B_arg, B_res, [&](const set<bool>& in)->typename super::value_type {
						set<bool> out;
						for(bool cur : in) out.insert(!cur);
						return this->atomic(out);
					}));
					return;
				}
			}

			// and binary operators
			if (call.size() != 2u) {
				// conduct std-procedure
				super::visitCallExpr(call, ctxt, constraints);
				return;
			}


			// boolean relations
			{
				// get sets for operators
				auto B_lhs = cba.getVar(B, cba.getLabel(call[0]), ctxt);
				auto B_rhs = cba.getVar(B, cba.getLabel(call[1]), ctxt);

				if (base.isBoolEq(fun)) {
					// equality is guaranteed if symbols are identical - no matter what the value is
					if (isBooleanSymbol(call[0]) && isBooleanSymbol(call[1])) {
						constraints.add(elem(call[0].as<ExpressionPtr>() == call[1].as<ExpressionPtr>(), B_res));
					} else {
						constraints.add(subsetBinary(B_lhs, B_rhs, B_res, pack(pairwise([](bool a, bool b) { return a == b; }))));
					}
					return;
				}

				if (base.isBoolNe(fun)) {
					// equality is guaranteed if symbols are identical - no matter what the value is
					if (isBooleanSymbol(call[0]) && isBooleanSymbol(call[1])) {
						constraints.add(elem(call[0].as<ExpressionPtr>() != call[1].as<ExpressionPtr>(), B_res));
					} else {
						constraints.add(subsetBinary(B_lhs, B_rhs, B_res, pack(pairwise([](bool a, bool b) { return a != b; }))));
					}
					return;
				}
			}

			// arithmetic relations
			{
				auto A_lhs = cba.getVar(cba::A, cba.getLabel(call[0]), ctxt);
				auto A_rhs = cba.getVar(cba::A, cba.getLabel(call[1]), ctxt);

				typedef core::arithmetic::Formula F;
				typedef core::arithmetic::Inequality Inequality;		// shape: formula <= 0

				if(base.isSignedIntLt(fun) || base.isUnsignedIntLt(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, B_res, pack(compareFormula([](const F& a, const F& b) {
						// a < b  ... if !(a >= b) = !(b <= a) = !(b-a <= 0)
						Inequality i(b-a);
						return std::make_pair(i.isUnsatisfiable(), i.isValid());
					}))));
					return;
				}

				if(base.isSignedIntLe(fun) || base.isUnsignedIntLe(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, B_res, pack(compareFormula([](const F& a, const F& b) {
						// a <= b ... if (a-b <= 0)
						Inequality i(a-b);
						return std::make_pair(i.isValid(), i.isUnsatisfiable());
					}))));
					return;
				}

				if(base.isSignedIntGe(fun) || base.isUnsignedIntGe(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, B_res, pack(compareFormula([](const F& a, const F& b){
						// a >= b ... if (b <= a) = (b-a <= 0)
						Inequality i(b-a);
						return std::make_pair(i.isValid(), i.isUnsatisfiable());
					}))));
					return;
				}

				if(base.isSignedIntGt(fun) || base.isUnsignedIntGt(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, B_res, pack(compareFormula([](const F& a, const F& b){
						// a > b ... if !(a <= b) = !(a-b <= 0)
						Inequality i(a-b);
						return std::make_pair(i.isUnsatisfiable(), i.isValid());
					}))));
					return;
				}

				if(base.isSignedIntEq(fun) || base.isUnsignedIntEq(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, B_res, pack(compareFormula([](const F& a, const F& b) {
						// just compare formulas (in normal form)
						bool equal = (a==b);
						return std::make_pair(equal, !equal && a.isConstant() && b.isConstant());
					}))));
					return;
				}

				if(base.isSignedIntNe(fun) || base.isUnsignedIntNe(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, B_res, pack(compareFormula([](const F& a, const F& b) {
						// just compare formulas (in normal form)
						bool equal = (a==b);
						return std::make_pair(!equal && a.isConstant() && b.isConstant(), equal);
					}))));
					return;
				}
			}

			// conduct std-procedure
			super::visitCallExpr(call, ctxt, constraints);
			return;
		}

	};

} // end namespace cba
} // end namespace analysis
} // end namespace insieme
