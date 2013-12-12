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

#include <gtest/gtest.h>

#include <sstream>

#include "insieme/backend/sequential/sequential_backend.h"

#include "insieme/frontend/stmt_converter.h"
#include "insieme/frontend/expr_converter.h"
#include "insieme/frontend/type_converter.h"

#include "insieme/core/ir_program.h"
#include "insieme/core/checks/full_check.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/frontend/program.h"
#include "insieme/frontend/clang_config.h"
#include "insieme/frontend/convert.h"
#include "insieme/frontend/pragma/insieme.h"

#include "insieme/frontend/extensions/frontend_plugin.h"

using namespace insieme;

bool typeVisited = false;
bool exprVisited = false;
bool stmtVisited = false;
bool postTypeVisited = false;
bool postExprVisited = false;
bool postStmtVisited = false;
bool tuVisited = false;
bool progVisited = false;

insieme::core::NodeManager manager;

class ClangTestPlugin : public insieme::frontend::extensions::FrontendPlugin {
public:
    ClangTestPlugin(int N=0) {
        if(N==0) {
            macros.insert(std::make_pair<std::string,std::string>("A","char *rule_one = \"MOOSI_FOR_PRESIDENT\""));
            injectedHeaders.push_back("injectedHeader.h");
        }
        if(N==1) {
            macros.insert(std::make_pair<std::string,std::string>("A",""));
            kidnappedHeaders.push_back(SRC_DIR "/inputs/kidnapped");
        }
        if(N==2) {
            using namespace insieme::frontend::pragma;
            using namespace insieme::core;

            macros.insert(std::make_pair<std::string,std::string>("A",""));
            injectedHeaders.push_back("injectedHeader.h");

            auto var_list = tok::var >> *(~tok::comma >> tok::var);

            node&& x =  var_list["private"] >> kwd("num_threads") >> tok::l_paren >>
                        tok::expr["num_threads"] >> tok::r_paren >> tok::eod;

            node&& y =  kwd("auto") >> tok::eod;

            //pragma that should be matched: #pragma te loop x num_threads(x*2)
            auto a = insieme::frontend::extensions::PragmaHandler("te", "loop", x,
                            [](MatchObject object, NodePtr node) {
                                EXPECT_TRUE(object.getVars("private").size() && object.getExprs("num_threads").size());
                                EXPECT_TRUE(object.getVars("private")[0]);
                                EXPECT_TRUE(object.getExprs("num_threads")[0]);
                                LiteralPtr literal = Literal::get(manager, manager.getLangBasic().getInt4(), "15");
                                ReturnStmtPtr stmt = ReturnStmt::get(manager, literal);
                                return stmt;
                            });
            //pragma that should be matched: #pragma te scheduling auto
            auto b = insieme::frontend::extensions::PragmaHandler("te", "scheduling", y,
                            [](MatchObject object, NodePtr node) {
                                EXPECT_EQ ("return 15", toString(*node));
                                LiteralPtr literal = Literal::get(manager, manager.getLangBasic().getInt4(), "12");
                                ReturnStmtPtr stmt = ReturnStmt::get(manager, literal);
                                return stmt;
                            });
            //pragma that should be matched: #pragma te barrier
            auto c = insieme::frontend::extensions::PragmaHandler("te", "barrier", tok::eod,
                            [](MatchObject object, NodePtr node) {
                                LiteralPtr literal = Literal::get(manager, manager.getLangBasic().getInt4(), "0");
                                ReturnStmtPtr stmt = ReturnStmt::get(manager, literal);
                                return stmt;
                            });


            pragmaHandlers.push_back(std::make_shared<insieme::frontend::extensions::PragmaHandler>(a));
            pragmaHandlers.push_back(std::make_shared<insieme::frontend::extensions::PragmaHandler>(b));
            pragmaHandlers.push_back(std::make_shared<insieme::frontend::extensions::PragmaHandler>(c));
        }
    }

    //TYPE VISITOR
	virtual core::TypePtr Visit(const clang::Type* type, frontend::conversion::Converter& convFact) {
        typeVisited=true;
        return nullptr;
	}

    virtual insieme::core::TypePtr PostVisit(const clang::Type* type, const insieme::core::TypePtr& irType,
                                             frontend::conversion::Converter& convFact) {
        postTypeVisited=true;
        return irType;
	}

	//EXPR VISITOR
	virtual core::ExpressionPtr Visit(const clang::Expr* expr, frontend::conversion::Converter& convFact) {
        exprVisited=true;
        return nullptr;
	}

