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

#include "insieme/core/arithmetic/arithmetic_utils.h"

#include "insieme/core/ir_node.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/type_utils.h"

#include "insieme/core/transform/node_replacer.h"

#include "insieme/utils/numeric_cast.h"

#include "insieme/core/analysis/ir_utils.h"

namespace insieme {
namespace core {
namespace arithmetic {

NotAFormulaException::NotAFormulaException(const ExpressionPtr& expr) :expr(expr) {
	if (!expr) { return; }

	std::ostringstream ss;
	ss << "Cannot convert expression '" << *expr << "'" << " of type " << *expr->getType() << " - it is not a formula!";
	msg = ss.str();
}

const char* NotAFormulaException::what() const throw() {	
	return msg.c_str(); 
}

namespace {

	class FormulaConverter : public IRVisitor<Formula> {

		const lang::BasicGenerator& lang;

	public:

		FormulaConverter(const lang::BasicGenerator& lang) : IRVisitor(false), lang(lang) {}

	protected:

		Formula visitLiteral(const LiteralPtr& cur) {
			checkType(cur);
			return utils::numeric_cast<int>(cur->getValue()->getValue());
		}

		Formula visitVariable(const VariablePtr& cur) {
			checkType(cur);
			return cur;
		}

		Formula visitCallExpr(const CallExprPtr& call) {
			checkType(call);

			// check for terminal values
			if (Value::isValue(call)) {
				return Value(call);
			}

			// check number of arguments
			if (call->getArguments().size() != static_cast<std::size_t>(2)) {
				throw NotAFormulaException(call);
			}

			// check function
			ExpressionPtr fun = call->getFunctionExpr();

			// special handling of division

			// handle remaining operators as usual
			Formula a = visit(call->getArgument(0));
			Formula b = visit(call->getArgument(1));

			if (lang.isSignedIntAdd(fun) || lang.isUnsignedIntAdd(fun)) {
				return a + b;
			}
			if (lang.isSignedIntSub(fun) || lang.isUnsignedIntSub(fun)) {
				return a - b;
			}
			if (lang.isSignedIntMul(fun) || lang.isUnsignedIntMul(fun)) {
				return a * b;
			}
			if (lang.isSignedIntDiv(fun) || lang.isUnsignedIntDiv(fun)) {
				if (b.getTerms().size() > static_cast<std::size_t>(1)) {
					throw NotAFormulaException(call);
				}
				return a / b.getTerms()[0];
			}

			// no supported formula
			throw NotAFormulaException(call);
		}

		Formula visitCastExpr(const CastExprPtr& cur) {
			checkType(cur);
			return visit(cur->getSubExpression());
		}

		Formula visitExpression(const ExpressionPtr& cur) {
			throw NotAFormulaException(cur);
		}

		Formula visitNode(const NodePtr& cur) {
			throw NotAFormulaException(ExpressionPtr());
		}

	private:

		void checkType(const ExpressionPtr& expr) {
			// check that current expression is a integer expression
			if (!lang.isInt(expr->getType())) {
				throw NotAFormulaException(expr);
			}
		}

	};

} // end anonumous namespace


Formula toFormula(const ExpressionPtr& expr) {
	// the magic is done by the formula converter
	return FormulaConverter(expr->getNodeManager().getLangBasic()).visit(expr);
}

namespace {

	class PiecewiseConverter : public IRVisitor<Piecewise> {

		FormulaConverter formulaConverter;

		const lang::BasicGenerator& lang;

	public:

		PiecewiseConverter(const lang::BasicGenerator& lang)
			: IRVisitor(false), formulaConverter(lang), lang(lang) {}

		Piecewise visit(const NodePtr& node) {
			try {

				// try whether it is a regular formula
				return formulaConverter.visit(node);

			} catch (const NotAFormulaException& nafe) {
				// convert pieces => using visitor mechanism
			}

			return IRVisitor<Piecewise>::visit(node);
		}

	protected:

