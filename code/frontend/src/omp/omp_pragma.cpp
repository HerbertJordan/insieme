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

#include "omp/omp_pragma.h"
#include "omp/omp_annotation.h"

#include "pragma_handler.h"
#include "pragma_matcher.h"

#include <clang/Lex/Pragma.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Expr.h>
#include "conversion.h"

#include <glog/logging.h>

#include <iostream>

using namespace std;

namespace {

using namespace insieme::frontend;
using namespace insieme::frontend::omp;

#define OMP_PRAGMA(TYPE) 	\
struct OmpPragma ## TYPE: public OmpPragma { \
	OmpPragma ## TYPE(const clang::SourceLocation& startLoc, const clang::SourceLocation& endLoc,	\
		   const std::string& name, const insieme::frontend::MatchMap& mmap):	\
		OmpPragma(startLoc, endLoc, name, mmap) { }	\
	virtual AnnotationPtr toAnnotation(conversion::ConversionFactory& fact) const; 	\
}

// Defines basic OpenMP pragma types which will be created by the pragma_matcher class
OMP_PRAGMA(Parallel);
OMP_PRAGMA(For);
OMP_PRAGMA(Sections);
OMP_PRAGMA(Section);
OMP_PRAGMA(Single);
OMP_PRAGMA(Task);
OMP_PRAGMA(Master);
OMP_PRAGMA(Critical);
OMP_PRAGMA(Barrier);
OMP_PRAGMA(TaskWait);
OMP_PRAGMA(Atomic);
OMP_PRAGMA(Flush);
OMP_PRAGMA(Ordered);
OMP_PRAGMA(ThreadPrivate);

} // End anonymous namespace

