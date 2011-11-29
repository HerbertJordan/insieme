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

#include "insieme/frontend/omp/omp_utils.h"

#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/lang/basic.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/utils/set_utils.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/annotation.h"

namespace insieme {
namespace frontend {
namespace omp {
	
using namespace std;
using namespace core;
using namespace utils::log;

const string GlobalRequiredAnnotation::name = "GlobalRequiredAnnotation";
const utils::StringKey<GlobalRequiredAnnotation> GlobalRequiredAnnotation::key("GlobalRequiredAnnotation");

bool GlobalRequiredAnnotation::migrate(const NodeAnnotationPtr& ptr, const NodePtr& before, const NodePtr& after) const {
	after->addAnnotation(ptr);
	return true;
}

StructExpr::Members markGlobalUsers(const core::ProgramPtr& prog) {
	NodeManager& nodeMan = prog->getNodeManager();
	IRBuilder build(nodeMan);
	boost::unordered_set<std::string> handledGlobals;
	StructExpr::Members retval;
	auto anno = std::make_shared<GlobalRequiredAnnotation>();
	visitDepthFirst(ProgramAddress(prog), [&](const LiteralAddress& lit) {
		const string& gname = lit->getStringValue();
		if(gname.find("global_omp") == 0) {
			// add global to set if required
			if(handledGlobals.count(gname) == 0) {
				ExpressionPtr initializer;
				if(nodeMan.getLangBasic().isLock(lit.getAddressedNode()->getType())) {
					initializer = build.createLock();
				} else assert(false && "Unsupported OMP global type");
				retval.push_back(build.namedValue(gname, initializer));
				handledGlobals.insert(gname);
			}
			// mark upward path from global
			auto pathMarker = makeLambdaVisitor([&](const NodeAddress& node) { node->addAnnotation(anno); });
			visitPathBottomUp(lit, pathMarker);
		}
	});
	return retval;
}

const NodePtr GlobalMapper::mapElement(unsigned index, const NodePtr& ptr) {
	// only start mapping once global is encountered
	if(*ptr == *curVar) startedMapping = true;
	if(!startedMapping) return ptr->substitute(nodeMan, *this);
	if(ptr->hasAnnotation(GlobalRequiredAnnotation::key)) {
		//LOG(INFO) << "?????? Mapping 1 T: " << getNodeTypeName(ptr->getNodeType()) << " -- N: " << *ptr;
		// recursively forward the variable
		switch(ptr->getNodeType()) {
		case NT_CallExpr:
			return mapCall(static_pointer_cast<const CallExpr>(ptr));
			break;
		case NT_BindExpr:
			return mapBind(static_pointer_cast<const BindExpr>(ptr));
			break;
		//case NT_JobExpr:
		//	return mapJob(static_pointer_cast<const JobExpr>(ptr));
		//	break;
		case NT_LambdaExpr:
			return cache.get(static_pointer_cast<const LambdaExpr>(ptr));
			break;
		case NT_Literal:
			return mapLiteral(static_pointer_cast<const Literal>(ptr));
			break;
		default:
			// no changes at this node, but at child nodes
			return ptr->substitute(nodeMan, *this);
		}
	} else {
		//LOG(INFO) << "?????? Mapping 2 " << ptr;
		// no changes required
		return ptr;
	}
}

const NodePtr GlobalMapper::mapCall(const CallExprPtr& call) {
	//LOG(INFO) << "?????? Mapping call " << call;
	auto func = call->getFunctionExpr();
	if(func && func->hasAnnotation(GlobalRequiredAnnotation::key)) {
		vector<ExpressionPtr> newCallArgs = call->getArguments();
		newCallArgs.push_back(curVar);
		// complete call
		return build.callExpr(call->getType(), static_pointer_cast<const Expression>(map(func)), newCallArgs);
	}
	return call->substitute(nodeMan, *this);
}

const NodePtr GlobalMapper::mapBind(const BindExprPtr& bind) {
	//LOG(INFO) << "?????? Mapping bind " << bind;
	auto boundCall = bind->getCall();
	auto boundCallFunc = boundCall->getFunctionExpr();
	if(boundCallFunc && boundCallFunc->hasAnnotation(GlobalRequiredAnnotation::key)) {
		vector<ExpressionPtr> newBoundCallArguments = boundCall->getArguments();
		newBoundCallArguments.push_back(curVar);
		auto newFunctionExpr = this->map(boundCallFunc);
		auto newCall = build.callExpr(boundCall->getType(), newFunctionExpr, newBoundCallArguments);
		return build.bindExpr(static_pointer_cast<FunctionTypePtr>(bind->getType()), bind->getParameters(), newCall);
	}
	return bind->substitute(nodeMan, *this);
}

//const core::NodePtr mapJob(const core::JobExprPtr& bind) {
//
//}

const NodePtr GlobalMapper::mapLambdaExpr(const LambdaExprPtr& lambdaExpr) {
	LambdaExprPtr ret;
	//LOG(INFO) << "?????? Mapping lambda expr " << lambdaExpr;
	auto lambdaDef = lambdaExpr->getDefinition();
	auto lambda = lambdaExpr->getLambda();
	// create new var for global struct
	VariablePtr innerVar = build.variable(curVar->getType());
	VariablePtr outerVar = curVar;
	curVar = innerVar;
	// map body
	auto newBody = static_pointer_cast<const CompoundStmt>(map(lambda->getBody()));
	// add param
	VariableList newParams = lambda->getParameterList();
	newParams.push_back(curVar);
	// build replacement lambda
	TypePtr retType = lambda->getType()->getReturnType();
	//TypeList paramTypes = ::transform(newParams, [&](const VariablePtr& v){ return v->getType(); });
	//FunctionTypePtr lambdaType = build.functionType(paramTypes, retType);
	//auto newLambda = build.lambda(lambdaType, newParams, newBody);
	// restore previous global variable
	curVar = outerVar;
	// build replacement lambda expression
	ret = build.lambdaExpr(retType, newBody, newParams);
	//LOG(INFO) << "!!!!!!! Mapped lambda expr " << ret;
	return ret;
}

const NodePtr GlobalMapper::mapLiteral(const LiteralPtr& literal) {
	const string& gname = literal->getStringValue();
	if(gname.find("global_omp") == 0) {
		return build.accessMember(curVar, gname);
	}

	return literal;
}

} // namespace omp
} // namespace frontend
} // namespace insieme