    virtual insieme::core::ExpressionPtr PostVisit(const clang::Expr* expr, const insieme::core::ExpressionPtr& irExpr,
                                             frontend::conversion::Converter& convFact) {
        postExprVisited=true;
        return irExpr;
	}

	//STMT VISITOR
    virtual stmtutils::StmtWrapper Visit(const clang::Stmt* stmt, insieme::frontend::conversion::Converter& convFact) {
        stmtVisited=true;
        return stmtutils::StmtWrapper();
    }

    virtual stmtutils::StmtWrapper PostVisit(const clang::Stmt* stmt, const stmtutils::StmtWrapper& irStmt,
                                             frontend::conversion::Converter& convFact) {
        postStmtVisited=true;
        return irStmt;
	}

	virtual core::ProgramPtr IRVisit(core::ProgramPtr& prog) {
        progVisited = true;
        return prog;
	}

    virtual frontend::tu::IRTranslationUnit IRVisit(frontend::tu::IRTranslationUnit& tu) {
        tuVisited = true;
        return tu;
	}

};



/**
 *  This test checks if the user provided
 *  clang plugin registration works correctly.
 */
TEST(ClangStage, Initialization) {
	//initialization
	insieme::core::NodeManager mgr;
    insieme::frontend::ConversionJob job(SRC_DIR "/inputs/simple.c");
    job.registerFrontendPlugin<ClangTestPlugin>();

	// register the frontend plugin
	EXPECT_EQ(1, job.getPlugins().size());
}

/**
 *  This test checks if the user provided
 *  clang visitors are working correctly.
 */
TEST(ClangStage, Conversion) {
	//initialization
	insieme::core::NodeManager mgr;
    insieme::frontend::ConversionJob job(SRC_DIR "/inputs/simple.c");
    job.registerFrontendPlugin<ClangTestPlugin>();

	EXPECT_FALSE(typeVisited);
	EXPECT_FALSE(stmtVisited);
	EXPECT_FALSE(exprVisited);
	EXPECT_FALSE(postTypeVisited);
	EXPECT_FALSE(postStmtVisited);
	EXPECT_FALSE(postExprVisited);
	auto program = job.execute(mgr);
	EXPECT_TRUE(typeVisited);
	EXPECT_TRUE(stmtVisited);
	EXPECT_TRUE(exprVisited);
	EXPECT_TRUE(postTypeVisited);
	EXPECT_TRUE(postStmtVisited);
	EXPECT_TRUE(postExprVisited);
}

/**
 *  This test checks if the user provided
 *  macros are working correctly.
 */
TEST(PreClangStage, Macros) {
	//initialization
	insieme::core::NodeManager mgr;
    insieme::frontend::ConversionJob job(SRC_DIR "/inputs/simple.c");
    job.registerFrontendPlugin<ClangTestPlugin>();
    //execute job
    auto program = job.execute(mgr);
  	auto targetCode = insieme::backend::sequential::SequentialBackend::getDefault()->convert(program);
  	std::stringstream code;
  	code << (*targetCode);
  	EXPECT_TRUE(code.str().find("MOOSI_FOR_PRESIDENT") != std::string::npos);
  	EXPECT_TRUE(code.str().find("char* rule_one") != std::string::npos);
}

/**
 *  This test checks if the user plugin
 *  header injection is working correctly.
 */
TEST(PreClangStage, HeaderInjection) {
	//initialization
	insieme::core::NodeManager mgr;
    insieme::frontend::ConversionJob job(SRC_DIR "/inputs/simple.c");
    job.registerFrontendPlugin<ClangTestPlugin>();
    //execute job
    auto program = job.execute(mgr);
  	auto targetCode = insieme::backend::sequential::SequentialBackend::getDefault()->convert(program);
  	std::stringstream code;
  	code << (*targetCode);
 	EXPECT_TRUE(code.str().find("int32_t magicFunction()") != std::string::npos);
 	EXPECT_TRUE(code.str().find("return 42;") != std::string::npos);
}

/**
 *  This test checks if the user plugin
 *  header injection is working correctly.
 *  We try to kidnap stdio.h and provide our
 *  implementation, that defines the magicFunction
 */
TEST(PreClangStage, HeaderKidnapping) {
	//initialization
	insieme::core::NodeManager mgr;
    insieme::frontend::ConversionJob job(SRC_DIR "/inputs/simple.c");
    job.registerFrontendPlugin<ClangTestPlugin>(1);
    //execute job
    auto program = job.execute(mgr);
  	auto targetCode = insieme::backend::sequential::SequentialBackend::getDefault()->convert(program);
  	std::stringstream code;
  	code << (*targetCode);
 	EXPECT_TRUE(code.str().find("int32_t magicFunction()") != std::string::npos);
 	EXPECT_TRUE(code.str().find("return -42;") != std::string::npos);
}

