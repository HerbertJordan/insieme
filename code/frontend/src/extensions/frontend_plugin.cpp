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

#include "insieme/frontend/extensions/frontend_plugin.h"
#include "insieme/frontend/clang.h"
#include "insieme/frontend/stmt_converter.h"

namespace insieme {
namespace frontend {
namespace extensions {

    // ############ PRE CLANG STAGE ############ //
    const FrontendPlugin::macroMap& FrontendPlugin::getMacroList() const {
        return macros;
    }

    const FrontendPlugin::headerVec& FrontendPlugin::getInjectedHeaderList() const {
        return injectedHeaders;
    }

    const FrontendPlugin::headerVec& FrontendPlugin::getKidnappedHeaderList() const {
        return kidnappedHeaders;
    }

    // ############ CLANG STAGE ############ //
    insieme::core::ExpressionPtr FrontendPlugin::Visit(const clang::Expr* expr, insieme::frontend::conversion::Converter& convFact) {
        return nullptr;
    }

    insieme::core::TypePtr FrontendPlugin::Visit(const clang::Type* type, insieme::frontend::conversion::Converter& convFact) {
        return nullptr;
    }

    stmtutils::StmtWrapper FrontendPlugin::Visit(const clang::Stmt* stmt, insieme::frontend::conversion::Converter& convFact) {
        return stmtutils::StmtWrapper();
    }

    bool FrontendPlugin::Visit(const clang::Decl* decl, insieme::frontend::conversion::Converter& convFact) {
        return false;
    }

    insieme::core::ExpressionPtr FrontendPlugin::PostVisit(const clang::Expr* expr, const insieme::core::ExpressionPtr& irExpr,
                                                           insieme::frontend::conversion::Converter& convFact) {
        return irExpr;
    }

    insieme::core::TypePtr FrontendPlugin::PostVisit(const clang::Type* type, const insieme::core::TypePtr& irType,
                                                     insieme::frontend::conversion::Converter& convFact) {
        return irType;
    }

    stmtutils::StmtWrapper FrontendPlugin::PostVisit(const clang::Stmt* stmt, const stmtutils::StmtWrapper& irStmt,
                                                     insieme::frontend::conversion::Converter& convFact) {
        return irStmt;
    }

    // ############ POST CLANG STAGE ############ //
    insieme::core::ProgramPtr FrontendPlugin::IRVisit(insieme::core::ProgramPtr& prog) {
        return prog;
    }

    insieme::frontend::tu::IRTranslationUnit FrontendPlugin::IRVisit(insieme::frontend::tu::IRTranslationUnit& tu) {
        return tu;
    }

}
}
}