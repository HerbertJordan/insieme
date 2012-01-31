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

#pragma once

#include "insieme/core/forward_decls.h"

#include "insieme/core/ir_pointer.h"
#include "insieme/core/ir_node_traits.h"

#include "insieme/core/ir_node.h"
#include "insieme/core/ir_values.h"
#include "insieme/core/ir_int_type_param.h"
#include "insieme/core/ir_types.h"
#include "insieme/core/ir_expressions.h"
#include "insieme/core/ir_statements.h"
#include "insieme/core/ir_program.h"

namespace insieme {
namespace core {


	class IRBuilder {

		/**
		 * The manager used by this builder to create new nodes.
		 */
		NodeManager& manager;

	public:

		/**
		 * A type used within some signatures mapping variables to values.
		 */
		typedef utils::map::PointerMap<VariablePtr, ExpressionPtr> VarValueMapping;

		/**
		 * Creates a new IR builder working with the given node manager.
		 */
		IRBuilder(NodeManager& manager) : manager(manager) { }

		/**
		 * Obtains a reference to the node manager used by this builder.
		 */
		NodeManager& getNodeManager() const {
			return manager;
		}

		/**
		 * Obtains a reference to the basic generator within the node manager.
		 */
		const lang::BasicGenerator& getLangBasic() const;

		template<typename T, typename ... Children>
		Pointer<const T> get(Children ... child) const {
			return T::get(manager, child ...);
		}

		template<typename T>
		Pointer<const T> get(const NodeList& children) const {
			return T::get(manager, children);
		}

		template<
			NodeType type,
			typename Node = typename to_node_type<type>::type
		>
		Pointer<const Node> get(const NodeList& children) const {
			// use factory method of Node implementation
			return Node::get(manager, children);
		}

		NodePtr get(NodeType type, const NodeList& children) const;


		// --- Imported Standard Factory Methods from Node Types ---

		#include "ir_builder.inl"

		// --- Handle value classes ---

		StringValuePtr stringValue(const char* str) const;
		StringValuePtr stringValue(const string& str) const;

		BoolValuePtr boolValue(bool value) const;
		CharValuePtr charValue(char value) const;
		IntValuePtr intValue(int value) const;
		UIntValuePtr uintValue(unsigned value) const;

		// --- Convenience Utilities ---

		GenericTypePtr genericType(const StringValuePtr& name, const TypeList& typeParams, const IntParamList& intTypeParams) const;

		StructTypePtr structType(const vector<std::pair<StringValuePtr,TypePtr>>& entries) const;
		UnionTypePtr unionType(const vector<std::pair<StringValuePtr,TypePtr>>& entries) const;

		NamedTypePtr namedType(const string& name, const TypePtr& type) const;
		NamedValuePtr namedValue(const string& name, const ExpressionPtr& value) const;

		inline GenericTypePtr volatileType(const TypePtr& type) const {
			return genericType("volatile", {type}, IntParamList());
		}

		TupleExprPtr tupleExpr(const ExpressionList& values) const;
		StructExprPtr structExpr(const StructTypePtr& structType, const vector<NamedValuePtr>& values) const;
		StructExprPtr structExpr(const vector<std::pair<StringValuePtr, ExpressionPtr>>& values) const;
		StructExprPtr structExpr(const vector<NamedValuePtr>& values) const;


		VectorExprPtr vectorExpr(const VectorTypePtr& type, const ExpressionList& values) const;
		VectorExprPtr vectorExpr(const ExpressionList& values) const;

		// creates a program - empty or based on the given entry points
		ProgramPtr createProgram(const ExpressionList& entryPoints = ExpressionList()) const;

		// Function Types
		FunctionTypePtr toPlainFunctionType(const FunctionTypePtr& funType) const;
		FunctionTypePtr toThickFunctionType(const FunctionTypePtr& funType) const;

		// Literals
		LiteralPtr stringLit(const std::string& str) const;
		LiteralPtr intLit(const int val, bool tight = false) const;
		LiteralPtr uintLit(const unsigned int val, bool tight = false) const;
		LiteralPtr integerLit(const int val, bool tight = false) const;
		LiteralPtr boolLit(bool value) const;

		// Support reverse literal construction
		LiteralPtr literal(const std::string& value, const TypePtr& type) const { return literal(type, value); }
		LiteralPtr literal(const StringValuePtr& value, const TypePtr& type) const  { return literal(type, value); }

		// Build undefined initializers
		ExpressionPtr undefined(const TypePtr& type);
		ExpressionPtr undefinedVar(const TypePtr& type);
		ExpressionPtr undefinedNew(const TypePtr& type);