/**
 *  This test checks if the user plugin
 *  IR visitor (tu and program) is working
 *  correctly.
 */
TEST(PostClangStage, IRVisit) {
	//initialization
	insieme::core::NodeManager mgr;
    insieme::frontend::ConversionJob job(SRC_DIR "/inputs/simple.c");
    job.registerFrontendPlugin<ClangTestPlugin>();
    //execute job
    progVisited = false;
    tuVisited = false;
    EXPECT_FALSE(progVisited);
 	EXPECT_FALSE(tuVisited);
    auto program = job.execute(mgr);
 	EXPECT_TRUE(progVisited);
 	EXPECT_TRUE(tuVisited);
}

/**
 *  This test checks if the user plugin
 *  pragma handler is working correctly.
 *  for normal pragmas, deattached pragmas
 *  and stmts with multiple pragmas
 */
TEST(PragmaHandlerTest, PragmaTest) {
    //initialization
    insieme::core::NodeManager mgr;
    insieme::frontend::ConversionJob job(SRC_DIR "/inputs/simple.c");
    job.registerFrontendPlugin<ClangTestPlugin>(2);
    //execute job
    //checks also if handling for stmts that are
    //attached with multiple pragmas is working

    //pragma one changes the return value from 42 to 15
    //pragma two which is attached to the same stmt
    //changes it to 12. the deattached stmt adds a return 0;
    //stmt at the end of the compound (where the pragma actually is placed)
    auto program = job.execute(mgr);
    auto targetCode = insieme::backend::sequential::SequentialBackend::getDefault()->convert(program);
    std::stringstream code;
    code << (*targetCode);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(code, line)) {
        lines.push_back(line);
    }
    //check if our de-attached pragma was matched and handled
    EXPECT_TRUE(lines[lines.size()-4].find("return 0;") != std::string::npos);
    EXPECT_TRUE(lines[lines.size()-3].find("}") != std::string::npos);
    //check if the other pragma handlers provided the correct result
    EXPECT_TRUE(code.str().find("return 12;") != std::string::npos);
}

////////////////////////////////////////////////////////////////////////////////////////////////
//    Decls visitor

 	int varsPre, varsPost;
	int funcsPre, funsPost;
	int typesPre, typesPost;

struct DeclVistors : public insieme::frontend::extensions::FrontendPlugin {

    virtual core::ExpressionPtr FuncDeclVisit(const clang::FunctionDecl* decl, frontend::conversion::Converter& convFact) {
        funcsPre++;
        return nullptr;
    }

	virtual core::ExpressionPtr ValueDeclVisit(const clang::ValueDecl* decl, frontend::conversion::Converter& convFact) {
        varsPre++;
        return nullptr;
    }

    virtual core::TypePtr TypeDeclVisit(const clang::TypeDecl* decl, frontend::conversion::Converter& convFact) {
        typesPre++;
        return nullptr;
    }

	virtual void PostVisit(const clang::Decl* decl, frontend::conversion::Converter& convFact) {
		if (llvm::dyn_cast<clang::FunctionDecl>(decl)){
			funsPost++;
		}
		else if (llvm::dyn_cast<clang::VarDecl>(decl)){
			varsPost++;
		}
		else if (llvm::dyn_cast<clang::TypeDecl>(decl)){
			typesPost++;
		}
	}
};


TEST(DeclsStage, MatchVisits) {
	//initialization
	insieme::core::NodeManager mgr;
    insieme::frontend::ConversionJob job(SRC_DIR "/inputs/decls.cpp");

	varsPre  = varsPost = 0;
	funcsPre = funsPost = 0;
	typesPre = typesPost = 0;

	// register plugin
    job.registerFrontendPlugin<DeclVistors>();

    //execute job
    auto program = job.execute(mgr);

//	std::cout << varsPre   <<" , " << varsPost << std::endl;
//	std::cout << funcsPre  <<" , " << funsPost << std::endl;
//	std::cout << typesPre  <<" , " << typesPost<< std::endl;

	EXPECT_EQ (10, varsPre);
	EXPECT_EQ (18, funcsPre);   // this is weird, but works
	EXPECT_EQ (6, typesPre);

	EXPECT_EQ (varsPre, varsPost);
	EXPECT_EQ (funcsPre, funsPost);
	EXPECT_EQ (typesPre, typesPost);
}




