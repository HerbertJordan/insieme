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

#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/XMLUni.hpp>

#include "ast_builder.h"
#include "xml_utils.h"
#include "xsd_config.h"
#include "logging.h"

using namespace insieme::core;
using namespace insieme::utils;
using namespace insieme::xml;
using namespace std;

XERCES_CPP_NAMESPACE_USE

// ------------------------------------ XStr ----------------------------

#define toUnicode(str) XStr(str).unicodeForm()

class XStr {
	::XMLCh* fUnicodeForm;
public:
	XStr(const string& toTranscode);

	~XStr();

	const ::XMLCh* unicodeForm();
};

XStr::XStr(const string& toTranscode) { fUnicodeForm = XMLString::transcode(toTranscode.c_str()); }

XStr::~XStr() { XMLString::release(&fUnicodeForm); }

const ::XMLCh* XStr::unicodeForm() { return fUnicodeForm; }


// ------------------------------------ XmlVisitor ----------------------------	

class XmlVisitor : public ASTVisitor<void> {
	DOMDocument* doc;
	XmlElement rootElem;

public:
	XmlVisitor(DOMDocument* udoc): doc(udoc), rootElem(doc->getDocumentElement()) { }
	
	~XmlVisitor() { }
	
	void visitAnnotations(const AnnotationMap& map, XmlElement& node){
		if (!map.empty()){
			XmlElement annotations("annotations", doc);
			node << annotations;

			XmlConverter& xmlConverter = XmlConverter::get();
			for(AnnotationMap::const_iterator iter = map.begin(); iter != map.end(); ++iter) {
				annotations << xmlConverter.irToDomAnnotation (*(iter->second), doc);
			}
		}
	}

