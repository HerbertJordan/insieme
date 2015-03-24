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

#include <cstdlib>
#include <boost/optional.hpp>
#include <iostream>
#include <memory>
#include <vector>

#include "insieme/core/annotations/source_location.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"
#include "insieme/core/ir_address.h"
#include "insieme/transform/polyhedral/scopvisitor.h"
#include "insieme/utils/logging.h"

using namespace insieme::core;
using namespace insieme::transform::polyhedral::novel;

/// The constructor initializes class variables and triggers the visit of all nodes in the program.
SCoPVisitor::SCoPVisitor(const ProgramAddress &node): varstack(std::stack<std::vector<VariableAddress> >()), fornests(0),
	scoplist(node) {
	visit(node);
}

/// Debug routine to print the given node and its immediate children. The node address is the only mandatory
/// argument; the other arguments are a textual description, and a start index and a node count. The start index
/// tells the subroutine where to start printing (0 means the given node itself, 1 means the first child node,
/// n means the n-th child node. The count gives the number of child nodes which should be printed;
/// 0 means print only the given node, 1 means print only one child node, etc.
void SCoPVisitor::printNode(const NodeAddress &node, std::string descr, unsigned int start, int count) {
	// determine total number of child nodes, and adjust count accordingly
	auto children=node.getChildAddresses();
	if (count<0) count=children.size();

	// if requested (start=0), print the node itself with some debug information
	if (start==0) {
		if (!descr.empty()) descr=" ("+descr+")";
		std::cout << node.getNodeType() << descr << ": " << node << std::endl;
		start++;
	}

	// iterate over the requested children to complete the output
	for (unsigned int n=start-1; n<start-1+count; n++)
		std::cout << "\t-" << n << "\t" << children[n].getNodeType() << ": " << *children[n] << std::endl;
	std::cout << std::endl;
}

/// visitNode is the entry point for visiting all statements within a program to search for a SCoP. It will visit
/// all child nodes and call their respective visitor methods.
void SCoPVisitor::visitNode(const NodeAddress &node) {
	// some debug output
	//if (node.getNodeType()==NT_CompoundStmt) printNode(node);

	// visit all children unconditionally
	visitChildren(node);
}

/// Visit all the child nodes of the given node. The given node itself is not taken into account.
/// Hence, this subroutine can only be used in the case you want to traverse all children unconditionally. When
/// visiting single child nodes, you cannot use this function to do the work.
void SCoPVisitor::visitChildren(const NodeAddress &node) {
	for (auto child: node->getChildList()) visit(child);
}

/// Convert the given expression (Literal or CallExpr) to an affine formula, if possible.
/// If this conversion is not possible, return an empty element.
boost::optional<arithmetic::Formula> SCoPVisitor::parseAffine(const ExpressionAddress &addr) {
	boost::optional<arithmetic::Formula> maybe;
	try {
		auto formula=arithmetic::toFormula(addr.getAddressedNode());
		if (formula.isLinear()) maybe=boost::optional<arithmetic::Formula>(formula);
	} catch (...) { }
	return maybe;
}

/// When visiting visitLambdaExpr, we encountered a function call. Apart from declarations, this is another
/// possibility to instantiate variables, so we need to gather all variables from the closure here.
void SCoPVisitor::visitLambdaExpr(const LambdaExprAddress &expr) {
	// after entering a function, we need to allocate a new variable vector on our variable vector stack
	varstack.push(std::vector<VariableAddress>());
	visitChildren(expr);

	// after we have visited all children, we can remove the topmost variable vector from the stack, and
	// decrease the function nesting level again
	varstack.pop();
}

/// visitForStmt describes what should happen when a for stmt is encountered within the program.
/// Is it the outermost for, or is it already nested?
void SCoPVisitor::visitForStmt(const ForStmtAddress &stmt) {
	// when we encounter a for loop, first increase the loop nesting level, and then copy the currently available
	// variables, since variable modifications in the for loop cannot be seen outside the for loop
	fornests++;
	varstack.push(varstack.top());
	auto decl=stmt.getAddressOfChild(0);
	visit(decl); // visit declaration to add variable to the list of known variables
	auto    lb  =parseAffine(decl.getAddressOfChild(1).as<ExpressionAddress>()),
			ub  =parseAffine(stmt.getAddressOfChild(1).as<ExpressionAddress>()),
			incr=parseAffine(stmt.getAddressOfChild(2).as<ExpressionAddress>());
	if (lb && ub && incr) std::cout << "lb: " << *lb << "; ub: " << *ub << "; incr: " << *incr << std::endl;
	else std::cout << "Loop parameters are not affine" << std::endl;

	// process all the children, and — when done — remove the scope modifications and lessen the nesting level again
	printNode(stmt, "for nesting lvl " + std::to_string(fornests-1));
	for (auto i: {1, 2, 3}) visit(stmt.getAddressOfChild(i));
	varstack.pop();
	fornests--;
}

/// Visit lambda parameters so that we can add them to the list of available variables/SCoP parameters.
/// LambdaExpr { FunctionType, Variable, LambdaDefinition {
///                                      LambdaBinding {
///                                      Variable Lambda {
///                                               FunctionType Parameters CompoundStmt }}}}
void SCoPVisitor::visitParameters(const ParametersAddress &node) {
	std::vector<VariableAddress> &vec=varstack.top();
	for (auto c: node.getChildAddresses()) vec.push_back(c.as<VariableAddress>());
	visitChildren(node);
}

/// Visit all the declaration statements so that we can collect variable assignments.
void SCoPVisitor::visitDeclarationStmt(const DeclarationStmtAddress &node) {
	//std::cout << "DeclarationStmt: " << *(node.getAddressOfChild(0)) << std::endl;
	varstack.top().push_back(node.getAddressOfChild(0).as<VariableAddress>());
	visitChildren(node);
}
