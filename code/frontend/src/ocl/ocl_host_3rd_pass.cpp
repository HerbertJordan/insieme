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

#include "insieme/frontend/ocl/ocl_host_utils.h"
#include "insieme/frontend/ocl/ocl_host_passes.h"
#include "insieme/core/transform/node_replacer.h"


namespace ba = boost::algorithm;

namespace insieme {
namespace frontend {
namespace ocl {
using namespace insieme::core;

/*
void HostMapper3rdPass::getVarOutOfCrazyInspireConstruct(core::ExpressionPtr& arg) {
// remove stuff added by (void*)&
	if(const CallExprPtr& refToAnyref = dynamic_pointer_cast<const CallExpr>(arg))
		if(refToAnyref->getFunctionExpr() == BASIC.getRefToAnyRef())
			if(const CallExprPtr& scalarToArray = dynamic_pointer_cast<const CallExpr>(refToAnyref->getArgument(0)))
				if(scalarToArray->getFunctionExpr() == BASIC.getScalarToArray())
					arg = scalarToArray->getArgument(0);
}
*/
/* Assumptions:
 * 1. the work dimension is a scalar in the arguments
 * 2. The cast to void* of the local/global size happens in the argument
*/

const ExpressionPtr HostMapper3rdPass::anythingToVec3(ExpressionPtr workDim, ExpressionPtr size) {
	const TypePtr vecTy = builder.vectorType(BASIC.getUInt4(), builder.concreteIntTypeParam(static_cast<size_t>(3)));
	TypePtr argTy;
	VariablePtr param;
	ExpressionPtr arg;
	unsigned int wd;

	if(const CastExprPtr& cast = dynamic_pointer_cast<const CastExpr>(workDim)) {
		workDim = cast->getSubExpression();
	}

	// check work dimension
	const LiteralPtr& dim = dynamic_pointer_cast<const Literal>(workDim);
	assert(dim && "Cannot determine work_dim of clEnqueueNDRangeKernel. Should be a literal!");
	wd = atoi(dim->getValue().c_str());
	//    std::cout << "*****************WorkDim: " << dim->getValue() << std::endl;
	assert(workDim < 3u && "Invalid work_dim. Should be 1 - 3!");

	// check if there is a x to array called
	if(const CallExprPtr& toArray = dynamic_pointer_cast<const CallExpr>(size)) {
		if(toArray->getFunctionExpr() == BASIC.getScalarToArray()) {
			// check consitency with workDim, should be 1
			assert(wd == 1 && "Scalar group size passed to a multi dimensional work_dim");

			argTy = toArray->getArgument(0)->getType();
			param = builder.variable(argTy);
			arg = toArray->getArgument(0);
		} else if(toArray->getFunctionExpr() == BASIC.getRefVectorToRefArray()) {
			argTy = toArray->getArgument(0)->getType();
			param = builder.variable(argTy);
			arg = toArray->getArgument(0);
		} else {
			std::cerr << "Unexpected Function: " << toArray << " of type " << toArray->getArgument(0)->getType() << std::endl;
			assert(false && "Unexpected function in OpenCL size argument");
		}
	} else { // the argument is an array
		size = tryDeref(size, builder);
		assert(size->getType()->getNodeType() == NT_ArrayType && "Called clEnqueueNDRangeKernel with invalid group argument");
		argTy = size->getType();
		param = builder.variable(argTy);
		arg = size;
	}

	ExpressionPtr init = param;

	if(RefTypePtr ref = dynamic_pointer_cast<const RefType>(param->getType())) {
		init = builder.deref(param);
		//        argTy = ref->getElementType();
	}

	TypePtr fieldTy;
	if(const ArrayTypePtr& array = dynamic_pointer_cast<const ArrayType>(init->getType()))
	fieldTy = array->getElementType();

	if(const VectorTypePtr& vector = dynamic_pointer_cast<const VectorType>(init->getType()))
	fieldTy = vector->getElementType();

	DeclarationStmtPtr vDecl;
	if(wd == 1) {
		if(fieldTy)
		init = builder.callExpr(fieldTy, BASIC.getArraySubscript1D(), init, builder.literal(BASIC.getUInt8(), "0"));
		if(init->getType() != BASIC.getUInt4()) {
			init = builder.castExpr(BASIC.getUInt4(), init);
		}
		vDecl = builder.declarationStmt(vecTy,
				builder.vectorExpr(toVector<ExpressionPtr>(init, builder.literal(BASIC.getUInt4(), "1"), builder.literal(BASIC.getUInt4(), "1"))));
	} else {
		assert(fieldTy && "Size argument of multidimensional group is no vector or array");

		vector<ExpressionPtr> subscripts;
		subscripts.push_back(builder.callExpr(fieldTy, BASIC.getArraySubscript1D(), init, builder.literal(BASIC.getUInt8(), "0")));
		subscripts.push_back(builder.callExpr(fieldTy, BASIC.getArraySubscript1D(), init, builder.literal(BASIC.getUInt8(), "1")));
		subscripts.push_back(wd == 3 ? (ExpressionPtr)builder.callExpr(fieldTy, BASIC.getArraySubscript1D(), init, builder.literal(BASIC.getUInt8(), "2")) :
				(ExpressionPtr)builder.literal(BASIC.getUInt4(), "1"));

		for_each(subscripts, [&](ExpressionPtr& r) {
					if(r->getType() != BASIC.getUInt4())
					r = builder.castExpr(BASIC.getUInt4(), r);
				});

		vDecl = builder.declarationStmt(vecTy, builder.vectorExpr(subscripts));
	}

	FunctionTypePtr fctTy = builder.functionType(toVector(argTy), vecTy);
	return builder.callExpr(vecTy, builder.lambdaExpr(fctTy, toVector(param) , builder.compoundStmt(vDecl,
							builder.returnStmt(vDecl->getVariable()))), arg);
}

const NodePtr HostMapper3rdPass::resolveElement(const NodePtr& element) {
	// stopp recursion at type level
	if (element->getNodeCategory() == NodeCategory::NC_Type) {
		return element;//->substitute(builder.getNodeManager(), *this);
	}

	// TODO make more efficient
	if(replacements.find(element) != replacements.end())
		return replacements[element];

	if(const VariablePtr& var = dynamic_pointer_cast<const Variable>(element)) {
		if(cl_mems.find(var) != cl_mems.end()) {
			return cl_mems[var];
		}
	}

	if(const DeclarationStmtPtr& decl = dynamic_pointer_cast<const DeclarationStmt>(element)) {
		const VariablePtr& var = decl->getVariable();
		if(cl_mems.find(var) != cl_mems.end()) {
			if(const StructTypePtr& sType = dynamic_pointer_cast<const StructType>(getNonRefType(cl_mems[var]))) {
				// throw elements which are not any more in the struct out of the initialization expression and update init values for the remaining ones
				// look into ref.new
				if(const CallExprPtr& refNew = dynamic_pointer_cast<const CallExpr>(decl->getInitialization())) {
					if(refNew->getFunctionExpr() == BASIC.getRefNew() || refNew->getFunctionExpr() == BASIC.getRefVar()) {
						if(const StructExprPtr& oldInit = dynamic_pointer_cast<const StructExpr>(refNew->getArgument(0))) {
							core::StructExpr::Members newInitMembers;
							core::NamedCompositeType::Entries newMembers = sType->getEntries();
							size_t i = 0;

							for_each(oldInit->getMembers(), [&](core::StructExpr::Member oldInitMember) {
								// assuming that the order of the (exisiting) elements in newMembers and oldMember is the same,
								// we always have to compare only one element
								if(newMembers.size() > i && oldInitMember.first == newMembers.at(i).first) {
									// check if the type of the init expression is the same as the type of the field (type of field may changed)
									if(newMembers.at(i).second != oldInitMember.second->getType()) {
										// always init as undefined in this case
										const TypePtr& initType = newMembers.at(i).second;
										core::StructExpr::Member newInitMember = std::make_pair(oldInitMember.first,
												builder.callExpr(BASIC.getUndefined(), BASIC.getTypeLiteral(initType)));
										newInitMembers.push_back(newInitMember);
									} else
										newInitMembers.push_back(oldInitMember);
									++i;
								}
							});
							// create a new Declaration Statement which's init expression contains only the remaining fields
							return builder.declarationStmt(cl_mems[var], builder.refNew(builder.structExpr(newInitMembers)));
						}
					}
				}
			}


			// check if the init funcition of the variable is still of cl_mem type
			TypePtr oldType = decl->getInitialization()->getType();

			if(const CallExprPtr& initFct = dynamic_pointer_cast<const CallExpr>(this->resolveElement(decl->getInitialization()))) {
				LiteralPtr typeLiteral;
				if(const CallExprPtr& undefined = dynamic_pointer_cast<const CallExpr>(initFct->getArgument(0))) { // default init
					if(undefined->getFunctionExpr() == BASIC.getUndefined()) {
						typeLiteral = dynamic_pointer_cast<const Literal>(undefined->getArgument(0));
						assert(typeLiteral && "Unexpected argument when initializing variable with undefined");
						oldType = static_pointer_cast<const Type>(typeLiteral->getType()->getChildList().at(0));
					}
				}

				if(const CastExprPtr& castToClMem = dynamic_pointer_cast<const CastExpr>(initFct->getArgument(0))) {
					oldType = (castToClMem->getType());
					assert(oldType && "Unexpected argument when initializing variable by cast");
				}

			}


			if(oldType->toString().find("array<_cl_mem,1>") != string::npos) {
				// get new element type
				while(const SingleElementTypePtr & interType = dynamic_pointer_cast<const SingleElementType>(oldType) )
					oldType = interType->getElementType();

//				if(initFct->getArgument(0) == builder.callExpr(BASIC.getUndefined(), BASIC.getTypeLiteral(builder.arrayType(builder.genericType("_cl_mem")))) // default init
//						|| initFct->getArgument(0) == builder.castExpr(builder.arrayType(builder.genericType("_cl_mem")),
//								builder.callExpr(BASIC.getGetNull(), BASIC.getTypeLiteral(BASIC.getInt8())))) { // init with NULL
					// overwrite default initialization to cl_mem with default initialization to array<whatever>
					TypePtr newType = dynamic_pointer_cast<const Type>(core::transform::replaceAll(builder.getNodeManager(), oldType,
							builder.genericType("_cl_mem"),	cl_mems[var]->getType()));
					if(const RefTypePtr& rt = dynamic_pointer_cast<const RefType>(cl_mems[var]->getType()))
						newType = rt->getElementType();
					else
						newType = cl_mems[var]->getType();

					NodePtr ret = builder.declarationStmt(cl_mems[var], builder.refNew(builder.callExpr(BASIC.getUndefined(), BASIC.getTypeLiteral(newType))));
					copyAnnotations(decl, ret);
					return ret;
			//	}
			}
		} else {
// remove delarations of opencl type variables. Should not be used any more
			if(var->getType()->toString().find("_cl_") != string::npos /*&& var->getNodeCategory() != NT_StructType*/)
				return BASIC.getNoOp();
		}

	}

	if(const CallExprPtr& callExpr = dynamic_pointer_cast<const CallExpr>(element)) {
		const ExpressionPtr& fun = callExpr->getFunctionExpr();
		if(fun == BASIC.getRefAssign()) {
			// get rid of some not needed functions
			if(CallExprPtr rhs = dynamic_pointer_cast<const CallExpr>(callExpr->getArgument(1))) {
				// check if it is embedded in a ref.deref
				if(const CallExprPtr& deref = dynamic_pointer_cast<const CallExpr>(rhs->getArgument(0)))
					if(rhs->getFunctionExpr() == BASIC.getRefDeref())
						rhs = deref;
				if(rhs->getFunctionExpr()->toString() == "clCreateContext")
					return BASIC.getNoOp();
				if(rhs->getFunctionExpr()->toString() == "clCreateCommandQueue")
					return BASIC.getNoOp();

			}
		}

		if(const CallExprPtr& newCall = dynamic_pointer_cast<const CallExpr>(callExpr->substitute(builder.getNodeManager(), *this))) {
			if(const LiteralPtr& fun = dynamic_pointer_cast<const Literal>(newCall->getFunctionExpr())) {
				if(fun->getValue() == "clEnqueueNDRangeKernel" ) {
					// get kernel function
					ExpressionPtr k = callExpr->getArgument(1);

					// check if argument is a call to composite.ref.elem
//					if(const CallExprPtr cre = dynamic_pointer_cast<const CallExpr>(k))
	//					k = cre->getArgument(0);
std::cout <<  kernelLambdas << std::endl << k << std::endl;
					// get corresponding lambda expression
					LambdaExprPtr lambda = kernelLambdas[k];
					assert(lambda && "No lambda expression for kernel call found");

std::cout << "Devcon 4\n";
					assert(kernelArgs.find(k) != kernelArgs.end() && "No arguments for call to kernel function found");
std::cout << "Devcon 5\n";
					vector<ExpressionPtr>& args = kernelArgs[k];

					// make a three element vector out of the global and local size
					const ExpressionPtr global = anythingToVec3(newCall->getArgument(2), newCall->getArgument(4));
					const ExpressionPtr local = anythingToVec3(newCall->getArgument(2), newCall->getArgument(5));

					vector<ExpressionPtr> newArgs;
					// construct call to kernel function
					if(localMemDecls.find(k) == localMemDecls.end() || localMemDecls[k].size() == 0) {
						// if there is no local memory in argument, the arguments can simply be copyied
						for_each(args, [&](ExpressionPtr& arg) {
								assert(!!arg && "Kernel has illegal global memory argument");
								//global and private memory arguments must be variables
								getVarOutOfCrazyInspireConstruct(arg, builder);

								newArgs.push_back(dynamic_pointer_cast<const Expression>(this->resolveElement(arg)));
							});

						// add global and local size to arguments
						newArgs.push_back(global);
						newArgs.push_back(local);

						NodePtr kernelCall = builder.callExpr(BASIC.getInt4(), lambda, newArgs);
						copyAnnotations(callExpr, kernelCall);
						return kernelCall;
					}
					// add declarations for argument local variables if any, warping a function around it

					vector<StatementPtr> declsAndKernelCall;
					for_each(localMemDecls[k], [&](DeclarationStmtPtr decl) {
							assert(!!decl && "Kernel has illegal local memory argument");
							declsAndKernelCall.push_back(decl);
						});

					vector<VariablePtr> params;
					vector<ExpressionPtr> innerArgs;
					vector<TypePtr> wrapperInterface;

					for_each(args, [&](ExpressionPtr& arg) {
							assert(!!arg && "Kernel has illegal global memory argument");
							bool local = false;
							//global and private memory arguments must be variables
							getVarOutOfCrazyInspireConstruct(arg, builder);
							// local args are declared in localMemDecls
							for_each(localMemDecls[k], [&](DeclarationStmtPtr decl) {
									assert(!!decl && "Kernel has illegal local memory argument");
									if(arg == decl->getVariable()) {
//                                params.push_back(decl->getVariable());
										// will be declared inside wrapper function
										local = true;
									}
								});
							if(!local) {
								// global and private memory arguments will be passed to the wrapper function as agrument
								ExpressionPtr newArg = dynamic_pointer_cast<const Expression>(this->resolveElement(arg));
								assert(!!newArg && "Argument of kernel function must be an Expression");
								newArgs.push_back(newArg);
								wrapperInterface.push_back(newArg->getType());

								// kernel funtion will take a new variable as argument
								params.push_back(builder.variable(newArg->getType()));
							}
							// the kernel call will use the params of the outer call as arguments, they must be an expression
							innerArgs.push_back(params.back());
						});

					// add global and local size to arguments
					TypePtr vec3type = builder.vectorType(BASIC.getUInt4(), builder.concreteIntTypeParam(static_cast<size_t>(3)));
					newArgs.push_back(global);
					VariablePtr globalVar = builder.variable(vec3type);
					params.push_back(globalVar);
					innerArgs.push_back(globalVar);
					wrapperInterface.push_back(vec3type);

					newArgs.push_back(local);
					VariablePtr localVar = builder.variable(vec3type);
					params.push_back(localVar);
					innerArgs.push_back(localVar);
					wrapperInterface.push_back(vec3type);

					NodePtr kernelCall = builder.callExpr(BASIC.getInt4(), lambda, innerArgs);
					copyAnnotations(callExpr, kernelCall);

					declsAndKernelCall.push_back(dynamic_pointer_cast<const Statement>(kernelCall));
					const FunctionTypePtr& wrapperType = builder.functionType(wrapperInterface, BASIC.getInt4());
					return builder.callExpr(builder.lambdaExpr(builder.lambda(wrapperType, params, builder.compoundStmt(declsAndKernelCall))), newArgs);
				}
			}
			if(newCall->getFunctionExpr() == BASIC.getCompositeRefElem()) {
				// replace variable with new version if necessary
				const VariablePtr& newStruct = dynamic_pointer_cast<const Variable>(newCall->getArgument(0));
				const VariablePtr& oldStruct = dynamic_pointer_cast<const Variable>(callExpr->getArgument(0));
				assert(oldStruct && newStruct && "First argument of composite.ref.elem must be a struct variable");
				if(newStruct != oldStruct) { // struct variable has been replaced, may need to update type of composite.ref.elem
					const TypePtr& newType = dynamic_pointer_cast<const StructType>(getNonRefType(newStruct->getType()));
					assert(newType && "First argument of composite.ref.elem must be a struct variable");

					const LiteralPtr& oldIdLit = dynamic_pointer_cast<const Literal>(callExpr->getArgument(1));
					const LiteralPtr& oldTypeLit = dynamic_pointer_cast<const Literal>(callExpr->getArgument(2));
					assert(oldIdLit && oldTypeLit && "Second and third argument of composite.ref.elem must be a literals");
					// check if the field has been replaced with another field of the same struct with a different identifier
					const LiteralPtr& newIdLit = replacements.find(oldIdLit) != replacements.end() ?
							static_pointer_cast<const Literal>(replacements[oldIdLit]) : oldIdLit;
					const IdentifierPtr& id = builder.identifier(newIdLit->getValue());
					const TypePtr& oldType = dynamic_pointer_cast<const GenericType>(oldTypeLit->getType())->getTypeParameter().at(0);
					for_each(replacements, [&](std::pair<const NodePtr, NodePtr> n) {
					});

					const core::TypePtr& memberTy = static_pointer_cast<const NamedCompositeType>(newType)->getTypeOfMember(id);
					if(!memberTy) { // someone requested a field which has been removed from the struct -> will be deleted anyways
						return newCall;
					}

					if(memberTy != oldType) { // need to update the type argument of the call
						NodePtr retExpr = builder.callExpr( builder.refType(memberTy), BASIC.getCompositeRefElem(),
								toVector<ExpressionPtr>(newCall->getArgument(0), newIdLit, BASIC.getTypeLiteral(memberTy) ));

						copyAnnotations(newCall, retExpr);
						return retExpr;

					}

				}

			}
			// check if return type of array/vector subscript calls are still valid
			if(BASIC.isSubscriptOperator(newCall->getFunctionExpr()))
				if(newCall->getType()->toString().find("array<_cl_mem,1>") != string::npos) {
					if(const SingleElementTypePtr& seType = dynamic_pointer_cast<const SingleElementType>(getNonRefType(newCall->getArgument(0)))) {
/*						if(BASIC.isArrayOp(newCall->getFunctionExpr()))
							std::cout << "ARRRARY" << std::endl;
						else if(BASIC.isVectorOp(newCall->getFunctionExpr()))
							std::cout << "VECTOR" << std::endl;
						else
							std::cout << "NOTHING" << newCall->getFunctionExpr() << std::endl;*/
						const TypePtr& oldType = getNonRefType(newCall);
						if(seType->getElementType() != oldType) {
							TypePtr newRetType = dynamic_pointer_cast<const Type>(core::transform::replaceAll(builder.getNodeManager(), newCall->getType(),
									oldType, seType->getElementType()));
							return builder.callExpr(newRetType, newCall->getFunctionExpr(), newCall->getArguments());
						}
					}
				}
		}

	}

	NodePtr ret = element->substitute(builder.getNodeManager(), *this);
	copyAnnotations(element, ret);
	return ret;
}

} //namespace ocl
} //namespace frontend
} //namespace insieme