		/**
		 * A factory method for intTypeParam literals.
		 */
		LiteralPtr getIntParamLiteral(unsigned value) const;
		LiteralPtr getIntTypeParamLiteral(const IntTypeParamPtr& param) const;

		/**
		 * A factory method for a identifier literal.
		 */
		LiteralPtr getIdentifierLiteral(const string& value) const;
		LiteralPtr getIdentifierLiteral(const StringValuePtr& value) const;

		/**
		 * A factory method for a type literals.
		 */
		LiteralPtr getTypeLiteral(const TypePtr& type) const;

		/**
		 * A method generating a vector init expression form a scalar.
		 */
		ExpressionPtr scalarToVector(const TypePtr& type, const ExpressionPtr& subExpr) const;

		// Values
		// obtains a zero value - recursively resolved for the given type
		ExpressionPtr getZero(const TypePtr& type) const;

		// Referencing
		CallExprPtr deref(const ExpressionPtr& subExpr) const;
		CallExprPtr refVar(const ExpressionPtr& subExpr) const;
		CallExprPtr refNew(const ExpressionPtr& subExpr) const;
		CallExprPtr assign(const ExpressionPtr& target, const ExpressionPtr& value) const;

		ExpressionPtr invertSign(const ExpressionPtr& subExpr) const;
		// Returns the negation of the passed subExpr (which must be of boolean type)
		// 	       (<BOOL> expr) -> <BOOL> !expr
		ExpressionPtr negateExpr(const ExpressionPtr& subExpr) const;

		// Vectors
		CallExprPtr vectorSubscript(const ExpressionPtr& vec, const ExpressionPtr& index) const;
		CallExprPtr vectorRefElem(const ExpressionPtr& vec, const ExpressionPtr& index) const;
		//CallExprPtr vectorSubscript(const ExpressionPtr& vec, unsigned index) const;
		CallExprPtr vectorInit(const ExpressionPtr& initExp, const IntTypeParamPtr& size) const;

		// Compound Statements
		template<typename ... Nodes>
		CompoundStmtPtr compoundStmt(const Pointer<const Nodes>& ... nodes) const {
			return compoundStmt(toVector<StatementPtr>(nodes...));
		}

		// Declaration Statements
		DeclarationStmtPtr declarationStmt(const TypePtr& type, const ExpressionPtr& value) const;

		// Call Expressions
		CallExprPtr callExpr(const TypePtr& resultType, const ExpressionPtr& functionExpr) const;
		CallExprPtr callExpr(const TypePtr& resultType, const ExpressionPtr& functionExpr, const ExpressionPtr& arg1) const;
		CallExprPtr callExpr(const TypePtr& resultType, const ExpressionPtr& functionExpr, const ExpressionPtr& arg1, const ExpressionPtr& arg2) const;
		CallExprPtr callExpr(const TypePtr& resultType, const ExpressionPtr& functionExpr, const ExpressionPtr& arg1, const ExpressionPtr& arg2, const ExpressionPtr& arg3) const;

		// For the methods below, the return type is deduced from the functionExpr's function type
		CallExprPtr callExpr(const ExpressionPtr& functionExpr, const vector<ExpressionPtr>& arguments = vector<ExpressionPtr>()) const;
		CallExprPtr callExpr(const ExpressionPtr& functionExpr, const ExpressionPtr& arg1) const;
		CallExprPtr callExpr(const ExpressionPtr& functionExpr, const ExpressionPtr& arg1, const ExpressionPtr& arg2) const;
		CallExprPtr callExpr(const ExpressionPtr& functionExpr, const ExpressionPtr& arg1, const ExpressionPtr& arg2, const ExpressionPtr& arg3) const;


		// Lambda Nodes
		LambdaPtr lambda(const FunctionTypePtr& type, const ParametersPtr& params, const StatementPtr& body) const;
		LambdaPtr lambda(const FunctionTypePtr& type, const VariableList& params, const StatementPtr& body) const;

		// Lambda Expressions
		LambdaExprPtr lambdaExpr(const StatementPtr& body, const ParametersPtr& params) const;
		LambdaExprPtr lambdaExpr(const StatementPtr& body, const VariableList& params) const;
		LambdaExprPtr lambdaExpr(const TypePtr& returnType, const StatementPtr& body, const ParametersPtr& params) const;
		LambdaExprPtr lambdaExpr(const TypePtr& returnType, const StatementPtr& body, const VariableList& params) const;
		LambdaExprPtr lambdaExpr(const FunctionTypePtr& type, const VariableList& params, const StatementPtr& body) const;

		BindExprPtr bindExpr(const VariableList& params, const CallExprPtr& call) const;

		template<typename ... Vars>
		ParametersPtr parameters(const Vars& ... vars) const {
			return Parameters::get(manager, toVector<VariablePtr>(vars...));
		}

