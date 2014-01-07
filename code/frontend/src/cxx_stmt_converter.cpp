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

#include "insieme/frontend/stmt_converter.h"

#include "insieme/frontend/utils/source_locations.h"
#include "insieme/frontend/analysis/loop_analyzer.h"
#include "insieme/frontend/ocl/ocl_compiler.h"
#include "insieme/frontend/utils/ir_cast.h"
#include "insieme/frontend/utils/debug.h"
#include "insieme/frontend/utils/macros.h"

#include "insieme/frontend/pragma/insieme.h"
#include "insieme/frontend/omp/omp_pragma.h"
#include "insieme/frontend/mpi/mpi_pragma.h"

#include "insieme/utils/container_utils.h"
#include "insieme/utils/logging.h"

#include "insieme/core/lang/ir++_extension.h"
#include "insieme/core/ir_statements.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/ir++_utils.h"

#include "insieme/annotations/ocl/ocl_annotations.h"

#include "insieme/core/transform/node_replacer.h"

using namespace clang;

namespace insieme {
namespace frontend {
namespace conversion {

//---------------------------------------------------------------------------------------------------------------------
//							CXX STMT CONVERTER -- takes care of CXX nodes and C nodes with CXX code mixed in
//---------------------------------------------------------------------------------------------------------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//							DECLARATION STATEMENT
// 			In clang a declstmt is represented as a list of VarDecl
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
stmtutils::StmtWrapper Converter::CXXStmtConverter::VisitDeclStmt(clang::DeclStmt* declStmt) {
	return StmtConverter::VisitDeclStmt(declStmt);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//							RETURN STATEMENT
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
stmtutils::StmtWrapper Converter::CXXStmtConverter::VisitReturnStmt(clang::ReturnStmt* retStmt) {

	stmtutils::StmtWrapper stmt = StmtConverter::VisitReturnStmt(retStmt);
	LOG_STMT_CONVERSION(retStmt, stmt);

	if(!retStmt->getRetValue() ) {
		//if there is no return value its an empty return "return;"
		return stmt;
	}

	core::ExpressionPtr retExpr = stmt.getSingleStmt().as<core::ReturnStmtPtr>().getReturnExpr();

	// check if the return must be converted or not to a reference,
	// is easy to check if the value has being derefed or not
	if (gen.isPrimitive(retExpr->getType())){
			return stmt;
	}

	// return by value ALWAYS, will fix this in a second pass (check cpp_ref plugin)
	//   - var(undefined (Obj))    
	//   - ctor(undefined(Obj))
	//   - vx (where type is ref<Obj<..>>)
	//   		all this casses, by value!
	//   all of those cases result in an expression typed ref<obj>
	if (retExpr->getType().isa<core::RefTypePtr>()){
		if (core::analysis::isObjectType(retExpr->getType().as<core::RefTypePtr>()->getElementType())){
			vector<core::StatementPtr> stmtList;
			stmtList.push_back(builder.returnStmt(builder.deref(retExpr)));
			core::StatementPtr retStatement = builder.compoundStmt(stmtList);
			stmt = stmtutils::tryAggregateStmts(builder,stmtList );
		}
	}

	return stmt;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//							COMPOUND STATEMENT
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
stmtutils::StmtWrapper Converter::CXXStmtConverter::VisitCompoundStmt(clang::CompoundStmt* compStmt) {

	return StmtConverter::VisitCompoundStmt(compStmt);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
stmtutils::StmtWrapper Converter::CXXStmtConverter::VisitCXXCatchStmt(clang::CXXCatchStmt* catchStmt) {
	frontend_assert(false && "Catch -- Taken care of inside of TryStmt!");
	return stmtutils::StmtWrapper();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
stmtutils::StmtWrapper Converter::CXXStmtConverter::VisitCXXTryStmt(clang::CXXTryStmt* tryStmt) {

	//frontend_assert(false && "Try -- Currently not supported!");
	core::CompoundStmtPtr body = builder.wrapBody( stmtutils::tryAggregateStmts( builder, Visit(tryStmt->getTryBlock()) ) );

	vector<core::CatchClausePtr> catchClauses;
	unsigned numCatch = tryStmt->getNumHandlers();
	for(unsigned i=0;i<numCatch;i++) {
		clang::CXXCatchStmt* catchStmt = tryStmt->getHandler(i);

		core::VariablePtr var;
		if(const clang::VarDecl* exceptionVarDecl = catchStmt->getExceptionDecl() ) {
			core::TypePtr exceptionTy = convFact.convertType(catchStmt->getCaughtType().getTypePtr());


			var = builder.variable(exceptionTy);

			//we assume that exceptionVarDecl is not in the varDeclMap
			frontend_assert(convFact.varDeclMap.find(exceptionVarDecl) == convFact.varDeclMap.end()
					&& "excepionVarDecl already in vardeclmap");
			//insert var to be used in conversion of handlerBlock
			convFact.varDeclMap.insert( { exceptionVarDecl, var } );
			VLOG(2) << convFact.lookUpVariable(catchStmt->getExceptionDecl()).as<core::VariablePtr>();
		}
		else {
			//no exceptiondecl indicates a catch-all (...)
			var = builder.variable(gen.getAny());
		}

		core::CompoundStmtPtr body = builder.wrapBody(stmtutils::tryAggregateStmts(builder, Visit(catchStmt->getHandlerBlock())));
		catchClauses.push_back(builder.catchClause(var, body));
	}

	return stmtutils::tryAggregateStmt(builder, builder.tryCatchStmt(body, catchClauses));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
stmtutils::StmtWrapper Converter::CXXStmtConverter::VisitCXXForRangeStmt(clang::CXXForRangeStmt* frStmt) {
	frontend_assert(false && "ForRange -- Currently not supported!");
	return stmtutils::StmtWrapper();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Overwrite the basic visit method for expression in order to automatically
// and transparently attach annotations to node which are annotated
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
stmtutils::StmtWrapper Converter::CXXStmtConverter::Visit(clang::Stmt* stmt) {
	VLOG(2) << "CXX";

    //iterate clang handler list and check if a handler wants to convert the stmt
    stmtutils::StmtWrapper retStmt;
	for(auto plugin : convFact.getConversionSetup().getPlugins()) {
        retStmt = plugin->Visit(stmt, convFact);
		if(retStmt.size())
			break;
	}
    if(retStmt.size()==0){
		convFact.trackSourceLocation(stmt->getLocStart());
        retStmt = StmtVisitor<CXXStmtConverter, stmtutils::StmtWrapper>::Visit(stmt);
	}

	// print diagnosis messages
	convFact.printDiagnosis(stmt->getLocStart());

	// build the wrapper for single statements
	if ( retStmt.isSingleStmt() ) {
		core::StatementPtr&& irStmt = retStmt.getSingleStmt();

		// Deal with mpi pragmas
		mpi::attachMPIStmtPragma(irStmt, stmt, convFact);

		// Deal with transfromation pragmas
		pragma::attachPragma(irStmt,stmt,convFact);

		// Deal with omp pragmas
		if ( irStmt->getAnnotations().empty() )
            retStmt = omp::attachOmpAnnotation(irStmt, stmt, convFact);
	}

    // call frontend plugin post visitors
    for(auto plugin : convFact.getConversionSetup().getPlugins()) {
        retStmt = plugin->PostVisit(stmt, retStmt, convFact);
    }

	return  retStmt;
}

}
}
}