		Piecewise visitCallExpr(const CallExprPtr& call) {
			checkType(call);

			// it can be assumed that it is not a formula (yet the type is correct)

			// check function
			ExpressionPtr fun = call->getFunctionExpr();

			// process selects
			if (lang.isSelect(fun)) {
				assert(call->getArguments().size() ==3u);

				// arguments must be formulas
				Piecewise a = visit(call->getArgument(0));
				Piecewise b = visit(call->getArgument(1));

				if (!(a.isFormula() && b.isFormula())) {
					throw NotAPiecewiseException(call);
				}

				Formula fa = a.toFormula();
				Formula fb = b.toFormula();

				Constraint c;
				const auto& pred = call->getArgument(2);
				if (lang.isSignedIntLt(pred) || lang.isUnsignedIntLt(pred)) {
					c = fa < fb;
				} else if (lang.isSignedIntLe(pred) || lang.isUnsignedIntLe(pred)) {
					c = fa <= fb;
				} else if (lang.isSignedIntGt(pred) || lang.isUnsignedIntGt(pred)) {
					c = fa > fb;
				} else if (lang.isSignedIntGe(pred) || lang.isUnsignedIntGe(pred)) {
					c = fa >= fb;
				} else if (lang.isSignedIntEq(pred) || lang.isUnsignedIntEq(pred)) {
					c = eq(fa, fb);
				} else if (lang.isSignedIntNe(pred) || lang.isUnsignedIntNe(pred)) {
					c = ne(fa, fb);
				} else {
					assert(false && "Unsupported select-predicate encountered!");
				}

				// create resulting piecewise formula
				return Piecewise(c, fa, fb);
			}

			// handle remaining integer operators as usual
			Piecewise a = visit(call->getArgument(0));
			Piecewise b = visit(call->getArgument(1));

			if (lang.isSignedIntAdd(fun) || lang.isUnsignedIntAdd(fun)) {
				return a + b;
			}
			if (lang.isSignedIntSub(fun) || lang.isUnsignedIntSub(fun)) {
				return a - b;
			}
			if (lang.isSignedIntMul(fun) || lang.isUnsignedIntMul(fun)) {
				return a * b;
			}

			// no supported formula
			throw NotAPiecewiseException(call);
		}

		Piecewise visitCastExpr(const CastExprPtr& cur) {
			checkType(cur);
			return visit(cur->getSubExpression());
		}

		Piecewise visitNode(const NodePtr& cur) {
			throw NotAPiecewiseException(ExpressionPtr());
		}

	private:

		void checkType(const ExpressionPtr& expr) {
			// check that current expression is a integer expression
			if (!lang.isInt(expr->getType())) {
				throw NotAPiecewiseException(expr);
			}
		}

	};

} // end anonumous namespace

Piecewise toPiecewise(const ExpressionPtr& expr) {
	// the magic is done by the piecewise converter
	return PiecewiseConverter(expr->getNodeManager().getLangBasic()).visit(expr);
}

namespace {

	const TypePtr getBiggerType(const TypePtr& a, const TypePtr& b) {
		assert(a->getNodeManager().getLangBasic().isInt(a) && "Has to be a IntegerType!");
		assert(b->getNodeManager().getLangBasic().isInt(b) && "Has to be a IntegerType!");

		// quick exit
		if (*a == *b) {
			return a;
		}

		const GenericTypePtr& t1 = static_pointer_cast<const GenericType>(a);
		const GenericTypePtr& t2 = static_pointer_cast<const GenericType>(b);
		const IntTypeParamPtr& p1 = t1->getIntTypeParameter()[0];
		const IntTypeParamPtr& p2 = t2->getIntTypeParameter()[0];

		// if both are signed / unsigned
		if (t1->getFamilyName() == t2->getFamilyName()) {
			return (*p1<*p2)?b:a;
		}

		// for the rest => use generic solution
		TypePtr res = getSmallestCommonSuperType(a, b);
		assert(!a->getNodeManager().getLangBasic().isUnit(res) && "Invalid arguments passed to function!");
		return res;
	}