namespace insieme {
namespace frontend {
namespace omp {

void registerPragmaHandlers(clang::Preprocessor& pp) {
	using namespace insieme::frontend;
	using namespace insieme::frontend::tok;

	// if(scalar-expression)
	auto if_expr 		   	= kwd("if") >> l_paren >> tok::expr["if"] >> r_paren;

	// default(shared | none)
	auto def			   	= Tok<clang::tok::kw_default>() >> l_paren >> ( kwd("shared") | kwd("none") )["default"] >> r_paren;

	// identifier *(, identifier)
	auto identifier_list   	= identifier >> *(~comma >> identifier);

	// private(list)
	auto private_clause    	=  kwd("private") >> l_paren >> identifier_list["private"] >> r_paren;

	// firstprivate(list)
	auto firstprivate_clause = kwd("firstprivate") >> l_paren >> identifier_list["firstprivate"] >> r_paren;

	// lastprivate(list)
	auto lastprivate_clause = kwd("lastprivate") >> l_paren >> identifier_list["lastprivate"] >> r_paren;

	// + or - or * or & or | or ^ or && or ||
	auto op 			  	= tok::plus | tok::minus | tok::star | tok::amp | tok::pipe | tok::caret | tok::ampamp | tok::pipepipe;

	// reduction(operator: list)
	auto reduction_clause 	= kwd("reduction") >> l_paren >> op["reduction_op"] >> colon >> identifier_list["reduction"] >> r_paren;

	auto parallel_clause =  ( 	// if(scalar-expression)
								if_expr
							| 	// num_threads(integer-expression)
								(kwd("num_threads") >> l_paren >> expr["num_threads"] >> r_paren)
							|	// default(shared | none)
								def
							|	// private(list)
								private_clause
							|	// firstprivate(list)
								firstprivate_clause
							|	// shared(list)
								(kwd("shared") >> l_paren >> identifier_list["shared"] >> r_paren)
							|	// copyin(list)
								(kwd("copyin") >> l_paren >> identifier_list["copyin"] >> r_paren)
							|	// reduction(operator: list)
								reduction_clause
							);

	auto kind 			=   Tok<clang::tok::kw_static>() | kwd("dynamic") | kwd("guided") | kwd("auto") | kwd("runtime");

	auto for_clause 	=	(	private_clause
							|	firstprivate_clause
							|	lastprivate_clause
							|	reduction_clause
								// schedule( (static | dynamic | guided | atuo | runtime) (, chunk_size) )
							|	(kwd("schedule") >> l_paren >> kind["schedule"] >> !( comma >> expr["chunk_size"] ) >> r_paren)
								// collapse( expr )
							|	(kwd("collapse") >> l_paren >> expr["collapse"] >> r_paren)
								// nowait
							|	kwd("nowait")
							);

	auto for_clause_list = !(for_clause >> *( !comma >> for_clause ));

	auto sections_clause =  ( 	// private(list)
								private_clause
							| 	// firstprivate(list)
								firstprivate_clause
							|	// lastprivate(list)
								lastprivate_clause
							|	// reduction(operator: list)
								reduction_clause
							| 	// nowait
								kwd("nowait")
							);

	auto sections_clause_list = !(sections_clause >> *( !comma >> sections_clause ));

	// [clause[ [, ]clause] ...] new-line
	auto parallel_for_clause_list = (parallel_clause | for_clause | sections_clause) >> *( !comma >> (parallel_clause | for_clause | sections_clause) );

	auto parallel_clause_list = !( 	(Tok<clang::tok::kw_for>("for") >> !parallel_for_clause_list)
								 |  (kwd("sections") >> !parallel_for_clause_list)
								 | 	(parallel_clause >> *(!comma >> parallel_clause))
								 );

	auto single_clause 	= 	(	// private(list)
								private_clause
							|	// firstprivate(list)
								firstprivate_clause
							|	// copyprivate(list)
							 	kwd("copyprivate") >> l_paren >> identifier_list["copyprivate"] >> r_paren
							|	// nowait
								kwd("nowait")
							);

	auto single_clause_list = !(single_clause >> *( !comma >> single_clause ));

	auto task_clause	 = 	(	// if(scalar-expression)
								if_expr
							|	// untied
								kwd("untied")
							|	// default(shared | none)
								def
							|	// private(list)
								private_clause
							| 	// firstprivate(list)
								firstprivate_clause
							|	// shared(list)
								kwd("shared") >> l_paren >> identifier_list["shared"] >> r_paren
							);

	auto task_clause_list = !(task_clause >> *( !comma >> task_clause ));

	// threadprivate(list)
	auto threadprivate_clause = l_paren >> identifier_list["thread_private"] >> r_paren;

	// define a PragmaNamespace for omp
	clang::PragmaNamespace* omp = new clang::PragmaNamespace("omp");
	pp.AddPragmaHandler(omp);

	// Add an handler for pragma omp parallel:
	// #pragma omp parallel [clause[ [, ]clause] ...] new-line
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaParallel>(pp.getIdentifierInfo("parallel"), parallel_clause_list >> tok::eom, "omp"));

	// omp for
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaFor>(pp.getIdentifierInfo("for"), for_clause_list >> tok::eom, "omp"));

	// #pragma omp sections [clause[[,] clause] ...] new-line
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaSections>(pp.getIdentifierInfo("sections"), sections_clause_list >> tok::eom, "omp"));

	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaSection>(pp.getIdentifierInfo("section"), tok::eom, "omp"));

	// omp single
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaSingle>(pp.getIdentifierInfo("single"), single_clause_list >> tok::eom, "omp"));

	// #pragma omp task [clause[[,] clause] ...] new-line
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaTask>(pp.getIdentifierInfo("task"), task_clause_list >> tok::eom, "omp"));

	// #pragma omp master new-line
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaMaster>(pp.getIdentifierInfo("master"), tok::eom, "omp"));

	// #pragma omp critical [(name)] new-line
	omp->AddPragma( PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaCritical>(pp.getIdentifierInfo("critical"),
					!(l_paren >> identifier >> r_paren) >> tok::eom, "omp"));

	//#pragma omp barrier new-line
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaBarrier>(pp.getIdentifierInfo("barrier"), tok::eom, "omp"));

	// #pragma omp taskwait newline
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaTaskWait>(pp.getIdentifierInfo("taskwait"), tok::eom, "omp"));

	// #pragma omp atimic newline
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaAtomic>(pp.getIdentifierInfo("atomic"), tok::eom, "omp"));

	// #pragma omp flush [(list)] new-line
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaFlush>(pp.getIdentifierInfo("flush"), !identifier_list["flush"] >> tok::eom, "omp"));

	// #pragma omp ordered new-line
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaOrdered>(pp.getIdentifierInfo("ordered"), tok::eom, "omp"));

	// #pragma omp threadprivate(list) new-line
	omp->AddPragma(PragmaHandlerFactory::CreatePragmaHandler<OmpPragmaThreadPrivate>(pp.getIdentifierInfo("threadprivate"), threadprivate_clause >> tok::eom, "omp"));
}

