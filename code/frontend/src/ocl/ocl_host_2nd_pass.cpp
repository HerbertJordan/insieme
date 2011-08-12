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

#include "insieme/annotations/c/naming.h"
#include "insieme/annotations/ocl/ocl_annotations.h"
#include "insieme/frontend/ocl/ocl_host_utils.h"
#include "insieme/frontend/ocl/ocl_host_2nd_pass.h"

namespace ba = boost::algorithm;

namespace insieme {
namespace frontend {
namespace ocl {
using namespace insieme::core;

void Host2ndPass::mapNamesToLambdas(const vector<ExpressionPtr>& kernelEntries)
{
	std::cout << "kernelNames:\n" << kernelNames << std::endl;
	std::map<string, int> checkDuplicates;
	for_each(kernelEntries, [&](ExpressionPtr entryPoint) {
			if(const LambdaExprPtr& lambdaEx = dynamic_pointer_cast<const LambdaExpr>(entryPoint))
			if(auto cname = lambdaEx->getLambda()->getAnnotation(annotations::c::CNameAnnotation::KEY)) {
				assert(checkDuplicates[cname->getName()] == 0 && "Multiple kernels with the same name not supported");
				checkDuplicates[cname->getName()] = 1;

				if(ExpressionPtr clKernel = kernelNames[cname->getName()]) {
					kernelLambdas[clKernel] = lambdaEx;
				}
			}
		});
}

ClmemTable& Host2ndPass::getCleanedStructures() {
	for_each(cl_mems, [&](std::pair<const VariablePtr, VariablePtr>& var) {
			if(StructTypePtr type = dynamic_pointer_cast<const StructType>(getNonRefType(var.second))) {
				// delete all unneccessary cl_* fields form the struct
				StructType::Entries entries = type->getEntries(); // actual fields of the struct
				StructType::Entries newEntries;

				for_each(entries, [&](std::pair<IdentifierPtr, TypePtr>& entry) {
							if(entry.second->toString().find("_cl_") == string::npos) {
								newEntries.push_back(entry);
							}
						});

				// update struct in replacement map
				NodePtr replacement = builder.variable(builder.refType(builder.structType(newEntries)));
				copyAnnotations(var.second, replacement);
				var.second = static_pointer_cast<const Variable>(replacement);
			}
		});

	return cl_mems;
}

} //namespace ocl
} //namespace frontend
} //namespace insieme