	void visitGenericType(const GenericTypePtr& cur) {
		XmlElement genType("genType", doc);
		genType.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		genType.setAttr("familyName", cur->getFamilyName().getName());
		rootElem << genType;

		if (const TypePtr& base = cur->getBaseType()) {
			XmlElement baseType("baseType", doc);
			genType << baseType;

			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*base)));		
			baseType << typePtr;
			
			visitAnnotations(base.getAnnotations(), typePtr);
		}

		const vector<TypePtr>& param = cur->getTypeParameter();
		if (!param.empty()){
			XmlElement typeParams("typeParams", doc);
			genType << typeParams;
			for(vector<TypePtr>::const_iterator iter = param.begin(); iter != param.end(); ++iter) {
				XmlElement typePtr("typePtr", doc);
				typePtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));			
				typeParams << typePtr;
				
				visitAnnotations((*iter).getAnnotations(), typePtr);
			}
		}

		const vector<IntTypeParam>& intParam = cur->getIntTypeParameter();
		if (!intParam.empty()){
			XmlElement intTypeParams("intTypeParams", doc);
			genType << intTypeParams;

			const vector<IntTypeParam>& intParam = cur->getIntTypeParameter();
			for(vector<IntTypeParam>::const_iterator iter = intParam.begin(); iter != intParam.end(); ++iter) {

				XmlElement intTypeParam("intTypeParam", doc);
				intTypeParams << intTypeParam;

				switch (iter->getType()) {
				case IntTypeParam::VARIABLE: {
					XmlElement variable("variable", doc);
					variable.setAttr("value", numeric_cast<string>(iter->getSymbol()));
					intTypeParam << variable;
					break;
					}
				case IntTypeParam::CONCRETE: {
					XmlElement concrete("concrete", doc);
					concrete.setAttr("value", numeric_cast<string>(iter->getValue()));
					intTypeParam << concrete;
					break;
					}
				case IntTypeParam::INFINITE: {
					XmlElement infinite("infinite", doc);
					intTypeParam << infinite;					
					break;
					}
				default:
					XmlElement invalide("Invalid Parameter", doc);
					intTypeParam << invalide;		
					break;
				}
			}	
		}
		
		visitAnnotations(cur->getAnnotations(), genType);
	}
	
	void visitFunctionType(const FunctionTypePtr& cur){
		XmlElement functionType("functionType", doc);
		functionType.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << functionType;
		
		XmlElement argumentType("argumentType", doc);
		functionType << argumentType;
		
		if (const TupleTypePtr& argument = cur->getArgumentType()) {
			XmlElement tupleTypePtr("tupleTypePtr", doc);
			tupleTypePtr.setAttr("ref", numeric_cast<string>((size_t)(&*argument)));			
			argumentType << tupleTypePtr;

			visitAnnotations(argument.getAnnotations(), tupleTypePtr);
		}
		
		XmlElement returnType("returnType", doc);
		functionType << returnType;
		
		if (const TypePtr& returnT = cur->getReturnType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*returnT)));			
			returnType << typePtr;

			visitAnnotations(returnT.getAnnotations(), typePtr);
		}
		
		visitAnnotations(cur->getAnnotations(), functionType);
	}
	
	void visitStructType(const StructTypePtr& cur) {
		XmlElement structType("structType",doc);
		structType.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << structType;
		
		visitNamedCompositeType_(structType, cur);
		
		visitAnnotations(cur->getAnnotations(), structType);
	}
	
	void visitUnionType(const UnionTypePtr& cur) {
		XmlElement unionType("unionType", doc);
		unionType.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << unionType;
		
		visitNamedCompositeType_(unionType, cur);
		
		visitAnnotations(cur->getAnnotations(), unionType);
	}
	
	void visitNamedCompositeType_(XmlElement& el, const NamedCompositeTypePtr& cur){
		XmlElement entries("entries",doc);
		el << entries;
		
		const vector<NamedCompositeType::Entry>& entriesVec = cur->getEntries ();
		for(vector<NamedCompositeType::Entry>::const_iterator iter = entriesVec.begin(); iter != entriesVec.end(); ++iter) {
			XmlElement entry("entry", doc);
			entries << entry;
			
			XmlElement id("id", doc);
			id.setText((iter->first).getName());		
			entry << id;

			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*iter->second)));			
			entry << typePtr;
			
			visitAnnotations((iter->second).getAnnotations(), typePtr);
		}
	}

	void visitTupleType(const TupleTypePtr& cur) {
		XmlElement tupleType("tupleType", doc);
		tupleType.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << tupleType;

		XmlElement elementTypeList("elementTypeList",doc);
		tupleType << elementTypeList;

		const vector<TypePtr>& elementList = cur->getElementTypes();
		for(vector<TypePtr>::const_iterator iter = elementList.begin(); iter != elementList.end(); ++iter) {
			XmlElement elementType("elementType", doc);
			elementTypeList << elementType;
			
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));			
			elementType << typePtr;
			
			visitAnnotations(iter->getAnnotations(), typePtr);
		}
		
		visitAnnotations(cur->getAnnotations(), tupleType);
	}

	void visitTypeVariable(const TypeVariablePtr& cur) {
		XmlElement typeVariable("typeVariable", doc);
		typeVariable.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		typeVariable.setAttr("name", cur->getVarName());
		rootElem << typeVariable;
		
		visitAnnotations(cur->getAnnotations(), typeVariable);
	}
	
	void visitRecType(const RecTypePtr& cur) {
		XmlElement recType("recType", doc);
		recType.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << recType;

		XmlElement definition("definition", doc);
		recType << definition;

		if (const RecTypeDefinitionPtr& definitionT = cur->getDefinition()) {
			XmlElement recTypeDefinitionPtr("recTypeDefinitionPtr", doc);
			recTypeDefinitionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*definitionT)));			
			definition << recTypeDefinitionPtr;

			visitAnnotations(definitionT.getAnnotations(), recTypeDefinitionPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), recType);
	}
	
	void visitRecTypeDefinition(const RecTypeDefinitionPtr& cur) {
		XmlElement recTypeDefinition("recTypeDefinition", doc);
		recTypeDefinition.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << recTypeDefinition;
		
		XmlElement definitions("definitions", doc);
		recTypeDefinition << definitions;
		
		const RecTypeDefinition::RecTypeDefs& defs = cur->getDefinitions();
		for(RecTypeDefinition::RecTypeDefs::const_iterator iter = defs.begin(); iter != defs.end(); ++iter) {
			XmlElement definition("definition", doc);
			definitions << definition;
			
			XmlElement typeVariablePtr("typeVariablePtr", doc);
			typeVariablePtr.setAttr("ref", numeric_cast<string>((size_t)&(*iter->first)));
			definition << typeVariablePtr;
			
			visitAnnotations((iter->first).getAnnotations(), typeVariablePtr);
			
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)&(*iter->second)));
			definition << typePtr;
			
			visitAnnotations((iter->second).getAnnotations(), typePtr);
		}
		
		visitAnnotations(cur->getAnnotations(), recTypeDefinition);
	}

	void visitLiteral(const LiteralPtr& cur) {
		XmlElement literal("literal", doc);
		literal.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		literal.setAttr("value", cur->getValue());
		rootElem << literal;
		
		XmlElement type("type", doc);
		literal << type;
		
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}
		
		visitAnnotations(cur->getAnnotations(), literal);
	}
	
	void visitReturnStmt(const ReturnStmtPtr& cur) {
		XmlElement returnStmt("returnStmt", doc);
		returnStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << returnStmt;
		
		XmlElement returnExpression("returnExpression", doc);
		returnStmt << returnExpression;
		
		if (const ExpressionPtr& returnE = cur->getReturnExpr()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*returnE)));		
			returnExpression << expressionPtr;
			
			visitAnnotations(returnE.getAnnotations(), expressionPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), returnStmt);
	}
	
	void visitForStmt(const ForStmtPtr& cur) {
		XmlElement forStmt("forStmt", doc);
		forStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << forStmt;
		
		XmlElement declaration("declaration", doc);
		forStmt << declaration;
		
		if (const DeclarationStmtPtr& declarationR = cur->getDeclaration()) {
			XmlElement declarationStmtPtr("declarationStmtPtr", doc);
			declarationStmtPtr.setAttr("ref", numeric_cast<string>((size_t)(&*declarationR)));		
			declaration << declarationStmtPtr;
			
			visitAnnotations(declarationR.getAnnotations(), declarationStmtPtr);
		}

		XmlElement body("body", doc);
		forStmt << body;
		
		if (const StatementPtr& bodyR = cur->getBody()) {
			XmlElement statementPtr("statementPtr", doc);
			statementPtr.setAttr("ref", numeric_cast<string>((size_t)(&*bodyR)));		
			body << statementPtr;
			
			visitAnnotations(bodyR.getAnnotations(), statementPtr);
		}

		XmlElement end("end", doc);
		forStmt << end;
		
		if (const ExpressionPtr& endR = cur->getEnd()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*endR)));		
			end << expressionPtr;
			
			visitAnnotations(endR.getAnnotations(), expressionPtr);
		}
		
		XmlElement step("step", doc);
		forStmt << step;
		
		if (const ExpressionPtr& stepR = cur->getStep()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*stepR)));		
			step << expressionPtr;
			
			visitAnnotations(stepR.getAnnotations(), expressionPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), forStmt);
	}
	
	void visitIfStmt(const IfStmtPtr& cur) {
		XmlElement ifStmt("ifStmt", doc);
		ifStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << ifStmt;

		XmlElement condition("condition", doc);
		ifStmt << condition;

		if (const ExpressionPtr& cond = cur->getCondition()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*cond)));		
			condition << expressionPtr;
			
			visitAnnotations(cond.getAnnotations(), expressionPtr);
		}
		
		XmlElement thenBody("thenBody", doc);
		ifStmt << thenBody;
		
		if (const StatementPtr& thenBodyR = cur->getThenBody()) {
			XmlElement statementPtr("statementPtr", doc);
			statementPtr.setAttr("ref", numeric_cast<string>((size_t)(&*thenBodyR)));		
			thenBody << statementPtr;
			
			visitAnnotations(thenBodyR.getAnnotations(), statementPtr);
		}

		XmlElement elseBody("elseBody", doc);
		ifStmt << elseBody;
		
		if (const StatementPtr& elseBodyR = cur->getElseBody()) {
			XmlElement statementPtr("statementPtr", doc);
			statementPtr.setAttr("ref", numeric_cast<string>((size_t)(&*elseBodyR)));		
			elseBody << statementPtr;
			
			visitAnnotations(elseBodyR.getAnnotations(), statementPtr);
		}

		visitAnnotations(cur->getAnnotations(), ifStmt);
	}
	
	void visitSwitchStmt(const SwitchStmtPtr& cur) {
		XmlElement switchStmt("switchStmt", doc);
		switchStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << switchStmt;

		XmlElement expression("expression", doc);
		switchStmt << expression;
		
		if (const ExpressionPtr& expr = cur->getSwitchExpr()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*expr)));		
			expression << expressionPtr;
			
			visitAnnotations(expr.getAnnotations(), expressionPtr);
		}
		
		const vector<SwitchStmt::Case>& cas = cur->getCases();
		if (!cas.empty()){
			XmlElement cases("cases", doc);
			switchStmt << cases;
			
			for(vector<SwitchStmt::Case>::const_iterator iter = cas.begin(); iter != cas.end(); ++iter) {
				XmlElement caseEl("case", doc);
				cases << caseEl;
				
				XmlElement expressionPtr("expressionPtr", doc);
				expressionPtr.setAttr("ref", numeric_cast<string>((size_t)&(*iter->first)));
				caseEl << expressionPtr;
				
				visitAnnotations((iter->first).getAnnotations(), expressionPtr);
				
				XmlElement statementPtr("statementPtr", doc);
				statementPtr.setAttr("ref", numeric_cast<string>((size_t)&(*iter->second)));
				caseEl << statementPtr;
				
				visitAnnotations((iter->second).getAnnotations(), statementPtr);
			}
		}

		XmlElement defaultCase("defaultCase", doc);
		switchStmt << defaultCase;
		
		if (const StatementPtr& defaultC = cur->getDefaultCase()) {
			XmlElement statementPtr("statementPtr", doc);
			statementPtr.setAttr("ref", numeric_cast<string>((size_t)(&*defaultC)));		
			defaultCase << statementPtr;
			
			visitAnnotations(defaultC.getAnnotations(), statementPtr);
		}

		visitAnnotations(cur->getAnnotations(), switchStmt);
	}
	
	void visitWhileStmt(const WhileStmtPtr& cur) {
		XmlElement whileStmt("whileStmt", doc);
		whileStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << whileStmt;

		XmlElement condition("condition", doc);
		whileStmt << condition;
		
		if (const ExpressionPtr& cond = cur->getCondition()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*cond)));		
			condition << expressionPtr;
			
			visitAnnotations(cond.getAnnotations(), expressionPtr);
		}
		
		XmlElement body("body", doc);
		whileStmt << body;

		if (const StatementPtr& bodyR = cur->getBody()) {
			XmlElement statementPtr("statementPtr", doc);
			statementPtr.setAttr("ref", numeric_cast<string>((size_t)(&*bodyR)));		
			body << statementPtr;
			
			visitAnnotations(bodyR.getAnnotations(), statementPtr);
		}

		visitAnnotations(cur->getAnnotations(), whileStmt);
	}
	
	void visitBreakStmt(const BreakStmtPtr& cur) {
		XmlElement breakStmt("breakStmt", doc);
		breakStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << breakStmt;

		visitAnnotations(cur->getAnnotations(), breakStmt);
	}
	
	void visitContinueStmt(const ContinueStmtPtr& cur) {
		XmlElement continueStmt("continueStmt", doc);
		continueStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << continueStmt;

		visitAnnotations(cur->getAnnotations(), continueStmt);
	}

	void visitCompoundStmt(const CompoundStmtPtr& cur) {
		XmlElement compoundStmt("compoundStmt", doc);
		compoundStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << compoundStmt;
		
		const vector<StatementPtr>& stats = cur->getStatements();
		XmlElement statements("statements", doc);
		compoundStmt << statements;
		
		for(vector<StatementPtr>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
			XmlElement statement("statement", doc);
			statements << statement;
			
			XmlElement statementPtr("statementPtr", doc);
			statementPtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));
			statement << statementPtr;
			
			visitAnnotations((*iter).getAnnotations(), statementPtr);
		}

		visitAnnotations(cur->getAnnotations(), compoundStmt);
	}
	
	void visitNamedCompositeExpr_(XmlElement& el, const NamedCompositeExprPtr& cur){
		XmlElement type("type", doc);
		el << type;		
		
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}
		
		XmlElement members("members",doc);
		el << members;
		
		const vector<NamedCompositeExpr::Member>& membersVec = cur->getMembers ();
		for(vector<NamedCompositeExpr::Member>::const_iterator iter = membersVec.begin(); iter != membersVec.end(); ++iter) {
			XmlElement member("member", doc);
			members << member;
			
			XmlElement id("id", doc);
			id.setText((iter->first).getName());		
			member << id;

			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*iter->second)));			
			member << expressionPtr;
			
			visitAnnotations((iter->second).getAnnotations(), expressionPtr);
		}
	}
	
	void visitStructExpr(const StructExprPtr& cur) {
		XmlElement structExpr("structExpr",doc);
		structExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << structExpr;
		
		visitNamedCompositeExpr_(structExpr, cur);
		
		visitAnnotations(cur->getAnnotations(), structExpr);
	}
	
	void visitUnionExpr(const UnionExprPtr& cur) {
		XmlElement unionExpr("unionExpr", doc);
		unionExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << unionExpr;
		
		visitNamedCompositeExpr_(unionExpr, cur);
		
		visitAnnotations(cur->getAnnotations(), unionExpr);
	}
	
	void visitVectorExpr(const VectorExprPtr& cur) {
		XmlElement vectorExpr("vectorExpr", doc);
		vectorExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << vectorExpr;
		
		XmlElement type("type", doc);
		vectorExpr << type;
		
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}
		
		XmlElement expressions("expressions",doc);
		vectorExpr << expressions;
		
		const vector<ExpressionPtr>& expressionsVec = cur->getExpressions ();
		for(vector<ExpressionPtr>::const_iterator iter = expressionsVec.begin(); iter != expressionsVec.end(); ++iter) {
			XmlElement expression("expression", doc);
			expressions << expression;

			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));			
			expression << expressionPtr;
			
			visitAnnotations((*iter).getAnnotations(), expressionPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), vectorExpr);
	}
	
	void visitTupleExpr(const TupleExprPtr& cur) {
		XmlElement tupleExpr("tupleExpr", doc);
		tupleExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << tupleExpr;
		
		XmlElement type("type", doc);
		tupleExpr << type;
		
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}
		
		XmlElement expressions("expressions",doc);
		tupleExpr << expressions;
		
		const vector<ExpressionPtr>& expressionsVec = cur->getExpressions ();
		for(vector<ExpressionPtr>::const_iterator iter = expressionsVec.begin(); iter != expressionsVec.end(); ++iter) {
			XmlElement expression("expression", doc);
			expressions << expression;

			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));			
			expression << expressionPtr;
			
			visitAnnotations((*iter).getAnnotations(), expressionPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), tupleExpr);
	}
	
	void visitCastExpr(const CastExprPtr& cur) {
		XmlElement castExpr("castExpr", doc);
		castExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << castExpr;
		
		XmlElement type("type", doc);
		castExpr << type;
		
		
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}
		
		XmlElement subExpression("subExpression", doc);
		castExpr << subExpression;
		
		if (const ExpressionPtr& expressionT = cur->getSubExpression()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*expressionT)));		
			subExpression << expressionPtr;
			
			visitAnnotations(expressionT.getAnnotations(), expressionPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), castExpr);
	}
	
	void visitCallExpr(const CallExprPtr& cur) {
		XmlElement callExpr("callExpr", doc);
		callExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << callExpr;
		
		XmlElement type("type", doc);
		callExpr << type;
		
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}

		XmlElement function("function", doc);
		callExpr << function;
		
		if (const ExpressionPtr& expressionT = cur->getFunctionExpr()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*expressionT)));		
			function << expressionPtr;
			
			visitAnnotations(expressionT.getAnnotations(), expressionPtr);
		}
		
		XmlElement arguments("arguments",doc);
		callExpr << arguments;
		
		const vector<ExpressionPtr>& argumentsVec = cur->getArguments ();
		for(vector<ExpressionPtr>::const_iterator iter = argumentsVec.begin(); iter != argumentsVec.end(); ++iter) {
			XmlElement argument("argument", doc);
			arguments << argument;

			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));			
			argument << expressionPtr;
			
			visitAnnotations((*iter).getAnnotations(), expressionPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), callExpr);
	}
	
	void visitVariable(const VariablePtr& cur) {
		XmlElement variable("variable", doc);
		variable.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		variable.setAttr("identifier", numeric_cast<string>(cur->getId()));
		rootElem << variable;

		XmlElement type("type", doc);
		variable << type;
		
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}
		
		visitAnnotations(cur->getAnnotations(), variable);
	}
	
	void visitDeclarationStmt(const DeclarationStmtPtr& cur) {
		XmlElement declarationStmt("declarationStmt", doc);
		declarationStmt.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << declarationStmt;

		XmlElement variable("variable", doc);
		declarationStmt << variable;

		if (const VariablePtr& varR = cur->getVariable()) {
			XmlElement variablePtr("variablePtr", doc);
			variablePtr.setAttr("ref", numeric_cast<string>((size_t)(&*varR)));		
			variable << variablePtr;
			
			visitAnnotations(varR.getAnnotations(), variablePtr);
		}

		XmlElement expression("expression", doc);
		declarationStmt << expression;
		
		if (const ExpressionPtr& init = cur->getInitialization()) {
			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*init)));		
			expression << expressionPtr;
			
			visitAnnotations(init.getAnnotations(), expressionPtr);
		}

		visitAnnotations(cur->getAnnotations(), declarationStmt);
	}
	
	void visitJobExpr(const JobExprPtr& cur) {
		XmlElement jobExpr("jobExpr", doc);
		jobExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << jobExpr;
		
		XmlElement type("type", doc);
		jobExpr << type;
		
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}
		
		const vector<DeclarationStmtPtr>& declarationsVec = cur->getLocalDecls ();
		if (!declarationsVec.empty()){
			XmlElement declarations("declarations", doc);
			jobExpr << declarations;
			
			for(vector<DeclarationStmtPtr>::const_iterator iter = declarationsVec.begin(); iter != declarationsVec.end(); ++iter) {
				XmlElement declaration("declaration", doc);
				declarations << declaration;

				XmlElement declarationStmtPtr("declarationStmtPtr", doc);
				declarationStmtPtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));			
				declaration << declarationStmtPtr;
			
				visitAnnotations((*iter).getAnnotations(), declarationStmtPtr);
			}
		}
		
		const vector<JobExpr::GuardedStmt>& stmtsVec = cur->getGuardedStmts ();
		if (!stmtsVec.empty()){
			XmlElement guardedStatements("guardedStatements", doc);
			jobExpr << guardedStatements;
			
			for(vector<JobExpr::GuardedStmt>::const_iterator iter = stmtsVec.begin(); iter != stmtsVec.end(); ++iter) {
				XmlElement guardedStatement("guardedStatement", doc);
				guardedStatements << guardedStatement;
				
				XmlElement expressionPtr("expressionPtr", doc);
				expressionPtr.setAttr("ref", numeric_cast<string>((size_t)&(*iter->first)));
				guardedStatement << expressionPtr;
				
				visitAnnotations((iter->first).getAnnotations(), expressionPtr);
				
				XmlElement statementPtr("statementPtr", doc);
				statementPtr.setAttr("ref", numeric_cast<string>((size_t)&(*iter->second)));
				guardedStatement << statementPtr;
				
				visitAnnotations((iter->second).getAnnotations(), statementPtr);
			}
		}
		
		XmlElement defaultStatement("defaultStatement", doc);
		jobExpr << defaultStatement;
		
		if (const StatementPtr& defaultT = cur->getDefaultStmt()) {
			XmlElement statementPtr("statementPtr", doc);
			statementPtr.setAttr("ref", numeric_cast<string>((size_t)(&*defaultT)));		
			defaultStatement << statementPtr;
			
			visitAnnotations(defaultT.getAnnotations(), statementPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), jobExpr);
	}
	
	
	void visitLambdaExpr(const LambdaExprPtr& cur) {
		XmlElement lambdaExpr("lambdaExpr", doc);
		lambdaExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << lambdaExpr;
		
		XmlElement type("type", doc);
		lambdaExpr << type;

		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}
		

		XmlElement params("params",doc);
		lambdaExpr << params;
		
		const vector<VariablePtr>& parList = cur->getParams ();
					
		for(vector<VariablePtr>::const_iterator iter = parList.begin(); iter != parList.end(); ++iter) {
			XmlElement param("param", doc);
			params << param;

			XmlElement variablePtr("variablePtr", doc);
			variablePtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));			
			param << variablePtr;
		
			visitAnnotations((*iter).getAnnotations(), variablePtr);
		}
		
		XmlElement body("body", doc);
		lambdaExpr << body;
		
		if (const StatementPtr& bodyT = cur->getBody()) {
			XmlElement statementPtr("statementPtr", doc);
			statementPtr.setAttr("ref", numeric_cast<string>((size_t)(&*bodyT)));		
			body << statementPtr;
			
			visitAnnotations(bodyT.getAnnotations(), statementPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), lambdaExpr);
	}
	
	void visitProgram(const ProgramPtr& cur) {
		XmlElement program("program", doc);
		program.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << program;

		XmlElement expressions("expressions",doc);
		program << expressions;
				
		const Program::EntryPointSet& entrySet = cur->getEntryPoints ();
		for(Program::EntryPointSet::const_iterator iter = entrySet.begin(); iter != entrySet.end(); ++iter) {
			XmlElement expression("expression", doc);
			expressions << expression;

			XmlElement expressionPtr("expressionPtr", doc);
			expressionPtr.setAttr("ref", numeric_cast<string>((size_t)&*(*iter)));			
			expression << expressionPtr;
		
			visitAnnotations((*iter).getAnnotations(), expressionPtr);
		}

		visitAnnotations(cur->getAnnotations(), program);
	}
	
	void visitRecLambdaExpr(const RecLambdaExprPtr& cur) {
		XmlElement recLambdaExpr("recLambdaExpr", doc);
		recLambdaExpr.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << recLambdaExpr;

		XmlElement type("type", doc);
		recLambdaExpr << type;
				
		if (const TypePtr& typeT = cur->getType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*typeT)));		
			type << typePtr;
			
			visitAnnotations(typeT.getAnnotations(), typePtr);
		}

		XmlElement variable("variable", doc);
		recLambdaExpr << variable;
		
		if (const VariablePtr& varT = cur->getVariable()) {
			XmlElement variablePtr("variablePtr", doc);
			variablePtr.setAttr("ref", numeric_cast<string>((size_t)(&*varT)));		
			variable << variablePtr;
			
			visitAnnotations(varT.getAnnotations(), variablePtr);
		}
		
		XmlElement definition("definition", doc);
		recLambdaExpr << definition;
		
		if (const RecLambdaDefinitionPtr& recT = cur->getDefinition()) {
			XmlElement recLambdaDefinitionPtr("recLambdaDefinitionPtr", doc);
			recLambdaDefinitionPtr.setAttr("ref", numeric_cast<string>((size_t)(&*recT)));		
			definition << recLambdaDefinitionPtr;
			
			visitAnnotations(recT.getAnnotations(), recLambdaDefinitionPtr);
		}

		visitAnnotations(cur->getAnnotations(), recLambdaExpr);
	}

	void visitRecLambdaDefinition(const RecLambdaDefinitionPtr& cur) {
		XmlElement recLambdaDefinition("recLambdaDefinition", doc);
		recLambdaDefinition.setAttr("id", numeric_cast<string>((size_t)(&*cur)));
		rootElem << recLambdaDefinition;

		XmlElement definitions("definitions", doc);
		recLambdaDefinition << definitions;
				
		const RecLambdaDefinition::RecFunDefs& defs = cur->getDefinitions();
		for(RecLambdaDefinition::RecFunDefs::const_iterator iter = defs.begin(); iter != defs.end(); ++iter) {
			XmlElement definition("definition", doc);
			definitions << definition;
			
			XmlElement variablePtr("variablePtr", doc);
			variablePtr.setAttr("ref", numeric_cast<string>((size_t)&(*iter->first)));
			definition << variablePtr;
			
			visitAnnotations((iter->first).getAnnotations(), variablePtr);
			
			XmlElement lambdaExprPtr("lambdaExprPtr", doc);
			lambdaExprPtr.setAttr("ref", numeric_cast<string>((size_t)&(*iter->second)));
			definition << lambdaExprPtr;
			
			visitAnnotations((iter->second).getAnnotations(), lambdaExprPtr);
		}
		
		visitAnnotations(cur->getAnnotations(), recLambdaDefinition);
	}
};