/**
 * take care of filtering OmpPragmas from the list of pragmas attached to the clang node and attaches the resulting annotation
 * to the IR node
 */
void attachOmpAnnotation(const core::NodePtr& irNode, const clang::Stmt* clangNode, conversion::ConversionFactory& fact) {
	const PragmaStmtMap::StmtMap& pragmaStmtMap = fact.getPragmaMap().getStatementMap();
	std::pair<PragmaStmtMap::StmtMap::const_iterator, PragmaStmtMap::StmtMap::const_iterator> iter = pragmaStmtMap.equal_range(clangNode);

	omp::BaseAnnotation::AnnotationList anns;
	std::for_each(iter.first, iter.second,
		[ &fact, &anns ](const PragmaStmtMap::StmtMap::value_type& curr){
			const OmpPragma* ompPragma = dynamic_cast<const OmpPragma*>(&*(curr.second));
			if(ompPragma) {
				VLOG(1) << "Statement has an OpenMP pragma attached";
				anns.push_back( ompPragma->toAnnotation(fact) );
			}
	});

	irNode.addAnnotation( std::make_shared<omp::BaseAnnotation>( anns ) );
}

OmpPragma::OmpPragma(const clang::SourceLocation& startLoc, const clang::SourceLocation& endLoc, const string& name, const MatchMap& mmap):
	Pragma(startLoc, endLoc, name, mmap), mMap(mmap) {

//	DLOG(INFO) << "~~~PRAGMA~~~" << std::endl;
//	for(MatchMap::const_iterator i = mmap.begin(), e = mmap.end(); i!=e; ++i) {
//		DLOG(INFO) << "KEYWORD: " << i->first << ":" << std::endl;
//		for(ValueList::const_iterator i2=i->second.begin(), e2=i->second.end(); i2!=e2; ++i2)
//			DLOG(INFO) << (*i2)->toStr() << ", ";
//		DLOG(INFO) << std::endl;
//	}
}

} // End omp namespace
} // End frontend namespace
} // End insieme namespace

