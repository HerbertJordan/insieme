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

#include "insieme/core/ir_builder.h"
#include "insieme/core/analysis/ir_utils.h"

#include "insieme/transform/datalayout/parallelSecAtt.h"
#include "insieme/transform/datalayout/datalayout_utils.h"


namespace insieme {
namespace transform {
namespace datalayout {

using namespace core;
//namespace pirp = pattern::irp;
//namespace ia = insieme::analysis;
utils::map::PointerMap<ExpressionPtr, RefTypePtr> ParSecAtt::findCandidates(NodeAddress toTransform) {
	utils::map::PointerMap<ExpressionPtr, RefTypePtr> structs;

	ExpressionMap jobReplacements;
	IRBuilder builder(mgr);

	core::visitBreadthFirst(toTransform, [&](const ExpressionAddress& expr) {
		// adding arguments which use a tuple member expression as argument which's tuple member has been replaced already to replace list
		if(CallExprAddress call = expr.isa<CallExprAddress>()) {
			if(!core::analysis::isCallOf(call.getAddressedNode(), mgr.getLangBasic().getTupleMemberAccess()))
				return;
std::cout << "\nat the tuple member access\n";

			// check if tuple argument has a member which will be updated
			ExpressionPtr oldRootVar = getRootVariable(call, call->getArgument(0)).as<ExpressionPtr>();
			auto newRootVarIter = varsToPropagate.find(oldRootVar);

			if(newRootVarIter != varsToPropagate.end()) { // tuple has been updated, check if it was the current field
				ExpressionPtr newRootVar = *newRootVarIter;

				RefTypePtr newType = getBaseType(newRootVar->getType()).as<TupleTypePtr>()->getElement(
						call->getArgument(1).as<LiteralPtr>()->getValueAs<unsigned>()).as<RefTypePtr>();
				TypePtr oldType = call->getArgument(2)->getType().as<GenericTypePtr>()->getTypeParameter(0);
std::cout << "Creating var of new type: " << newType << std::endl;

				// check if and update is needed
				if(newType == oldType)
					return;

				ExpressionAddress argument = call.getParentAddress(2).isa<CallExprAddress>();
				CallExprAddress parent = argument.getParentAddress(1).isa<CallExprAddress>();
				if(!parent)
					return;

				LambdaExprAddress lambda = parent->getFunctionExpr().isa<LambdaExprAddress>();

				if(!lambda)
					return;

				for_range(make_paired_range(parent->getArguments(), lambda->getLambda()->getParameters()->getElements()),
						[&](const std::pair<const core::ExpressionAddress, const core::VariableAddress>& pair) {
					if(pair.first == argument) {// found the argument which will be updated
						// create replacement for corresponding parameter
						TypePtr newParamType = newType->getElementType().as<ArrayTypePtr>()->getElementType();
						VariablePtr newParam = builder.variable(newParamType);

						// add corresponding parameter to update list
						jobReplacements[pair.second] = newParam;
std::cout << ": \nAdding: " << *pair.second << " - " << newParamType << std::endl;
					}
				});
			}
		}

		// propagating variables to be replaced through job expressions
		if(JobExprPtr job = expr.isa<JobExprPtr>()) {
			CallExprPtr parallelCall = job->getDefaultExpr().isa<BindExprPtr>()->getCall();

			if(!parallelCall)
				return;

			LambdaExprPtr parallelLambda = parallelCall->getFunctionExpr().isa<LambdaExprPtr>();
			if(!parallelLambda)
				return;

			for_range(make_paired_range(parallelCall->getArguments(), parallelLambda->getParameterList()->getElements()),
					[&](const std::pair<const ExpressionPtr, const VariablePtr>& pair) {
std::cout << "Looking for " << pair.first << std::endl;
				auto newArgIter = varsToPropagate.find(pair.first);
				if(newArgIter != varsToPropagate.end()) {
					jobReplacements[pair.second] = builder.variable((*newArgIter)->getType());
	std::cout << "Found in VARreplacements: " << pair.first << " -> " << jobReplacements[pair.second]->getType() << std::endl;
				}

				auto newArgIter1 = jobReplacements.find(pair.first);
				if(newArgIter1 != jobReplacements.end()) {
					jobReplacements[pair.second] = builder.variable(newArgIter1->second->getType());
	std::cout << "Found in jobREPLACEMENTS: " << pair.first << " -> " << jobReplacements[pair.second]->getType() << std::endl;
				}
			});
		}
	});

	return structs;
}

ParSecAtt::ParSecAtt(core::NodePtr& toTransform, ExpressionSet& varsToPropagate, const StructTypePtr& newStructType, const StructTypePtr& oldStructType)
		: AosToTaos(toTransform), varsToPropagate(varsToPropagate), newStructType(newStructType), oldStructType(oldStructType) {}

//utils::map::PointerMap<core::ExpressionPtr, core::RefTypePtr> OclAts::findCandidates(core::NodeAddress toTransform) {
//	utils::map::PointerMap<core::ExpressionPtr, core::RefTypePtr> retVal;
//
//	return retVal;
//}
//core::StructTypePtr OclAts::createNewType(core::StructTypePtr oldType) {
//	return StructTypePtr();
//}

void ParSecAtt::transform() {
	std::vector<std::pair<ExpressionSet, RefTypePtr>> toReplaceLists = createCandidateLists();
}


} // datalayout
} // transform
} // insieme