class error_handler: public DOMErrorHandler {
	bool failed_;

public:
	error_handler () : failed_ (false) {}

	bool failed () const { return failed_; }

	virtual bool handleError (const xercesc::DOMError& e){
		bool warn (e.getSeverity() == DOMError::DOM_SEVERITY_WARNING);
		if (!warn) failed_ = true;
	
		DOMLocator* loc (e.getLocation ());
	
		char* uri (XMLString::transcode (loc->getURI ()));
		char* msg (XMLString::transcode (e.getMessage ()));
	
		cerr << uri << ":" 
			<< loc->getLineNumber () << ":" << loc->getColumnNumber () << " "
			<< (warn ? "warning: " : "error: ") << msg << endl;

		XMLString::release (&uri);
		XMLString::release (&msg);
		return true;
	}
};



// ------------------------------------ XmlElement ----------------------------

XmlElement::XmlElement(DOMElement* elem) : doc(NULL), base(elem) { }
XmlElement::XmlElement(string name, DOMDocument* doc): doc(doc), base(doc->createElement(toUnicode(name))) { }
XmlElement::XmlElement(DOMElement* elem, DOMDocument* doc) : doc(doc), base(elem) { }

DOMElement* XmlElement::getBase() const {
	return base;
}

DOMDocument* XmlElement::getDoc() const {
	return doc;
}