namespace {

using namespace insieme;
using namespace insieme::frontend;

/**
 * Create an annotation with the list of identifiers, used for clauses: private,firstprivate,lastprivate
 */
VarListPtr handleIdentifierList(const MatchMap& mmap, const std::string& key, conversion::ConversionFactory& fact) {

	auto fit = mmap.find(key);
	if(fit == mmap.end())
		return VarListPtr();

	const ValueList& vars = fit->second;
	VarList* varList = new VarList;
	for(ValueList::const_iterator it = vars.begin(), end = vars.end(); it != end; ++it) {
		clang::Stmt* varIdent = (*it)->get<clang::Stmt*>();
		assert(varIdent && "Clause not containing var exps");

		clang::DeclRefExpr* refVarIdent = dyn_cast<clang::DeclRefExpr>(varIdent);
		assert(refVarIdent && "Clause not containing a DeclRefExpr");

		core::VariablePtr varExpr = core::dynamic_pointer_cast<const core::Variable>( fact.convertExpr( *refVarIdent ) );
		assert(varExpr && "Conversion to Insieme node failed!");
		varList->push_back( varExpr );
	}
	return VarListPtr( varList );
}

// reduction(operator: list)
// operator = + or - or * or & or | or ^ or && or ||
ReductionPtr handleReductionClause(const MatchMap& mmap, conversion::ConversionFactory& fact) {

	auto fit = mmap.find("reduction");
	if(fit == mmap.end())
		return ReductionPtr();

	// we have a reduction
	// check the operator
	auto opIt = mmap.find("reduction_op");
	assert(opIt != mmap.end() && "Reduction clause doesn't contains an operator");
	const ValueList& opVar = opIt->second;
	assert(opVar.size() == 1);

	std::string* opStr = opVar.front()->get<std::string*>();
	assert(opStr && "Reduction clause with no operator");

	Reduction::Operator op;
	if(*opStr == "+")		op = Reduction::PLUS;
	else if(*opStr == "-")	op = Reduction::MINUS;
	else if(*opStr == "*")	op = Reduction::STAR;
	else if(*opStr == "&")	op = Reduction::AND;
	else if(*opStr == "|")	op = Reduction::OR;
	else if(*opStr == "^")	op = Reduction::XOR;
	else if(*opStr == "&&")	op = Reduction::LAND;
	else if(*opStr == "||")	op = Reduction::LOR;
	else assert(false && "Reduction operator not supported.");

	return std::make_shared<Reduction>(op, handleIdentifierList(mmap, "reduction", fact));
}

core::ExpressionPtr handleSingleExpression(const MatchMap& mmap,  const std::string& key, conversion::ConversionFactory& fact) {

	auto fit = mmap.find(key);
	if(fit == mmap.end())
		return core::ExpressionPtr();

	// we have an expression
	const ValueList& expr = fit->second;
	assert(expr.size() == 1);
	clang::Expr* collapseExpr = dyn_cast<clang::Expr>(expr.front()->get<clang::Stmt*>());
	assert(collapseExpr && "OpenMP collapse clause's expression is not of type clang::Expr");
	return fact.convertExpr( *collapseExpr );
}

// schedule( (static | dynamic | guided | atuo | runtime) (, chunk_size) )
SchedulePtr handleScheduleClause(const MatchMap& mmap, conversion::ConversionFactory& fact) {

	auto fit = mmap.find("schedule");
	if(fit == mmap.end())
		return SchedulePtr();

	// we have a schedule clause
	const ValueList& kind = fit->second;
	assert(kind.size() == 1);
	std::string& kindStr = *kind.front()->get<std::string*>();

	Schedule::Kind k;
	if(kindStr == "static")
		k = Schedule::STATIC;
	else if (kindStr == "dynamic")
		k = Schedule::DYNAMIC;
	else if (kindStr == "guided")
		k = Schedule::GUIDED;
	else if (kindStr == "auto")
		k = Schedule::AUTO;
	else if (kindStr == "runtime")
		k = Schedule::RUNTIME;
	else
		assert(false && "Unsupported scheduling kind");

	// check for chunk_size expression
	core::ExpressionPtr chunkSize = handleSingleExpression(mmap, "chunk_size", fact);
	return std::make_shared<Schedule>(k, chunkSize);
}

bool hasKeyword(const MatchMap& mmap, const std::string& key) {
	auto fit = mmap.find(key);
	return fit != mmap.end();
}

DefaultPtr handleDefaultClause(const MatchMap& mmap, conversion::ConversionFactory& fact) {

	auto fit = mmap.find("default");
	if(fit == mmap.end())
		return DefaultPtr();

	// we have a schedule clause
	const ValueList& kind = fit->second;
	assert(kind.size() == 1);
	std::string& kindStr = *kind.front()->get<std::string*>();

	Default::Kind k;
	if(kindStr == "shared")
		k = Default::SHARED;
	else if(kindStr == "none")
		k = Default::NONE;
	else
		assert(false && "Unsupported default kind");

	return std::make_shared<Default>(k);
}


// if(scalar-expression)
// num_threads(integer-expression)
// default(shared | none)
// private(list)
// firstprivate(list)
// shared(list)
// copyin(list)
// reduction(operator: list)
AnnotationPtr OmpPragmaParallel::toAnnotation(conversion::ConversionFactory& fact) const {
	const MatchMap& map = getMap();
	// check for if clause
	core::ExpressionPtr	ifClause = handleSingleExpression(map, "if", fact);
	// check for num_threads clause
	core::ExpressionPtr	numThreadsClause = handleSingleExpression(map, "num_threads", fact);
	// check for default clause
	DefaultPtr defaultClause = handleDefaultClause(map, fact);
	// check for private clause
	VarListPtr privateClause = handleIdentifierList(map, "private", fact);
	// check for firstprivate clause
	VarListPtr firstPrivateClause = handleIdentifierList(map, "firstprivate", fact);
	// check for shared clause
	VarListPtr sharedClause = handleIdentifierList(map, "shared", fact);
	// check for copyin clause
	VarListPtr copyinClause = handleIdentifierList(map, "copyin", fact);
	// check for reduction clause
	ReductionPtr reductionClause = handleReductionClause(map, fact);

	// check for 'for'
	if(hasKeyword(map, "for")) {
		// this is a parallel for
		VarListPtr lastPrivateClause = handleIdentifierList(map, "lastprivate", fact);
		// check for schedule clause
		SchedulePtr scheduleClause = handleScheduleClause(map, fact);
		// check for collapse cluase
		core::ExpressionPtr	collapseClause = handleSingleExpression(map, "collapse", fact);
		// check for nowait keyword
		bool noWait = hasKeyword(map, "nowait");

		/* TODO: This is a Visual Studio 2010 fix. make_shared cannot be called with 12 parameters (max 10)
		return std::make_shared<ParallelFor>(
				ifClause, numThreadsClause, defaultClause, privateClause,
					firstPrivateClause, sharedClause, copyinClause, reductionClause, lastPrivateClause,
					scheduleClause, collapseClause, noWait
		);*/
		return std::shared_ptr<ParallelFor>(new ParallelFor(ifClause, numThreadsClause, defaultClause, privateClause,
					firstPrivateClause, sharedClause, copyinClause, reductionClause, lastPrivateClause,
					scheduleClause, collapseClause, noWait));
	}

	// check for 'sections'
	if(hasKeyword(map, "sections")) {
		// this is a parallel for
		VarListPtr lastPrivateClause = handleIdentifierList(map, "lastprivate", fact);
		// check for nowait keyword
		bool noWait = hasKeyword(map, "nowait");

		return std::make_shared<ParallelSections>(
			ifClause, numThreadsClause, defaultClause, privateClause,
					firstPrivateClause, sharedClause, copyinClause, reductionClause, lastPrivateClause, noWait
		);
	}

	return std::make_shared<Parallel>(
			ifClause, numThreadsClause, defaultClause, privateClause,
					firstPrivateClause, sharedClause, copyinClause, reductionClause
	);

}

AnnotationPtr OmpPragmaFor::toAnnotation(conversion::ConversionFactory& fact) const {
	const MatchMap& map = getMap();
	// check for private clause
	VarListPtr privateClause = handleIdentifierList(map, "private", fact);
	// check for firstprivate clause
	VarListPtr firstPrivateClause = handleIdentifierList(map, "firstprivate", fact);
	// check for lastprivate clause
	VarListPtr lastPrivateClause = handleIdentifierList(map, "lastprivate", fact);
	// check for reduction clause
	ReductionPtr reductionClause = handleReductionClause(map, fact);
	// check for schedule clause
	SchedulePtr scheduleClause = handleScheduleClause(map, fact);
	// check for collapse cluase
	core::ExpressionPtr	collapseClause = handleSingleExpression(map, "collapse", fact);
	// check for nowait keyword
	bool noWait = hasKeyword(map, "nowait");

	return std::make_shared<For>( privateClause, firstPrivateClause, lastPrivateClause, reductionClause, scheduleClause, collapseClause, noWait );
}

// Translate a pragma omp section into a OmpSection annotation
// private(list)
// firstprivate(list)
// lastprivate(list)
// reduction(operator: list)
// nowait
AnnotationPtr OmpPragmaSections::toAnnotation(conversion::ConversionFactory& fact) const {
	const MatchMap& map = getMap();
	// check for private clause
	VarListPtr privateClause = handleIdentifierList(map, "private", fact);
	// check for firstprivate clause
	VarListPtr firstPrivateClause = handleIdentifierList(map, "firstprivate", fact);
	// check for lastprivate clause
	VarListPtr lastPrivateClause = handleIdentifierList(map, "lastprivate", fact);
	// check for reduction clause
	ReductionPtr reductionClause = handleReductionClause(map, fact);
	// check for nowait keyword
	bool noWait = hasKeyword(map, "nowait");

	return std::make_shared<Sections>( privateClause, firstPrivateClause, lastPrivateClause, reductionClause, noWait );
}

AnnotationPtr OmpPragmaSection::toAnnotation(conversion::ConversionFactory& fact) const { return std::make_shared<Section>( ); }

// OmpSingle
// private(list)
// firstprivate(list)
// copyprivate(list)
// nowait
AnnotationPtr OmpPragmaSingle::toAnnotation(conversion::ConversionFactory& fact) const {
	const MatchMap& map = getMap();
	// check for private clause
	VarListPtr privateClause = handleIdentifierList(map, "private", fact);
	// check for firstprivate clause
	VarListPtr firstPrivateClause = handleIdentifierList(map, "firstprivate", fact);
	// check for copyprivate clause
	VarListPtr copyPrivateClause = handleIdentifierList(map, "copyprivate", fact);
	// check for nowait keyword
	bool noWait = hasKeyword(map, "nowait");

	return std::make_shared<Single>( privateClause, firstPrivateClause, copyPrivateClause, noWait );
}

// if(scalar-expression)
// untied
// default(shared | none)
// private(list)
// firstprivate(list)
// shared(list)
AnnotationPtr OmpPragmaTask::toAnnotation(conversion::ConversionFactory& fact) const {
	const MatchMap& map = getMap();
	// check for if clause
	core::ExpressionPtr	ifClause = handleSingleExpression(map, "if", fact);
	// check for nowait keyword
	bool untied = hasKeyword(map, "untied");
	// check for default clause
	DefaultPtr defaultClause = handleDefaultClause(map, fact);
	// check for private clause
	VarListPtr privateClause = handleIdentifierList(map, "private", fact);
	// check for firstprivate clause
	VarListPtr firstPrivateClause = handleIdentifierList(map, "firstprivate", fact);
	// check for shared clause
	VarListPtr sharedClause = handleIdentifierList(map, "shared", fact);
	// We need to check if the
	return make_shared<omp::Task>( ifClause, untied, defaultClause, privateClause, firstPrivateClause, sharedClause );
}

AnnotationPtr OmpPragmaMaster::toAnnotation(conversion::ConversionFactory& fact) const {
	return std::make_shared<Master>( );
}

AnnotationPtr OmpPragmaCritical::toAnnotation(conversion::ConversionFactory& fact) const {
	const MatchMap& map = getMap();

	// checking region name (if existing)
	core::VariablePtr criticalName = NULL;
	auto fit = map.find("critical");
	if(fit != map.end()) {
		const ValueList& vars = fit->second;
		assert(vars.size() == 1 && "Critical region has multiple names");
		clang::Stmt* name = vars.front()->get<clang::Stmt*>();
		clang::DeclRefExpr* refName = dyn_cast<clang::DeclRefExpr>(name);
		assert(refName && "Clause not containing an identifier");

		criticalName = core::dynamic_pointer_cast<const core::Variable>( fact.convertExpr( *refName ) );
		assert(criticalName && "Conversion to Insieme node failed!");
	}

	return std::make_shared<Critical>( criticalName );
}

AnnotationPtr OmpPragmaBarrier::toAnnotation(conversion::ConversionFactory& fact) const {
	return std::make_shared<Barrier>( );
}

AnnotationPtr OmpPragmaTaskWait::toAnnotation(conversion::ConversionFactory& fact) const {
	return std::make_shared<TaskWait>( );
}

AnnotationPtr OmpPragmaAtomic::toAnnotation(conversion::ConversionFactory& fact) const {
	return std::make_shared<Atomic>( );
}

AnnotationPtr OmpPragmaFlush::toAnnotation(conversion::ConversionFactory& fact) const {
	// check for flush identifier list
	VarListPtr flushList = handleIdentifierList(getMap(), "flush", fact);
	return std::make_shared<Flush>( flushList );
}

AnnotationPtr OmpPragmaOrdered::toAnnotation(conversion::ConversionFactory& fact) const {
	return std::make_shared<Ordered>( );
}

AnnotationPtr OmpPragmaThreadPrivate::toAnnotation(conversion::ConversionFactory& fact) const {
	VarListPtr threadPrivateClause = handleIdentifierList(getMap(), "threadprivate", fact);
	return std::make_shared<ThreadPrivate>( threadPrivateClause );
}
} // end anonymous namespace
