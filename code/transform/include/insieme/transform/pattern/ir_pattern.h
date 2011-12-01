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

#include <string>
#include <memory>
#include <ostream>
#include <unordered_map>

#include "insieme/core/forward_decls.h"
#include "insieme/transform/pattern/pattern.h"
#include "insieme/core/parser/ir_parse.h"

#include "insieme/utils/logging.h"

namespace insieme {
namespace transform {
namespace pattern {
namespace irp {
	using std::make_shared;

	inline TreePatternPtr atom(const core::NodePtr& node) {
		return pattern::atom(node);
	}

	inline TreePatternPtr atom(core::NodeManager& manager, const char* code) {
		auto a = [&manager] (const string& str) {return core::parse::parseIR(manager, str); };
		return atom(a(string(code)));
	}

	inline TreePatternPtr atom(core::NodeManager& manager, string& code) {
		auto a = [&manager] (const string& str) {return core::parse::parseIR(manager, str); };
		return atom(a(code));
	}

	inline TreePatternPtr wrapBody(const TreePatternPtr& body) {
		return node(core::NT_CompoundStmt, single(body)) | body;
	}


	inline TreePatternPtr genericType(const TreePatternPtr& family, const ListPatternPtr& subtypes = empty, const ListPatternPtr& typeParams = empty) {
		return node(core::NT_GenericType, family << single(node(subtypes)) << single(node(typeParams)));
	}
	inline TreePatternPtr genericType(const core::StringValuePtr& family, const ListPatternPtr& typeParams = empty, const ListPatternPtr& intParams = empty) {
		return genericType(atom(family), typeParams, intParams);
	}
	inline TreePatternPtr genericType(const string& name, const ListPatternPtr& typeParams = empty, const ListPatternPtr& intParams = empty) {
		return genericType(value(name), typeParams, intParams);
	}


	inline TreePatternPtr literal(const TreePatternPtr& type, const TreePatternPtr& value) {
		return node(core::NT_Literal, single(type) << single(value));
	}

	inline TreePatternPtr literal(const TreePatternPtr& type, const core::StringValuePtr& value) {
		return literal(type, atom(value));
	}

	inline TreePatternPtr literal(const TreePatternPtr& type, const string& str) {
		return literal(type, value(str));
	}
	inline TreePatternPtr literal(const string& str) {
		return literal(any, str);
	}


	inline TreePatternPtr tupleType(const ListPatternPtr& pattern) {
		return node(core::NT_TupleType, pattern);
	}

	inline TreePatternPtr variable(const TreePatternPtr& type, const TreePatternPtr& id) {
		return node(core::NT_Variable, single(type) << single(id));
	}

	inline TreePatternPtr callExpr(const TreePatternPtr& type, const TreePatternPtr& function, const ListPatternPtr& parameters) {
		return node(core::NT_CallExpr, type << single(function) << node(core::NT_Expressions, parameters));
	}

	inline TreePatternPtr callExpr(const core::NodePtr& function, const ListPatternPtr& parameters) {
		return callExpr(any, atom(function), parameters);
	}

	inline TreePatternPtr callExpr(const TreePatternPtr& function, const ListPatternPtr& parameters) {
		return callExpr(any, function, parameters);
	}

	inline TreePatternPtr castExpr(const TreePatternPtr& expression) {
		return node(core::NT_CastExpr, single(expression));
	}

	inline TreePatternPtr bindExpr(const ListPatternPtr& parameters, const TreePatternPtr& call) {
		return node(core::NT_BindExpr, parameters << single(call));
	}

	inline TreePatternPtr tupleExpr(const ListPatternPtr& expressions) {
		return node(core::NT_TupleExpr, expressions);
	}

	inline TreePatternPtr vectorExpr(const ListPatternPtr& expressions) {
		return node(core::NT_VectorExpr, expressions);
	}

	inline TreePatternPtr structExpr(const ListPatternPtr& members) {
		return node(core::NT_StructExpr, members);
	}

	inline TreePatternPtr unionExpr(const TreePatternPtr& memberName, const TreePatternPtr& member) {
		return node(core::NT_UnionExpr, single(memberName) << single(member));
	}

	inline TreePatternPtr markerExpr(const TreePatternPtr& subExpression, const TreePatternPtr& id) {
		return node(core::NT_MarkerExpr, single(subExpression) << single(id));
	}

	inline TreePatternPtr lambda(const TreePatternPtr& type, const ListPatternPtr& parameters, const TreePatternPtr& body) {
		return node(core::NT_Lambda, single(type) << single(node(core::NT_Parameters, parameters)) << wrapBody(body));
	}

	inline TreePatternPtr lambdaExpr(const TreePatternPtr& variable, const TreePatternPtr& lambdaDef) {
		return node(core::NT_LambdaExpr, single(any) << single(variable) << single(lambdaDef));
	}

	inline TreePatternPtr lambdaDefinition(const ListPatternPtr& definitions) {
		return node(core::NT_LambdaDefinition, definitions);
	}

	inline TreePatternPtr compoundStmt(const ListPatternPtr& stmts = empty) {
		return node(core::NT_CompoundStmt, stmts);
	}
	inline TreePatternPtr compoundStmt(const TreePatternPtr& stmt) {
		return compoundStmt(single(stmt));
	}

	inline TreePatternPtr declarationStmt(const TreePatternPtr& variable, const TreePatternPtr& initExpr) {
		return node(core::NT_DeclarationStmt, single(variable) << single(initExpr));
	}

	inline TreePatternPtr ifStmt(const TreePatternPtr& condition, const TreePatternPtr& thenBody, const TreePatternPtr& elseBody){
		return node(core::NT_IfStmt, single(condition) << wrapBody(thenBody) << wrapBody(elseBody));
	}


	inline TreePatternPtr forStmt(const TreePatternPtr& iterator, const TreePatternPtr& start,
									  const TreePatternPtr& end, const TreePatternPtr& step,
									  const ListPatternPtr& body )
		{
			return node(core::NT_ForStmt, single(declarationStmt(iterator,start)) <<
										  single(end) << single(step) << compoundStmt(body)
					   );
		}

	inline TreePatternPtr forStmt(const TreePatternPtr& iterator, const TreePatternPtr& start,
								  const TreePatternPtr& end, const TreePatternPtr& step,
								  const TreePatternPtr& body )
	{
		return node(core::NT_ForStmt, single(declarationStmt(iterator,start)) <<
										  single(end) << single(step) << wrapBody(body)
					   );
	}

	inline TreePatternPtr forStmt(){ return node(core::NT_ForStmt, anyList); }

	inline TreePatternPtr whileStmt(const TreePatternPtr& condition, const TreePatternPtr& body){
		return node(core::NT_WhileStmt, single(condition) << wrapBody(body));
	}

	inline TreePatternPtr switchStmt(const TreePatternPtr& expression, const ListPatternPtr& cases, const TreePatternPtr& defaultCase){
		return node(core::NT_SwitchStmt, single(expression) << cases << single(defaultCase));
	}

	inline TreePatternPtr returnStmt(const TreePatternPtr& returnExpression){
		return node(core::NT_ReturnStmt, single(returnExpression));
	}

	inline TreePatternPtr markerStmt(const TreePatternPtr& subExpr, const TreePatternPtr& id){
		return node(core::NT_MarkerStmt, single(subExpr) << single(id));
	}

	const TreePatternPtr continueStmt = node(core::NT_ContinueStmt);
	const TreePatternPtr breakStmt = node(core::NT_BreakStmt);

} // end namespace irp
} // end namespace pattern
} // end namespace transform
} // end namespace insieme