XmlElement& XmlElement::operator<<(XmlElement& childNode) {
	base->appendChild(childNode.base);
	return childNode;
}

XmlElement& XmlElement::operator<<(shared_ptr<XmlElement> childNode) {
	if (childNode) {
		base->appendChild(childNode->base);
		return *childNode;
	}
	return *this; // if XmlElement is NULL (annotation without registration) return the left element
}


XmlElement& XmlElement::setAttr(const string& id, const string& value) {
	base->setAttribute(toUnicode(id), toUnicode(value));
	return *this;
}

XmlElement& XmlElement::setText(const string& text) {
	assert(doc != NULL && "Attempt to create text on a root node");
	DOMNode* first = base->getFirstChild();
	bool found = false;
	while(first && !found){
		if (first->getNodeType() == 3) {
			first->setTextContent(toUnicode(text));
			found = true;
		}
		first = first->getPreviousSibling();
	}
	if (!found){
		DOMText* textNode = doc->createTextNode(toUnicode(text));
		base->appendChild(textNode);
	}
	return *this;
}

string XmlElement::getAttr(const string& id) const { // return the empty string if there is not attribute
	char* ctype (XMLString::transcode (base->getAttribute(toUnicode(id))));
	string type(ctype);
	XMLString::release(&ctype);	
	return type;
}

