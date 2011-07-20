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

/**
 * This include file defines some macros simplifying the specification of operator converters.
 */

/**
 * This macro allows to access an argument of the call to be processed by the formatter. 
 * The indexing starts with 0. The result will be an ExpressionPtr. If there is no such argument,
 * the pointer will be null.
 * 
 * @param N the index of the argument to be obtained 
 */
#define ARG(N) call->getArgument(N)

#define NODE_MANAGER context.getConverter().getNodeManager()
#define C_NODE_MANAGER context.getConverter().getCNodeManager()

#define CONVERT_TYPE(Node) context.getConverter().getStmtConverter().convertType(context, Node)
#define CONVERT_EXPR(Node) context.getConverter().getStmtConverter().convertExpression(context, Node)
#define CONVERT_STMT(Node) context.getConverter().getStmtConverter().convertStatement(context, Node)

#define LANG_BASIC context.getConverter().getNodeManager().getBasicGenerator()

#define GET_TYPE_INFO(Type) context.getConverter().getTypeManager().getTypeInfo(Type)
#define GET_VAR_INFO(Var) context.getVariableManager().getInfo(Var)

/**
 * A macro triggering the deceding into an argument. This will result in the printing of the code
 * required for evaluating the corresponding argument.
 * 
 * @param N the index of the argument to be printed 
 */
#define CONVERT_ARG(N) CONVERT_EXPR(ARG(N))

/**
 * A macro triggering the conversion of the type of the current call.
 */
#define CONVERT_RES_TYPE CONVERT_TYPE(call->getType())

/**
 * Creates a new operator converter conducting the given conversion.
 * 
 * @param FORMAT the function body realizing the formatting 
 */
#define OP_CONVERTER(Conversion) \
		&(((struct { \
			int dummy; \
			static c_ast::ExpressionPtr f(ConversionContext& context, const core::CallExprPtr& call) Conversion \
		 }){0}).f)	