		// Create a job expression
		JobExprPtr jobExpr(const ExpressionPtr& threadNumRange, const vector<DeclarationStmtPtr>& localDecls, const vector<GuardedExprPtr>& guardedExprs, const ExpressionPtr& defaultExpr) const;

		// Create a marker expression
		MarkerExprPtr markerExpr(const ExpressionPtr& subExpr, unsigned id) const;
		MarkerExprPtr markerExpr(const ExpressionPtr& subExpr, const UIntValuePtr& id) const;
		MarkerStmtPtr markerStmt(const StatementPtr& subExpr, unsigned id) const;
		MarkerStmtPtr markerStmt(const StatementPtr& subExpr, const UIntValuePtr& id) const;

		// Creation of thread number ranges
		CallExprPtr getThreadNumRange(unsigned min) const;
		CallExprPtr getThreadNumRange(unsigned min, unsigned max) const;
		CallExprPtr getThreadNumRange(const ExpressionPtr& min) const;
		CallExprPtr getThreadNumRange(const ExpressionPtr& min, const ExpressionPtr& max) const;

		// Direct call expression of getThreadGroup
		CallExprPtr getThreadGroup(ExpressionPtr level = ExpressionPtr()) const;
		CallExprPtr getThreadGroupSize(ExpressionPtr level = ExpressionPtr()) const;
		CallExprPtr getThreadId(ExpressionPtr level = ExpressionPtr()) const;

		// Direct call expression of barrier
		CallExprPtr barrier(ExpressionPtr threadgroup = ExpressionPtr()) const;

		// Direct call expression of pfor
		CallExprPtr pfor(const ExpressionPtr& body, const ExpressionPtr& start, const ExpressionPtr& end, ExpressionPtr step = ExpressionPtr()) const;

		// Build a Call expression for a pfor that mimics the effect of the given for statement
		CallExprPtr pfor(const ForStmtPtr& initialFor) const;

		/*
		 * creates a function call from a list of expressions
		 */
		ExpressionPtr createCallExprFromBody(StatementPtr body, TypePtr retTy, bool lazy=false) const;

		/**
		 * Creates an expression accessing the corresponding member of the given struct.
		 */
		ExpressionPtr accessMember(const ExpressionPtr& structExpr, const string& member) const;

		/**
		 * Creates an expression accessing the corresponding member of the given struct.
		 */
		ExpressionPtr accessMember(const ExpressionPtr& structExpr, const StringValuePtr& member) const;

		/**
		 * Creates an expression obtaining a reference to a member of a struct.
		 */
		ExpressionPtr refMember(const ExpressionPtr& structExpr, const StringValuePtr& member) const;

		/**
		 * Creates an expression obtaining a reference to a member of a struct.
		 */
		ExpressionPtr refMember(const ExpressionPtr& structExpr, const string& member) const;

		/**
		 * Creates an expression accessing the given component of the given tuple value.
		 */
		ExpressionPtr accessComponent(ExpressionPtr tupleExpr, unsigned component) const;
		ExpressionPtr accessComponent(ExpressionPtr tupleExpr, ExpressionPtr component) const;

		/**
		 * Creates an expression accessing the reference to a component of the given tuple value.
		 */
		ExpressionPtr refComponent(ExpressionPtr tupleExpr, unsigned component) const;
		ExpressionPtr refComponent(ExpressionPtr tupleExpr, ExpressionPtr component) const;

		// Locks
		CallExprPtr acquireLock(const ExpressionPtr& lock) const;
		CallExprPtr releaseLock(const ExpressionPtr& lock) const;
		CallExprPtr createLock() const;

		// Variants
		CallExprPtr pickVariant(const ExpressionList& variants) const;


		/**
		 * A function obtaining a reference to a NoOp instance.
		 */
		CompoundStmtPtr getNoOp() const;

		/**
		 * Tests whether the given node is a no-op.
		 *
		 * @param node the node to be tested
		 * @return true if it is a no-op, false otherwise
		 */
		bool isNoOp(const NodePtr& node) const;
		bool isNoOp(const CompoundStmtPtr& p) const { return p->empty(); }

		IfStmtPtr ifStmt(const ExpressionPtr& condition, const StatementPtr& thenBody, const StatementPtr& elseBody = StatementPtr()) const;
		WhileStmtPtr whileStmt(const ExpressionPtr& condition, const StatementPtr& body) const;
		ForStmtPtr forStmt(const DeclarationStmtPtr& var, const ExpressionPtr& end, const ExpressionPtr& step, const StatementPtr& body) const;
		ForStmtPtr forStmt(const VariablePtr& var, const ExpressionPtr& start, const ExpressionPtr& end, const ExpressionPtr& step, const StatementPtr& body) const;