string XmlElement::getText() const { // return the empty string if there is no text
	DOMNode* first = base->getFirstChild();
	while(first){
		if (first->getNodeType() == 3) {
			char* ctext (XMLString::transcode(first->getNodeValue()));
			string text(ctext);
			XMLString::release(&ctext);
			return text;
		}
		first = first->getPreviousSibling();
	}
	return "";
}

string XmlElement::getName() const {
	char* cname (XMLString::transcode (base->getTagName()));
	string name(cname);
	XMLString::release(&cname);	
	return name;
}

const vector<XmlElement> XmlElement::getChildrenByName(const string& name) const {
	vector<XmlElement> vec2;
	vector<XmlElement> vec = XmlElement::getChildren();
	for (auto iter = vec.begin(); iter != vec.end(); ++iter ){
		if ((*iter).getName() == name){
			vec2.push_back(*iter);
		}
	}
	return vec2;
}

const XmlElementPtr XmlElement::getFirstChildByName(const string& name) const {
	vector<XmlElement> vec = XmlElement::getChildren();
	for (auto iter = vec.begin(); iter != vec.end(); ++iter ){
		if ((*iter).getName() == name){
			return make_shared<XmlElement>(*iter);
		}
	}
	return shared_ptr<XmlElement>();
}

const vector<XmlElement> XmlElement::getChildren() const {
	vector<XmlElement> vec;
	DOMElement* first = base->getFirstElementChild();
	while(first) {
		vec.push_back(XmlElement(first, doc));
		first = first->getNextElementSibling();
	}
	return vec;
}

