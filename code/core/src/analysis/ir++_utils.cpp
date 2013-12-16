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

#include "insieme/core/analysis/ir++_utils.h"

#include "insieme/core/ir.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_class_info.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/lang/ir++_extension.h"

#include "insieme/core/datapath/datapath.h"

#include "insieme/utils/assert.h"

namespace insieme {
namespace core {
namespace analysis {

	bool isIRpp(const NodePtr& node) {
		return visitDepthFirstOnceInterruptible(node, [](const NodePtr& cur)->bool {

			// check whether there are structs using inheritance
			if (StructTypePtr structType = cur.isa<StructTypePtr>()) {

				// if not empty => it is a IR++ code
				if (!structType->getParents().empty()) {
					return true;
				}

				// if there is meta-info => it is a IR++ code
				if (hasMetaInfo(structType)) {
					return true;
				}
			}

			// check function types
			if (FunctionTypePtr funType = cur.isa<FunctionTypePtr>()) {
				return funType->isConstructor() || funType->isDestructor() || funType->isMemberFunction();
			}

			// if there are exceptions, it is IR++
			if (cur->getNodeType() == NT_ThrowStmt || cur->getNodeType() == NT_TryCatchStmt) {
				return true;
			}

			// no IR++ content found
			return false;
		}, true);
	}

	bool isObjectType(const TypePtr& type) {


		// decide whether something is an object type based on the node type
		switch(type->getNodeType()) {
		case NT_GenericType:
			// no built-in type is an object type
			return !type->getNodeManager().getLangBasic().isPrimitive(type);
		case NT_StructType:
		case NT_TypeVariable:
		case NT_UnionType:
			return true;			// all this types are always object types
		case NT_RecType:
			return isObjectType(type.as<RecTypePtr>()->getTypeDefinition());
		default: break;
		}

		// everything else is not an object type
		return false;
	}

	bool isObjectReferenceType(const TypePtr& type) {
		return type->getNodeType() == NT_RefType && isObjectReferenceType(type.as<RefTypePtr>());
	}

	bool isObjectReferenceType(const RefTypePtr& type) {
		return isObjectType(type->getElementType());
	}

	bool isPureVirtual(const CallExprPtr& call) {
		// check for null
		if (!call) return false;

		// check whether it is a call to the pure-virtual literal
		const auto& ext = call->getNodeManager().getLangExtension<lang::IRppExtensions>();
		return isCallOf(call, ext.getPureVirtual());
	}

	bool isPureVirtual(const NodePtr& node) {
		return node && isPureVirtual(node.isa<CallExprPtr>());
	}


	// ---------------------------- References --------------------------------------

	namespace {

		bool isRef(const TypePtr& type, const string& memberName) {

			// filter out null-pointer
			if (!type) return false;

			// must be a struct type
			StructTypePtr structType = type.isa<StructTypePtr>();
			if (!structType) return false;

			// only one member
			if (structType.size() != 1u) return false;

			// check the one member element
			NamedTypePtr element = structType[0];
			return element->getType()->getNodeType() == NT_RefType
					&& !isCppRef(element->getType()) && !isConstCppRef(element->getType())
					&& element->getName().getValue() == memberName;
		}

		TypePtr getRef(const TypePtr& elementType, const string& memberName) {
			IRBuilder builder(elementType->getNodeManager());
			return builder.structType(toVector(
					builder.namedType(memberName, builder.refType(elementType))
			));
		}

		const string CppRefStringMember = "_cpp_ref";
		const string CppConstRefStringMember = "_const_cpp_ref";
	}

	bool isCppRef(const TypePtr& type) {
		return isRef(type, CppRefStringMember);
	}

	TypePtr getCppRef(const TypePtr& elementType) {
		return getRef(elementType, CppRefStringMember);
	}

	bool isConstCppRef(const TypePtr& type) {
		return isRef(type, CppConstRefStringMember);
	}

	TypePtr getConstCppRef(const TypePtr& elementType) {
		return getRef(elementType, CppConstRefStringMember);
	}