	ExpressionPtr createCall(IRBuilder& builder, const ExpressionPtr& fun, const ExpressionPtr& a, const ExpressionPtr& b) {
		return builder.callExpr(getBiggerType(a->getType(), b->getType()), fun, a, b);
	}

}


ExpressionPtr toIR(NodeManager& manager, const Rational& value) {

	IRBuilder builder(manager);

	// check for pure integer
	if (value.isInteger()) {
		// there is no denominator therefore we can return the integer numerator
		return builder.intLit( value.getNumerator() );
	}

	return builder.div(
			builder.intLit(value.getNumerator()),
			builder.intLit(value.getDenominator())
	);
}

ExpressionPtr toIR(NodeManager& manager, const Product::Factor& factor) {
	assert(factor.second != 0 && "Factor's exponent must not be 0!");

	IRBuilder builder(manager);
	const lang::BasicGenerator& basic = manager.getLangBasic();
	auto MulOp = basic.getSignedIntMul();

	// determine absolute exponent
	int exponent = factor.second;
	if (exponent < 0) {
		exponent = -exponent;
	}

	// handle value
	ExpressionPtr res = factor.first;
	if (analysis::isRefType(res->getType())) {
		res = builder.deref(res);
	}

	// handle exponent
	for (int i=1; i<exponent; ++i) {
		res = createCall(builder, MulOp, res, factor.first);
	}

	// handle negative exponent
	if (factor.second < 0) {
		auto DivOp = basic.getSignedIntDiv();
		auto one = builder.intLit(1);
		return createCall(builder, DivOp, one, res);
	}

	// done
	return res;

}

ExpressionPtr toIR(NodeManager& manager, const Product& product) {

	IRBuilder builder(manager);
	const lang::BasicGenerator& basic = manager.getLangBasic();
	auto MulOp = basic.getSignedIntMul();

	if (product.isOne()) {
		return builder.intLit(1);
	}

	auto it = product.getFactors().begin();
	auto end = product.getFactors().end();
	assert(it != end && "Must not be empty!");

	// append first
	ExpressionPtr res = toIR(manager, *it);
	++it;

	// append rest
	for_each(it, end, [&](const Product::Factor& cur) {
		res = createCall(builder, MulOp, res, toIR(manager, cur));
	});

	// done
	return res;
}

ExpressionPtr toIR(NodeManager& manager, const Formula& formula) {

	IRBuilder builder(manager);
	const lang::BasicGenerator& basic = manager.getLangBasic();

	auto AddOp = basic.getSignedIntAdd();
	auto SubOp = basic.getSignedIntSub();
	auto MulOp = basic.getSignedIntMul();


	if (formula.isZero()) {
		return builder.intLit(0);
	}

	auto it = formula.getTerms().begin();
	auto end = formula.getTerms().end();

	ExpressionPtr res;
	if (it->second.isOne() && it->first.isOne()) {
		res = builder.intLit(1);
	} else if (it->second == Rational(1)) {
		res = toIR(manager, it->first);
	} else if (it->first.isOne()) {
		res = toIR(manager, it->second);
	} else {
		res = createCall(builder, MulOp, toIR(manager, it->second), toIR(manager, it->first));
	}
	++it;

	for_each(it, end, [&](const Formula::Term& cur) {

		Rational factor;
		ExpressionPtr op;
		if (cur.second.getNumerator() > 0) {
			factor = cur.second;
			op = AddOp;
		} else {
			factor = -cur.second;
			op = SubOp;
		}

		if (factor.isOne() && cur.first.isOne()) {
			res = createCall(builder, op, res, builder.intLit(1));
		} else if (factor.isOne()) {
			res = createCall(builder, op, res, toIR(manager, cur.first));
		} else if (cur.first.isOne()) {
			res = createCall(builder, op, res, toIR(manager, factor));
		} else {
			res = createCall(builder, op, res, createCall(builder, MulOp, toIR(manager, factor), toIR(manager, cur.first)));
		}
	});

	// done
	return res;
}

ExpressionPtr toIR(NodeManager& manager, const Constraint& constraint) {
	IRBuilder builder(manager);

	// deal with valid and unsatisfiable
	if (constraint.isValid()) {
		return builder.boolLit(true);
	}
	if (constraint.isUnsatisfiable()) {
		return builder.boolLit(false);
	}

	// obtain zero for comparison
	ExpressionPtr zero = toIR(manager, Formula());

	// just use DNF to produce the corresponding IR structure
	ExpressionPtr res;
	for_each(constraint.toDNF(), [&](const Constraint::Conjunction& conjunct) {
		ExpressionPtr product;

		for_each(conjunct, [&](const Constraint::Literal& lit) {

			// convert literal
			ExpressionPtr cur = builder.le(toIR(manager, lit.first.getFormula()), zero);

			// negate if necessary
			if (!lit.second) { cur = builder.logicNeg(cur); }

			// combine literals
			product = (product) ? builder.logicAnd(product, builder.wrapLazy(cur)) : cur;
		});

		assert(product && "There should not be an empty conjunction!");

		// combine conjunctions
		res = (res) ? builder.logicOr(res, builder.wrapLazy(product)) : product;
	});

	assert(res && "There should not be an empty DNF!");

	// return result
	return res;

}

namespace {

	typedef vector<Piecewise::Piece>::const_iterator piece_iterator;

	ExpressionPtr convertPieceList(const IRBuilder& builder, piece_iterator cur, piece_iterator end) {
		NodeManager& manager = builder.getNodeManager();

		// check whether it is done
		if (cur + 1 == end) {
			// this is the last piece, so the condition can be skipped
			return toIR(manager, cur->second);
		}

		// process current piece and rest
		return builder.ite(
				toIR(manager, cur->first),
				builder.wrapLazy(toIR(manager, cur->second)),
				builder.wrapLazy(convertPieceList(builder, cur+1, end))
		);
	}

}


ExpressionPtr toIR(NodeManager& manager, const Piecewise& piecewise) {

	// handle formulas more directly
	if (piecewise.isFormula()) {
		return toIR(manager, piecewise.toFormula());
	}

	// convert piecewise step by step
	auto start = piecewise.getPieces().begin();
	auto end = piecewise.getPieces().end();
	assert(start != end && "Should not be empty if it is not a formula!");
	return convertPieceList(IRBuilder(manager), start, end);
}


} // end namespace arithmetic
} // end namespace core
} // end namespace insieme