// ------------------------------------ XmlConverter ----------------------------

XmlConverter& XmlConverter::get(){
	static XmlConverter instance;
	return instance;
}

shared_ptr<Annotation> XmlConverter::domToIrAnnotation (const XmlElement& el) const {
	string type = el.getAttr("type");
	DomToIrConvertMapType::const_iterator fit = DomToIrConvertMap.find(type);
	if(fit != DomToIrConvertMap.end()) {
		return (fit->second)(el);
	} else {
		DLOG(WARNING) << "Annotation \"" << type << "\" is not registred for Xml_Read!";
		return shared_ptr<Annotation>();
	}
}

shared_ptr<XmlElement> XmlConverter::irToDomAnnotation (const Annotation& ann, xercesc::DOMDocument* doc) const {
	const string& type = ann.getAnnotationName();
	IrToDomConvertMapType::const_iterator fit = IrToDomConvertMap.find(type);
	if(fit != IrToDomConvertMap.end()) {
		return (fit->second)(ann, doc);
	} else {
		DLOG(WARNING) << "Annotation \"" << type << "\" is not registred for Xml_write!";
		return shared_ptr<XmlElement>();
	}
}

void* XmlConverter::registerAnnotation(const string& name, const XmlConverter::IrToDomConvertType& toXml, const XmlConverter::DomToIrConvertType& fromXml) {
	IrToDomConvertMap.insert( std::make_pair(name, toXml) );
	DomToIrConvertMap.insert( std::make_pair(name, fromXml) );
	return NULL;
}

// ------------------------------------ XmlUtil ----------------------------

XmlUtil::XmlUtil(){
//	try {
		XMLPlatformUtils::Initialize();
		impl = DOMImplementationRegistry::getDOMImplementation(toUnicode("Core"));
		doc = NULL;
		rootElem = NULL;
		parser = NULL;
//	}
//	catch(const XMLException& toCatch)
//	{
//		char* pMsg = XMLString::transcode(toCatch.getMessage());
//		DLOG(ERROR) << "Error during Xerces-c initialization.\n" << "  Exception message:" << pMsg;
//		XMLString::release(&pMsg);
//	}
}