		SwitchCasePtr switchCase(const LiteralPtr& lit, const StatementPtr& stmt) const { return switchCase(lit, wrapBody(stmt)); };

		SwitchStmtPtr switchStmt(const ExpressionPtr& switchStmt, const vector<std::pair<ExpressionPtr, StatementPtr>>& cases, const StatementPtr& defaultCase = StatementPtr()) const;
		SwitchStmtPtr switchStmt(const ExpressionPtr& switchStmt, const vector<SwitchCasePtr>& cases, const StatementPtr& defaultCase = StatementPtr()) const;



		// ------------------------ Operators ---------------------------

		TypePtr infereExprType(const ExpressionPtr& op, const ExpressionPtr& a) const;
		TypePtr infereExprType(const ExpressionPtr& op, const ExpressionPtr& a, const ExpressionPtr& b) const;

		inline CallExprPtr unaryOp(const ExpressionPtr& op, const ExpressionPtr& a) const {
			return callExpr(infereExprType(op, a), op, a);
		}

		inline CallExprPtr binaryOp(const ExpressionPtr& op, const ExpressionPtr& a, const ExpressionPtr& b) const {
			return callExpr(infereExprType(op, a, b), op, a, b);
		}

		inline ExpressionPtr getOperator(lang::BasicGenerator::Operator op, const TypePtr& a) const {
			return getLangBasic().getOperator(a, op);
		}

		inline ExpressionPtr getOperator(lang::BasicGenerator::Operator op, const TypePtr& a, const TypePtr& b) const {
			// TODO: pick operator based on both operands types!!
			return getLangBasic().getOperator(a, op);
		}

		// unary operators

		inline CallExprPtr bitwiseNeg(const ExpressionPtr& a) const {
			return unaryOp(getOperator(lang::BasicGenerator::Not, a->getType()), a);
		}

		inline CallExprPtr logicNeg(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::LNot, a->getType(), b->getType()), a, b);
		}

		inline ExpressionPtr plus(const ExpressionPtr& a) const {
			return a; // this operator can be skipped
		}

		inline CallExprPtr minus(const ExpressionPtr& a) const {
			return unaryOp(getOperator(lang::BasicGenerator::Minus, a->getType()), a);
		}


		inline CallExprPtr preInc(const ExpressionPtr& a) const {
			return unaryOp(getOperator(lang::BasicGenerator::PreInc, a->getType()), a);
		}

		inline CallExprPtr postInc(const ExpressionPtr& a) const {
			return unaryOp(getOperator(lang::BasicGenerator::PostInc, a->getType()), a);
		}

		inline CallExprPtr preDec(const ExpressionPtr& a) const {
			return unaryOp(getOperator(lang::BasicGenerator::PreDec, a->getType()), a);
		}

		inline CallExprPtr postDec(const ExpressionPtr& a) const {
			return unaryOp(getOperator(lang::BasicGenerator::PostDec, a->getType()), a);
		}


		// binary operators

		inline CallExprPtr add(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Add, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr sub(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Sub, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr mul(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Mul, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr div(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Div, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr mod(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Mod, a->getType(), b->getType()), a, b);
		}


		inline CallExprPtr bitwiseAnd(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::And, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr bitwiseOr(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Or, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr bitwiseXor(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Xor, a->getType(), b->getType()), a, b);
		}


		inline CallExprPtr logicAnd(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::LAnd, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr logicOr(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::LOr, a->getType(), b->getType()), a, b);
		}


		inline CallExprPtr eq(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Eq, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr ne(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Ne, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr lt(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Lt, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr le(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Le, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr gt(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Gt, a->getType(), b->getType()), a, b);
		}

		inline CallExprPtr ge(const ExpressionPtr& a, const ExpressionPtr& b) const {
			return binaryOp(getOperator(lang::BasicGenerator::Ge, a->getType(), b->getType()), a, b);
		}


		/**
		 * Encapsulate the given statement inside a body.
		 */
		CompoundStmtPtr wrapBody(const StatementPtr& stmt) const;

	private:

		unsigned extractNumberFromExpression(ExpressionPtr& expr) const;
	};

	// Utilities

	template<
		typename T,
		typename boost::enable_if<boost::is_base_of<Expression, T>, int>::type = 0
	>
	static TypeList extractTypes(const vector<Pointer<const T>>& exprs) {
		TypeList types;
		std::transform(exprs.cbegin(), exprs.cend(), std::back_inserter(types),
			[](const ExpressionPtr& p) { return p->getType(); });
		return types;
	}


} // namespace core
} // namespace insieme
