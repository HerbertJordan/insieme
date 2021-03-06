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

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "insieme/backend/sequential/sequential_backend.h"

#include "insieme/frontend/stmt_converter.h"
#include "insieme/frontend/expr_converter.h"
#include "insieme/frontend/type_converter.h"

#include "insieme/core/ir_program.h"
#include "insieme/core/checks/full_check.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/utils/config.h"
#include "insieme/frontend/convert.h"
#include "insieme/frontend/utils/stmt_wrapper.h"

#include "insieme/frontend/extensions/frontend_extension.h"

#include "insieme/driver/cmd/insiemecc_options.h"

using namespace insieme;
using namespace insieme::frontend;

bool typeVisited 	 = false;
bool exprVisited 	 = false;
bool stmtVisited 	 = false;
bool postTypeVisited = false;
bool postExprVisited = false;
bool postStmtVisited = false;
bool tuVisited 		 = false;
bool progVisited 	 = false;

insieme::core::NodeManager manager;

class ClangTestExtension : public insieme::frontend::extensions::FrontendExtension {
    bool fstate1;
    bool fstate2;
    bool stateSet;
public:
    ClangTestExtension() : fstate1(false), fstate2(false), stateSet(false) { }

    void setState(int N) {
        if(stateSet)
            return;

        if(N==0) {
            macros.insert(std::make_pair<std::string,std::string>("A","char *rule_one = \"MOOSI_FOR_PRESIDENT\""));
            injectedHeaders.push_back("injectedHeader.h");
        }
        if(N==1) {
            macros.insert(std::make_pair<std::string,std::string>("A",""));
            kidnappedHeaders.push_back(CLANG_SRC_DIR "/inputs/kidnapped");
        }
        if(N==2) {
            using namespace insieme::frontend::pragma;
            using namespace insieme::core;

            macros.insert(std::make_pair<std::string,std::string>("A",""));
            injectedHeaders.push_back("injectedHeader.h");

            auto var_list = tok::var["v"] >> *(~tok::comma >> tok::var["v"]);

            node&& x =  var_list["private"] >> kwd("num_threads") >> tok::l_paren >>
                        tok::expr["num_threads"] >> tok::r_paren >> tok::eod;

            node&& y =  kwd("auto") >> tok::eod;

            //pragma that should be matched: #pragma te loop x num_threads(x*2)
            auto a = insieme::frontend::extensions::PragmaHandler("te", "loop", x,
                            [](const MatchObject& object, core::NodeList nodes) {
                                EXPECT_TRUE(object.getVars("private").size() && object.getExprs("num_threads").size());
                                EXPECT_TRUE(object.getVars("private")[0]);
                                EXPECT_TRUE(object.getExprs("num_threads")[0]);
                                LiteralPtr literal = Literal::get(manager, manager.getLangBasic().getInt4(), "15");
                                ReturnStmtPtr stmt = ReturnStmt::get(manager, literal);
                                return core::NodeList({stmt});
                            });
            //pragma that should be matched: #pragma te scheduling auto
            auto b = insieme::frontend::extensions::PragmaHandler("te", "scheduling", y,
                            [](const MatchObject& object, core::NodeList nodes) {
                                EXPECT_EQ ("return 15", toString(*nodes[0]));
                                LiteralPtr literal = Literal::get(manager, manager.getLangBasic().getInt4(), "12");
                                ReturnStmtPtr stmt = ReturnStmt::get(manager, literal);
                                return core::NodeList({stmt});
                            });
            //pragma that should be matched: #pragma te barrier
            auto c = insieme::frontend::extensions::PragmaHandler("te", "barrier", tok::eod,
                            [](const MatchObject& object, core::NodeList nodes) {
                                LiteralPtr literal = Literal::get(manager, manager.getLangBasic().getInt4(), "0");
                                ReturnStmtPtr stmt = ReturnStmt::get(manager, literal);
                                return core::NodeList({stmt});
                            });


            pragmaHandlers.push_back(std::make_shared<insieme::frontend::extensions::PragmaHandler>(a));
            pragmaHandlers.push_back(std::make_shared<insieme::frontend::extensions::PragmaHandler>(b));
            pragmaHandlers.push_back(std::make_shared<insieme::frontend::extensions::PragmaHandler>(c));
        }
        stateSet = true;
    }

    //TYPE VISITOR
	virtual core::TypePtr Visit(const clang::QualType& type, frontend::conversion::Converter& convFact) {
        typeVisited=true;
        return nullptr;
	}