XmlUtil::~XmlUtil(){
	if (parser) parser->release();
	if (doc) doc->release();
	XMLPlatformUtils::Terminate();
}

void XmlUtil::convertXmlToDom(const string fileName, const bool validate){
	((DOMImplementationLS*)impl)->createLSSerializer();
	parser = ((DOMImplementationLS*)impl)->createLSParser (DOMImplementationLS::MODE_SYNCHRONOUS, 0);
	
	// remove the old DOM
	if (doc) {
		doc->release();
		doc = NULL;
	}
	
	if (parser) {
		DOMConfiguration* conf (parser->getDomConfig ());

		conf->setParameter (XMLUni::fgDOMComments, false);
		conf->setParameter (XMLUni::fgDOMDatatypeNormalization, true);
		conf->setParameter (XMLUni::fgDOMEntities, false);
		conf->setParameter (XMLUni::fgDOMNamespaces, true);
		conf->setParameter (XMLUni::fgDOMElementContentWhitespace, false);

		// Enable validation.
		conf->setParameter (XMLUni::fgDOMValidate, validate);
		conf->setParameter (XMLUni::fgXercesSchema, validate);
		conf->setParameter (XMLUni::fgXercesSchemaFullChecking, false);

		// Use the loaded grammar during parsing.
		conf->setParameter (XMLUni::fgXercesUseCachedGrammarInParse, true);

		// Don't load schemas from any other source
		conf->setParameter (XMLUni::fgXercesLoadSchema, false);

		// We will release the DOM document ourselves.
		conf->setParameter (XMLUni::fgXercesUserAdoptsDOMDocument, true);

		error_handler eh;
		parser->getDomConfig ()->setParameter (XMLUni::fgDOMErrorHandler, &eh);
		
		if (validate) {
			if (!parser->loadGrammar ((XML_SCHEMA_DIR + "schema.xsd").c_str(), Grammar::SchemaGrammarType, true)) {
				cerr << "ERROR: Unable to load schema.xsd" << endl;
				return;
			}

			if (eh.failed ()){
				return;
			}
		}
		if (doc) doc->release();
		doc = parser->parseURI(fileName.c_str());
		if (eh.failed ()){
			doc->release();
			doc = NULL;
			cerr << "ERROR: found problem during parsing" << endl;
			return;
		}
	}
}

void XmlUtil::convertDomToXml(const string outputFile){
	DOMImplementationLS* implLS = dynamic_cast<DOMImplementationLS*>(impl);
	assert(implLS);
	DOMLSSerializer   *theSerializer = implLS->createLSSerializer();
	DOMLSOutput       *theOutputDesc = implLS->createLSOutput();
	DOMConfiguration* serializerConfig = theSerializer->getDomConfig();
	
	if (serializerConfig->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true))
		serializerConfig->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);
		
	XMLFormatTarget* myFormTarget = NULL;
	if (!outputFile.empty()){
		myFormTarget = new LocalFileFormatTarget(outputFile.c_str());
	} else {
		myFormTarget = new StdOutFormatTarget();
	}

	theOutputDesc->setByteStream(myFormTarget);
	theSerializer->write(doc, theOutputDesc);

	theOutputDesc->release();
	theSerializer->release();
	delete myFormTarget;
}

