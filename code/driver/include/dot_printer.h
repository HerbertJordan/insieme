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

#include "ast_visitor.h"
#include "ast_check.h"
#include "numeric_cast.h"
#include "string_utils.h"

#include <ostream>
#include <sstream>

using namespace insieme::core;

namespace insieme {

template <class NodeIdTy>
class GraphBuilder {
public:
	class Node {
		NodeIdTy id;
	public:
		Node(const NodeIdTy& id): id(id) { }
		const NodeIdTy& getId() const { return id; }
	};

	class Link {
		NodeIdTy src;
		NodeIdTy dest;
	public:
		Link(const NodeIdTy& src, const NodeIdTy& dest) : src(src), dest(dest) { }

		const NodeIdTy& getSrc() const { return src; }
		const NodeIdTy& getDest() const { return dest; }
	};

	template <class ProperyIdTy, class PropertyValueTy>
	class Decoration : public std::map<const ProperyIdTy, PropertyValueTy> { };

	virtual const void addNode(const Node& node) = 0;

	virtual void addLink(const Link& link) = 0;
};

template <class NodeIdTy>
class GraphPrinter: public ASTVisitor<> {
	std::ostream&			out;
	const MessageList& 		errors;
	GraphBuilder<NodeIdTy>*	builder;
public:
	GraphPrinter(const MessageList& errors, std::ostream& out);

	static NodeIdTy getNodeId(const core::NodePtr& node);

	void visitGenericType(const core::GenericTypePtr& genTy);
	void visitFunctionType(const FunctionTypePtr& funcType);
	void visitTupleType(const TupleTypePtr& tupleTy);
	void visitNamedCompositeType(const NamedCompositeTypePtr& compTy);

	void visitCompoundStmt(const CompoundStmtPtr& comp);
	void visitForStmt(const ForStmtPtr& forStmt);
	void visitIfStmt(const IfStmtPtr& ifStmt);
	void visitWhileStmt(const WhileStmtPtr& whileStmt);
	void visitDeclarationStmt(const DeclarationStmtPtr& declStmt);
	void visitReturnStmt(const ReturnStmtPtr& retStmt);

	void visitLambdaExpr(const LambdaExprPtr& lambdaExpr);
	void visitVariable(const VariablePtr& var);
	void visitCallExpr(const CallExprPtr& callExpr);
	void visitLiteral(const LiteralPtr& lit);
	void visitCastExpr(const CastExprPtr& castExpr);

	void visitStatement(const insieme::core::StatementPtr& stmt);
	void visitNode(const insieme::core::NodePtr& node);

	void visitProgram(const insieme::core::ProgramPtr& root);
};

void printDotGraph(const insieme::core::NodePtr& root, const MessageList& errors, std::ostream& out);

}