	TypePtr getCppRefElementType(const TypePtr& cppRefType) {
		assert(isCppRef(cppRefType) || isConstCppRef(cppRefType));
		return cppRefType.as<StructTypePtr>()[0]->getType().as<RefTypePtr>()->getElementType();
	}

	bool isAnyCppRef(const TypePtr& type){
		return isCppRef(type) || isConstCppRef(type);
	}

	ExpressionPtr unwrapCppRef (const ExpressionPtr& originalExpr){
		ExpressionPtr expr = originalExpr;
		NodeManager& manager = expr.getNodeManager();
		IRBuilder builder(manager);

		if (expr->getType().isa<core::RefTypePtr>()){
			expr = builder.deref(expr);
		}

		if (isCppRef(expr->getType())){
			return builder.callExpr(builder.refType(getCppRefElementType(expr->getType())), manager.getLangExtension<lang::IRppExtensions>().getRefCppToIR(), expr);
		}
		else if (isConstCppRef(expr->getType())){
			return builder.callExpr(builder.refType(getCppRefElementType(expr->getType())), manager.getLangExtension<lang::IRppExtensions>().getRefConstCppToIR(), expr);
		}

		// error fallthrow
		assert(false && "could not unwrapp Cpp ref, is it a cpp ref?");
		return ExpressionPtr();
	}

	// --------------------------- data member pointer -----------------------------------
	
	bool isMemberPointer (const TypePtr& type){

		// filter out null-pointer
		if (!type) return false;

		// must be a struct type
		StructTypePtr structType = type.isa<StructTypePtr>();
		if (!structType) return false;

		// has 3 elements
		if (structType.size() != 3u) return false;

		NamedTypePtr element = structType[0];
		if ( !isTypeLiteralType(element->getType())) 	return false;
		if (element->getName().getValue() != "objType") return false;

		element = structType[1];
		//if ( !isId(element->getType()))	return false; // TODO: ask if is an identifier
		if (element->getName().getValue() != "id") return false;

		element = structType[2];
		if ( !isTypeLiteralType(element->getType())) 	 return false;
		if (element->getName().getValue() != "membType") return false;

		return true;
	}

	TypePtr getMemberPointer (const TypePtr& classType, const TypePtr& membTy){
		NodeManager& manager = classType.getNodeManager();
		IRBuilder builder(manager);
		return builder.structType(toVector(
				builder.namedType(builder.stringValue("objType"), builder.getTypeLiteralType(classType)),
				builder.namedType(builder.stringValue("id"), builder.getLangBasic().getIdentifier()),
				builder.namedType(builder.stringValue("membType"), builder.getTypeLiteralType(membTy))
		));
	}

	ExpressionPtr getMemberPointerValue (const TypePtr& classType, const std::string& fieldName, const TypePtr& membType){
		NodeManager& manager = classType.getNodeManager();
		IRBuilder builder(manager);

		// retrieve the name and the field type to build the desired member pointer struct
		core::ExpressionPtr access = manager.getLangExtension<lang::IRppExtensions>().getMemberPointerCtor();
		return builder.callExpr(access, toVector<core::ExpressionPtr>(builder.getTypeLiteral(classType), builder.getIdentifierLiteral(fieldName), builder.getTypeLiteral(membType)));
	}

	ExpressionPtr getMemberPointerAccess (const ExpressionPtr& base, const ExpressionPtr& expr){
		NodeManager& manager = base.getNodeManager();
		IRBuilder builder(manager);
		
		// retrieve the name and the field type to build the desired access
		core::ExpressionPtr access = manager.getLangExtension<lang::IRppExtensions>().getMemberPointerAccess();
		return builder.callExpr(access,  toVector(base, expr));
	}

	// --------------------------- C++ calls ---------------------------------------------

	bool isConstructorCall(const core::ExpressionPtr& expr){
		core::CallExprPtr call = expr.isa<core::CallExprPtr>();
		return call && call->getFunctionExpr()->getType().as<FunctionTypePtr>()->isConstructor();
	}