namespace { // begin namespace
typedef map<string, pair <const XmlElement*, NodePtr>> elemMapType;

void buildAnnotations(const XmlElementPtr, const NodePtr, const bool);
void buildNode(NodeManager&, const XmlElement&, elemMapType&);
void checkRef(NodeManager&, const XmlElement&, elemMapType&);

void buildAnnotations(const XmlElement& type, const NodePtr baseType, const bool value){
	XmlElementPtr annotations = type.getFirstChildByName("annotations");
	if (annotations){
		XmlConverter& xmlConverter = XmlConverter::get();
		vector<XmlElement> ann = annotations->getChildrenByName("annotation");
		for(vector<XmlElement>::const_iterator iter = ann.begin(); iter != ann.end(); ++iter) {
			if (value)
				baseType.addAnnotation(xmlConverter.domToIrAnnotation(*iter));
			else
				baseType->addAnnotation(xmlConverter.domToIrAnnotation(*iter));
		}
	}
}
	
void buildNode(NodeManager& manager, const XmlElement& elem, elemMapType& elemMap) {
	checkRef(manager, elem, elemMap);
	
	// different types of nodes
	if (elem.getName() == "genType") {
		TypePtr baseType = NULL;
		XmlElementPtr base = elem.getFirstChildByName("baseType");
		if (base){
			XmlElementPtr type = base->getFirstChildByName("typePtr");
			baseType = dynamic_pointer_cast<const Type>(elemMap[type->getAttr("ref")].second);
			
			buildAnnotations(*type, baseType, true);
		}
		
		vector<TypePtr> typeParams;
		XmlElementPtr param = elem.getFirstChildByName("typeParams");
		if (param){
			vector<XmlElement> types = param->getChildrenByName("typePtr");
			for(vector<XmlElement>::const_iterator iter = types.begin(); iter != types.end(); ++iter) {
				TypePtr typeParam = dynamic_pointer_cast<const Type>(elemMap[iter->getAttr("ref")].second);
				buildAnnotations(*iter, typeParam, true);
				typeParams.push_back(typeParam);
			}
		}

		vector<IntTypeParam> intTypeParams;
		XmlElementPtr intParam = elem.getFirstChildByName("intTypeParams");
		if (intParam){
			vector<XmlElement> intPar = intParam->getChildrenByName("intTypeParam");
			for(vector<XmlElement>::const_iterator iter = intPar.begin(); iter != intPar.end(); ++iter) {
				if (iter->getChildrenByName("variable").size() != 0){
					intTypeParams.push_back(IntTypeParam::getVariableIntParam((iter->getFirstChildByName("variable"))->getAttr("value")[0]));
				}
				else if (iter->getChildrenByName("concrete").size() != 0){
					intTypeParams.push_back(IntTypeParam::getConcreteIntParam((iter->getFirstChildByName("concrete"))->getAttr("value")[0]));
				}
				else {
					intTypeParams.push_back(IntTypeParam::getInfiniteIntParam());
				}
			}
		}
		
		GenericTypePtr gen = GenericType::get(manager, elem.getAttr("familyName"), typeParams, intTypeParams, baseType);
		buildAnnotations(elem, gen, false);
		
		// update the map
		string id = elem.getAttr("id");
		pair <const XmlElement*, NodePtr> oldPair = elemMap[id];
		elemMap[id] = make_pair(oldPair.first, gen);
	}
	else if (elem.getName() == "functionType") {
		/*XmlElementPtr type = elem.getFirstChildByName("argumentType")->getFirstChildByName("typePtr");
		TypePtr argType = dynamic_pointer_cast<const Type>(elemMap[type->getAttr("ref")].second);
		buildAnnotations(*type, argType, true);
		
		type = elem.getFirstChildByName("returnType")->getFirstChildByName("typePtr");
		TypePtr retType = dynamic_pointer_cast<const Type>(elemMap[type->getAttr("ref")].second);
		buildAnnotations(*type, retType, true);
		
		//GenericTypePtr gen = GenericType::get(manager, elem.getAttr("familyName"), typeParams, intTypeParams, baseType);
		buildAnnotations(elem, fun, false);
		
		// update the map
		string id = elem.getAttr("id");
		pair <const XmlElement*, NodePtr> oldPair = elemMap[id];
		elemMap[id] = make_pair(oldPair.first, gen);*/
		
	}
	/*
		
		XmlElement returnType("returnType", doc);
		functionType << returnType;
		
		if (const TypePtr& returnT = cur->getReturnType()) {
			XmlElement typePtr("typePtr", doc);
			typePtr.setAttr("ref", numeric_cast<string>((size_t)(&*returnT)));			
			returnType << typePtr;

			visitAnnotations(returnT.getAnnotations(), typePtr);
		}
		
		visitAnnotations(cur->getAnnotations(), functionType);
	}
	*/
	else if (elem.getName() == "rootNode") {
		XmlElementPtr type = elem.getFirstChildByName("nodePtr");
		buildAnnotations(*type, elemMap[type->getAttr("ref")].second, true);
	}
}

void checkRef(NodeManager& manager, const XmlElement& elem, elemMapType& elemMap) {
	string id = elem.getAttr("ref");
	if (id.size() != 0){
		buildNode(manager, *(elemMap[id].first), elemMap);
	}
	
	vector<XmlElement> children = elem.getChildren();
	for(vector<XmlElement>::const_iterator iter = children.begin(); iter != children.end(); ++iter) {
		checkRef(manager, *iter, elemMap);
	}
}

} // end namespace

NodePtr XmlUtil::convertDomToIr(NodeManager& manager){
	elemMapType elemMap;
	
	XmlElement inspire(doc->getDocumentElement(), doc);
	
	vector<XmlElement> elemVec = inspire.getChildren();
	for(vector<XmlElement>::const_iterator iter = elemVec.begin(); iter != elemVec.end(); ++iter) {
		elemMap[iter->getAttr("id")] = make_pair(&(*iter), NodePtr());
	}
	
	XmlElementPtr root = inspire.getFirstChildByName("rootNode");
	
	assert ( root && "RootNode is not present in DOM");
	
	buildNode(manager, *root, elemMap);
	
	return elemMap[(*root->getFirstChildByName("nodePtr")).getAttr("ref")].second;
}

void XmlUtil::convertIrToDom(const NodePtr& node){
	if (doc) {
		doc->release();
		doc = NULL;
	}
	doc = impl->createDocument(0, toUnicode("inspire"),0);
	XmlElement rootNode("rootNode", doc);
	(doc->getDocumentElement())->appendChild(rootNode.getBase());
	
	XmlElement nodePtr ("nodePtr", doc);
	nodePtr.setAttr("ref", numeric_cast<string>((size_t)(&*node)));
	rootNode << nodePtr;

	AnnotationMap map = node.getAnnotations();
	if (!map.empty()){
		XmlElement annotations("annotations", doc);
		nodePtr << annotations;
		
		XmlConverter& xmlConverter = XmlConverter::get();
		for(AnnotationMap::const_iterator iter = map.begin(); iter != map.end(); ++iter) {
			annotations << xmlConverter.irToDomAnnotation (*(iter->second), doc);
		}
	}
	
	XmlVisitor visitor(doc);
	visitAllOnce(node, visitor);
}

string XmlUtil::convertDomToString(){
	if (doc){
		DOMImplementationLS* implLS = dynamic_cast<DOMImplementationLS*>(impl);
		DOMLSSerializer*	theSerializer = implLS->createLSSerializer();
		string stringTemp = XMLString::transcode (theSerializer->writeToString(doc));
		theSerializer->release();
		
		string stringDump = "";
		for (string::iterator it = stringTemp.begin() ; it < stringTemp.end(); ++it){
	    	if (!isspace (*it))
	      		stringDump += *it;
		}
		return stringDump;
	}
	throw "DOM is empty";
}



// -------------------------Xml Write - Read - Validate----------------------

void insieme::xml::xmlWrite(const NodePtr& node, const string fileName) {
	XmlUtil xml;
	xml.convertIrToDom(node);
	xml.convertDomToXml(fileName);
};

void insieme::xml::xmlRead(NodeManager& manager, const string fileName, const bool validate) {
	XmlUtil xml;
	xml.convertXmlToDom(fileName, validate);
	xml.convertDomToIr(manager);
};

void insieme::xml::xmlValidate(const string fileName) {
	XmlUtil xml;
	xml.convertXmlToDom(fileName, true);
};