    virtual insieme::core::TypePtr PostVisit(const clang::QualType& type, const insieme::core::TypePtr& irType,
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

    virtual frontend::extensions::FrontendExtension::flagHandler registerFlag(boost::program_options::options_description& options) {
        options.add_options()("fstate1", boost::program_options::value<bool>(&fstate1)->implicit_value(true), "Set state of Extension to 1");
        options.add_options()("fstate2", boost::program_options::value<bool>(&fstate2)->implicit_value(true), "Set state of Extension to 2");
        auto lambda = [&](const frontend::ConversionJob& job) {
            if(fstate1)
                setState(1);
            else if(fstate2)
                setState(2);
            else
                setState(0);
            return true;
        };
        return lambda;
	}

};


using namespace insieme::driver;

/**
 *  This test checks if the user provided
 *  clang extension registration works correctly.
 */
TEST(ClangStage, Initialization) {
	//initialization
	insieme::core::NodeManager mgr;
    std::vector<std::string> args = { "compiler", CLANG_SRC_DIR "/inputs/main.c", "--fnodefaultextensions" };

    cmd::Options options = cmd::Options::parse(args);
    //explicitly add extension without flag! - needs frontendExtensionInit call
    options.job.registerFrontendExtension<ClangTestExtension>();

	// register the frontend extension
	EXPECT_EQ(0, options.job.getExtensions().size());
	options.job.frontendExtensionInit();
	EXPECT_EQ(1, options.job.getExtensions().size());
}

/**
 *  This test checks if the user provided
 *  clang visitors are working correctly.
 */
TEST(ClangStage, Conversion) {
	//initialization
	insieme::core::NodeManager mgr;
    std::vector<std::string> args = { "compiler", CLANG_SRC_DIR "/inputs/simple.c", "--fnodefaultextensions" };

    cmd::Options options = cmd::Options::parse(args);
    options.job.registerFrontendExtension<ClangTestExtension>();


	EXPECT_FALSE(typeVisited);
	EXPECT_FALSE(stmtVisited);
	EXPECT_FALSE(exprVisited);
	EXPECT_FALSE(postTypeVisited);
	EXPECT_FALSE(postStmtVisited);
	EXPECT_FALSE(postExprVisited);
	auto program = options.job.execute(mgr);
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
    std::vector<std::string> args = { "compiler", CLANG_SRC_DIR "/inputs/simple.c", "--fnodefaultextensions" };

    cmd::Options options = cmd::Options::parse(args);
    options.job.registerFrontendExtension<ClangTestExtension>();
	options.job.frontendExtensionInit();

    //execute job
    auto program = options.job.execute(mgr);
  	auto targetCode = insieme::backend::sequential::SequentialBackend::getDefault()->convert(program);
  	std::stringstream code;
  	code << (*targetCode);
  	EXPECT_TRUE(code.str().find("MOOSI_FOR_PRESIDENT") != std::string::npos);
  	EXPECT_TRUE(code.str().find("char* rule_one") != std::string::npos);
}

/**
 *  This test checks if the user extension
 *  header injection is working correctly.
 */
TEST(PreClangStage, HeaderInjection) {
	//initialization
	insieme::core::NodeManager mgr;
    std::vector<std::string> args = { "compiler", CLANG_SRC_DIR "/inputs/simple.c", "--fnodefaultextensions" };

    cmd::Options options = cmd::Options::parse(args);
    options.job.registerFrontendExtension<ClangTestExtension>();
    options.job.frontendExtensionInit();

    //execute job
    auto program = options.job.execute(mgr);
  	auto targetCode = insieme::backend::sequential::SequentialBackend::getDefault()->convert(program);
  	std::stringstream code;
  	code << (*targetCode);
 	EXPECT_TRUE(code.str().find("int32_t magicFunction()") != std::string::npos);
 	EXPECT_TRUE(code.str().find("return 42;") != std::string::npos);
}

/**
 *  This test checks if the user extension
 *  header injection is working correctly.
 *  We try to kidnap stdio.h and provide our
 *  implementation, that defines the magicFunction
 */
TEST(PreClangStage, HeaderKidnapping) {
	//initialization
	insieme::core::NodeManager mgr;
    std::vector<std::string> args = { "compiler", CLANG_SRC_DIR "/inputs/simple.c", "-fstate1" , "--fnodefaultextensions"};

    cmd::Options options = cmd::Options::parse(args);
    options.job.registerFrontendExtension<ClangTestExtension>();
    options.job.frontendExtensionInit();

    //execute job
    auto program = options.job.execute(mgr);
  	auto targetCode = insieme::backend::sequential::SequentialBackend::getDefault()->convert(program);
  	std::stringstream code;
  	code << (*targetCode);
 	EXPECT_TRUE(code.str().find("int32_t magicFunction()") != std::string::npos);
 	EXPECT_TRUE(code.str().find("return -42;") != std::string::npos);
}

/**
 *  This test checks if the user extension
 *  IR visitor (tu and program) is working
 *  correctly.
 */
TEST(PostClangStage, IRVisit) {
	//initialization
	insieme::core::NodeManager mgr;
    std::vector<std::string> args = { "compiler", CLANG_SRC_DIR "/inputs/simple.c" , "--fnodefaultextensions"};

    cmd::Options options = cmd::Options::parse(args);
    options.job.registerFrontendExtension<ClangTestExtension>();
	options.job.frontendExtensionInit();


    //execute job
    progVisited = false;
    tuVisited = false;
    EXPECT_FALSE(progVisited);
 	EXPECT_FALSE(tuVisited);
    auto program = options.job.execute(mgr);
 	EXPECT_TRUE(progVisited);
 	EXPECT_TRUE(tuVisited);
}

/**
 *  This test checks if the user extension
 *  pragma handler is working correctly.
 *  for normal pragmas, deattached pragmas
 *  and stmts with multiple pragmas
 */
TEST(PragmaHandlerTest, PragmaTest) {
    //initialization
    insieme::core::NodeManager mgr;
    std::vector<std::string> args = { "compiler", CLANG_SRC_DIR "/inputs/simple.c", "-fstate2" , "--fnodefaultextensions" };

    cmd::Options options = cmd::Options::parse(args);
    options.job.registerFrontendExtension<ClangTestExtension>();
    options.job.frontendExtensionInit();

    //execute job
    //checks also if handling for stmts that are
    //attached with multiple pragmas is working

    //pragma one changes the return value from 42 to 15
    //pragma two which is attached to the same stmt
    //changes it to 12. the deattached stmt adds a return 0;
    //stmt at the end of the compound (where the pragma actually is placed)
    auto program = options.job.execute(mgr);
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

struct DeclVistors : public insieme::frontend::extensions::FrontendExtension {

    virtual core::ExpressionPtr FuncDeclVisit(const clang::FunctionDecl* decl, frontend::conversion::Converter& convFact, bool symbolic) {
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

    virtual core::ExpressionPtr FuncDeclPostVisit(const clang::FunctionDecl* decl, core::ExpressionPtr expr, frontend::conversion::Converter& convFact, bool symbolic) {
        funsPost++;
        return nullptr;
    }

	virtual core::ExpressionPtr ValueDeclPostVisit(const clang::ValueDecl* decl, core::ExpressionPtr expr, frontend::conversion::Converter& convFact) {
        varsPost++;
        return nullptr;
    }

    virtual core::TypePtr TypeDeclPostVisit(const clang::TypeDecl* decl, core::TypePtr type, frontend::conversion::Converter& convFact) {
        typesPost++;
        return nullptr;
    }

    virtual flagHandler registerFlag(boost::program_options::options_description& option) {
    	return [&](const ConversionJob& job) { return true; };
    }

};


TEST(DeclsStage, MatchVisits) {
	//initialization
	insieme::core::NodeManager mgr;
    std::vector<std::string> args = { "compiler", CLANG_SRC_DIR "/inputs/decls.cpp", "--fnodefaultextensions" };

    cmd::Options options = cmd::Options::parse(args);
    options.job.registerFrontendExtension<DeclVistors>();
    options.job.frontendExtensionInit();

	varsPre  = varsPost = 0;
	funcsPre = funsPost = 0;
	typesPre = typesPost = 0;

    //execute job
    auto program = options.job.execute(mgr);

//	std::cout << varsPre   <<" , " << varsPost << std::endl;
//	std::cout << funcsPre  <<" , " << funsPost << std::endl;
//	std::cout << typesPre  <<" , " << typesPost<< std::endl;

	EXPECT_EQ (10, varsPre);
	EXPECT_EQ (18, funcsPre);   // check the test code to count the instances
	EXPECT_EQ (3, typesPre);    // one class and two template spetializations

	EXPECT_EQ (varsPre, varsPost);
	EXPECT_EQ (funcsPre, funsPost);
	EXPECT_EQ (typesPre, typesPost);
}