	LambdaExprPtr createDefaultConstructor(const StructTypePtr& type) {
		NodeManager& manager = type.getNodeManager();
		IRBuilder builder(manager);

		// the type of the reference to be initialized by constructor (this type)
		TypePtr refType = builder.refType(type);

		// create the type of the resulting function
		FunctionTypePtr ctorType = builder.functionType(
				toVector(refType), refType, FK_CONSTRUCTOR
		);

		// build the constructor
		VariablePtr thisVar = builder.variable(refType, 1);
		return builder.lambdaExpr(ctorType, toVector(thisVar), builder.compoundStmt());
	}


	bool isDefaultConstructor(const LambdaExprPtr& lambda) {

		// it has to be a lambda
		if (!lambda) return false;

		// it has to be a constructor
		if (lambda->getFunctionType()->getKind() != FK_CONSTRUCTOR) return false;

		// it has to have a single parameter
		if (lambda->getParameterList()->size() != 1u) return false;

		// TODO: check the actual initialization of the members and parent types
		// if the body is empty, be fine
		return lambda->getBody() == IRBuilder(lambda->getNodeManager()).compoundStmt();
	}

	// ------------------------------- Long Long ---------------------------------------
	//
	bool isLongLong(const TypePtr& type){

		// filter out null-pointer
		if (!type) return false;

		// must be a struct type
		StructTypePtr structType = type.isa<StructTypePtr>();
		if (!structType) return false;

		// only one member
		if (structType.size() != 1u) return false;

		// check the one member element
		IRBuilder builder(type->getNodeManager());
		NamedTypePtr element = structType[0];

		if ((element->getName().getValue() != "longlong_val") &&
			!(element->getType() == builder.getLangBasic().getInt8() || element->getType() == builder.getLangBasic().getUInt8()))
			return false;

		return true;
	}
	
	bool isSignedLongLong(const TypePtr& type){
		assert(isLongLong(type));
		IRBuilder builder(type->getNodeManager());
		NamedTypePtr element = type.as<StructTypePtr>()[0];
		return element->getType() == builder.getLangBasic().getInt8();
	}

	ExpressionPtr castToLongLong( const ExpressionPtr& expr, bool _signed){
		NodeManager& manager = expr.getNodeManager();
		IRBuilder builder(manager);

		core::ExpressionPtr cast;
		if (_signed)
			cast = manager.getLangExtension<lang::IRppExtensions>().getLongToLongLong();
		else
			cast = manager.getLangExtension<lang::IRppExtensions>().getULongToULongLong();

		return builder.callExpr(cast, expr);
	}

	ExpressionPtr castFromLongLong( const ExpressionPtr& expr){
		assert(isLongLong(expr->getType()));
		NodeManager& manager = expr.getNodeManager();
		IRBuilder builder(manager);
		
		core::ExpressionPtr cast;
		NamedTypePtr element = expr->getType().as<core::StructTypePtr>()[0];
		if (element->getType() == builder.getLangBasic().getInt8())
			cast = manager.getLangExtension<lang::IRppExtensions>().getLongLongToLong();
		else if (element->getType() == builder.getLangBasic().getUInt8())
			cast = manager.getLangExtension<lang::IRppExtensions>().getULongLongToULong();
		else
			assert(false && "not a long long");

		return builder.callExpr(cast, expr);
	}
	ExpressionPtr castBetweenLongLong( const ExpressionPtr& expr){
		assert(isLongLong(expr->getType()));
		NodeManager& manager = expr.getNodeManager();
		IRBuilder builder(manager);
		
		core::ExpressionPtr cast;
		NamedTypePtr element = expr->getType().as<core::StructTypePtr>()[0];
		if (element->getType() == builder.getLangBasic().getInt8()){
			cast = manager.getLangExtension<lang::IRppExtensions>().getLongLongToULongLong();
		}else if (element->getType() == builder.getLangBasic().getUInt8()){
			cast = manager.getLangExtension<lang::IRppExtensions>().getULongLongToLongLong();
		}else{
			assert(false && "not a long long");
		}

		return builder.callExpr(cast, expr);
	}

} // end namespace analysis
} // end namespace core
} // end namespace insieme
