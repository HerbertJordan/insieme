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
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "insieme/core/ir_types.h"

#include "ir_node_test.inc"

using std::vector;

namespace insieme {
namespace core {

template<typename PT>
void basicTypeTests(PT type, bool concrete, const NodeList& children = NodeList());

TEST(TypeTest, NodeManager ) {

	// create type manager
	NodeManager manager;

	// get a type
	GenericTypePtr typeA1 = GenericType::get(manager, "A");
	GenericTypePtr typeA2 = GenericType::get(manager, "A");
	GenericTypePtr typeB = GenericType::get(manager, "B");

	EXPECT_TRUE (!!typeA1);
	EXPECT_TRUE (!!typeA2);
	EXPECT_TRUE (!!typeB);

	EXPECT_TRUE ( typeA1 == typeA2 );
	EXPECT_FALSE ( typeA1 == typeB );

}

TEST(TypeTest, NodeManagerGetAllBug ) {

	NodeManager manager;

	TypePtr typeA = GenericType::get(manager, "A");
	TypePtr typeB = GenericType::get(manager, "B");
	TypePtr typeR = GenericType::get(manager, "R");

	TypeList list;
	list.push_back(typeA);
	list.push_back(typeB);

	FunctionTypePtr funType = FunctionType::get(manager, list, typeR);
}

TEST(TypeTest, MultipleNodeManager ) {

	// create type manager
	NodeManager managerA;
	NodeManager managerB;

	// create a complex type in manager A
	GenericTypePtr baseA = GenericType::get(managerA, "A");
	GenericTypePtr baseB = GenericType::get(managerA, "B");

	vector<TypePtr> typesA;
	typesA.push_back(baseA);
	typesA.push_back(baseB);
	GenericTypePtr complexA = GenericType::get(managerA, "C", typesA);

	vector<TypePtr> typesB;
	typesB.push_back(complexA);
	typesB.push_back(baseB);
	GenericTypePtr rootA = GenericType::get(managerA, "R", typesB);

	EXPECT_EQ ( 12, managerA.size() );
	EXPECT_EQ ( 0, managerB.size() );

	// try to obtain the same type from the other manager
	GenericTypePtr rootB = GenericType::get(managerB, "R", typesB);
	EXPECT_EQ ( 12, managerA.size() );
	EXPECT_EQ ( 12, managerB.size() );

	EXPECT_NE ( rootA, rootB );
	EXPECT_EQ ( *rootA, *rootB );
}


TEST(TypeTest, GenericType) {

	// create a type manager for this test
	NodeManager manager;

	// some empty lists (required as arguments)
	vector<TypePtr> emptyPtr;
	vector<IntTypeParamPtr> emptyPar;

	// create some variable types
	TypeVariablePtr varA = TypeVariable::get(manager, "alpha");
	TypeVariablePtr varB = TypeVariable::get(manager, "beta");

	// create some int type parameter
	IntTypeParamPtr paramA = VariableIntTypeParam::get(manager, 'a');
	IntTypeParamPtr paramB = ConcreteIntTypeParam::get(manager, 12);

	// create some concrete types
	GenericTypePtr typeA = GenericType::get(manager, "A");
	EXPECT_EQ ( "A" , toString(*typeA) );
	GenericTypePtr typeB = GenericType::get(manager, "B");
	EXPECT_EQ ( "B" , toString(*typeB) );

	// create complex types
	GenericTypePtr typeC = GenericType::get(manager, "C", toVector<TypePtr>(varA));
	EXPECT_EQ ( "C<'alpha>" , toString(*typeC) );
	GenericTypePtr typeD = GenericType::get(manager, "D", emptyPtr, toVector(paramA));
	EXPECT_EQ ( "D<#a>" , toString(*typeD) );

	// create complex type with multiple parameter
	vector<TypePtr> typeListA;
	typeListA.push_back(typeA);
	typeListA.push_back(typeB);
	GenericTypePtr typeE = GenericType::get(manager, "E", typeListA);
	EXPECT_EQ ( "E<A,B>" , toString(*typeE) );

	// create type with base type
	// TODO: in case base type is re-enabled, do so for this test
//	GenericTypePtr typeF = GenericType::get(manager, "F", emptyPtr, emptyPar, typeA );
//	EXPECT_EQ ( "F" , toString(*typeF) );
//
//	GenericTypePtr typeG = GenericType::get(manager, "G", typeListA, toVector(paramB), typeA );
//	EXPECT_EQ ( "G<A,B,12>" , toString(*typeG) );

	// perform general test cases
	{
		SCOPED_TRACE ( "typeA" );
		basicTypeTests(typeA, true, toVector<NodePtr>(typeA->getName(), typeA->getTypeParameter(), typeA->getIntTypeParameter()));
	}{
		SCOPED_TRACE ( "typeB" );
		basicTypeTests(typeB, true, toVector<NodePtr>(typeB->getName(), typeB->getTypeParameter(), typeB->getIntTypeParameter()));
	}{
		SCOPED_TRACE ( "typeC" );
		basicTypeTests(typeC, false, toVector<NodePtr>(typeC->getName(), typeC->getTypeParameter(), typeC->getIntTypeParameter()));
	}{
		SCOPED_TRACE ( "typeD" );
		basicTypeTests(typeD, false, toVector<NodePtr>(typeD->getName(), typeD->getTypeParameter(), typeD->getIntTypeParameter()));
	}{
		SCOPED_TRACE ( "typeE" );
		basicTypeTests(typeE, true, toVector<NodePtr>(typeE->getName(), typeE->getTypeParameter(), typeE->getIntTypeParameter()));
//	}{
//		SCOPED_TRACE ( "typeF" );
//		basicTypeTests(typeF, true, toVector<NodePtr>(typeA));
//	}{
//		SCOPED_TRACE ( "typeG" );
//		NodeList list = toList(typeListA);
//		list.push_back(paramB);
//		list.push_back(typeA);
//		basicTypeTests(typeG, true, list);
	}

	// selected equality checks (not after copy)
	EXPECT_FALSE ( varA == varB );
	EXPECT_FALSE ( typeA == typeB );
	EXPECT_FALSE ( varA == typeB );
	EXPECT_TRUE ( varA == varA );
	EXPECT_TRUE ( typeA == typeA );

	// create list of all types
	vector<TypePtr> types;
	types.push_back(typeA);
	types.push_back(typeB);
	types.push_back(typeC);
	types.push_back(typeD);
	types.push_back(typeE);
//	types.push_back(typeF);

	// check whether equality is working properly
	for (unsigned i=0; i<types.size(); i++) {
		for (unsigned j=0; j<types.size(); j++) {
			EXPECT_EQ ( i == j , types[i] == types[j]);
		}
	}
}

TEST(TypeTest, TypeVariable) {

	NodeManager manager;
	TypeVariablePtr varTypeA = TypeVariable::get(manager, "alpha");
	TypeVariablePtr varTypeB = TypeVariable::get(manager, "beta");

	// check name
	EXPECT_EQ ( "'alpha", toString(*varTypeA) );
	EXPECT_EQ ( "alpha", varTypeA->getVarName()->getValue() );

	EXPECT_EQ ( "'beta", toString(*varTypeB) );
	EXPECT_EQ ( "beta", varTypeB->getVarName()->getValue() );

	// perform basic type tests
	basicTypeTests(varTypeA, false, toList(varTypeA->getVarName()));
}

TEST(TypeTest, TupleType) {

	NodeManager manager;

	vector<TypePtr> subTypesA;
	vector<TypePtr> subTypesB;
	subTypesB.push_back(GenericType::get(manager, "dummy1"));
	subTypesB.push_back(GenericType::get(manager, "dummy2"));

	TupleTypePtr typeA = TupleType::get(manager, subTypesA);
	TupleTypePtr typeB = TupleType::get(manager, subTypesB);


	// perform basic type tests
	basicTypeTests(typeA, true, toList(typeA->getElementTypes()));
	basicTypeTests(typeB, true, toList(typeB->getElementTypes()));

}

TEST(TypeTest, FunctionType) {

	NodeManager manager;

	TypePtr dummyA = GenericType::get(manager, "dummyA");
	TypePtr dummyB = GenericType::get(manager, "dummyB");
	TypePtr alpha = TypeVariable::get(manager, "alpha");

	TypePtr resultA = GenericType::get(manager, "returnA");
	TypePtr resultB = GenericType::get(manager, "returnB");

	FunctionTypePtr funTypeA = FunctionType::get(manager, toVector(dummyA), resultA);
	FunctionTypePtr funTypeB = FunctionType::get(manager, toVector(alpha), resultB);
	FunctionTypePtr funTypeC = FunctionType::get(manager, toVector(alpha, dummyA), resultB);
	FunctionTypePtr funTypeD = FunctionType::get(manager, toVector(dummyA), resultA, false);

	EXPECT_EQ ( "((dummyA)->returnA)" , toString(*funTypeA) );
	EXPECT_EQ ( "(('alpha)->returnB)" , toString(*funTypeB) );
	EXPECT_EQ ( "(('alpha,dummyA)->returnB)" , toString(*funTypeC) );
	EXPECT_EQ ( "((dummyA)=>returnA)" , toString(*funTypeD) );

	EXPECT_NE ( funTypeA, funTypeD );

	vector<NodePtr> subNodesA;
	subNodesA.push_back(Types::get(manager, toVector(dummyA)));
	subNodesA.push_back(resultA);
	subNodesA.push_back(BoolValue::get(manager, true));

	vector<NodePtr> subNodesB;
	subNodesB.push_back(Types::get(manager, toVector(alpha)));
	subNodesB.push_back(resultB);
	subNodesB.push_back(BoolValue::get(manager, true));

	vector<NodePtr> subNodesC;
	subNodesC.push_back(Types::get(manager, toVector(alpha, dummyA)));
	subNodesC.push_back(resultB);
	subNodesC.push_back(BoolValue::get(manager, true));

	vector<NodePtr> subNodesD;
	subNodesD.push_back(Types::get(manager, toVector(dummyA)));
	subNodesD.push_back(resultA);
	subNodesD.push_back(BoolValue::get(manager, false));

	// perform basic type tests
	basicTypeTests(funTypeA, true, toList(subNodesA));
	basicTypeTests(funTypeB, true, toList(subNodesB));
	basicTypeTests(funTypeC, true, toList(subNodesC));
	basicTypeTests(funTypeD, true, toList(subNodesD));
}

TEST(TypeTest, RecType) {

	// create a manager for this test
	NodeManager manager;


	// TODO: test whether order of definitions is important ... (it should not)

	// create a simple recursive type uX.X (no actual type)
	TypeVariablePtr varX = TypeVariable::get(manager, "X");

	// create definition
	RecTypeBindingPtr binding = RecTypeBinding::get(manager, varX, varX);
	RecTypeDefinitionPtr definition = RecTypeDefinition::get(manager, toVector(binding));
	EXPECT_EQ ( "{'X='X}", toString(*definition) );


	RecTypePtr type = RecType::get(manager, varX, definition);
	EXPECT_EQ ( "rec 'X.{'X='X}", toString(*type) );
	basicTypeTests(type, true, toList(varX, definition));


	// create mutually recursive type
	TypeVariablePtr varY = TypeVariable::get(manager, "Y");

	definition = RecTypeDefinition::get(manager, toVector(
			RecTypeBinding::get(manager, varX, varY),
			RecTypeBinding::get(manager, varY, varX)
	));
	EXPECT_TRUE ( toString(*definition)=="{'Y='X,'X='Y}" || toString(*definition)=="{'X='Y,'Y='X}" );

	RecTypePtr typeX = RecType::get(manager, varX, definition);
	RecTypePtr typeY = RecType::get(manager, varY, definition);

	EXPECT_NE ( typeX, typeY );
	EXPECT_NE ( typeX, type );

	EXPECT_TRUE ( toString(*typeX)=="rec 'X.{'Y='X,'X='Y}" || toString(*typeX)=="rec 'X.{'X='Y,'Y='X}" );

	basicTypeTests(typeX, true, toList(toVector<NodePtr>(varX, definition)));
	basicTypeTests(typeY, true, toList(toVector<NodePtr>(varY, definition)));
}


TEST(TypeTest, StructType) {

	NodeManager manager;

	StringValuePtr identA = StringValue::get(manager, "a");
	StringValuePtr identB = StringValue::get(manager, "b");

	vector<NamedTypePtr> entriesA;
	entriesA.push_back(NamedType::get(manager, identA, GenericType::get(manager, "A")));
	entriesA.push_back(NamedType::get(manager, identB, GenericType::get(manager, "B")));

	StructTypePtr structA = StructType::get(manager, entriesA);
	EXPECT_EQ ( "struct<a:A,b:B>", toString(*structA) );

	vector<NamedTypePtr> entriesB;
	StructTypePtr structB = StructType::get(manager, entriesB);
	EXPECT_EQ ( "struct<>", toString(*structB) );

	vector<NamedTypePtr> entriesC;
	entriesC.push_back(NamedType::get(manager, identA, TypeVariable::get(manager,"alpha")));
	entriesC.push_back(NamedType::get(manager, identB, GenericType::get(manager, "B")));
	StructTypePtr structC = StructType::get(manager, entriesC);
	EXPECT_EQ ( "struct<a:'alpha,b:B>", toString(*structC) );

	// test for elements with same name
	vector<NamedTypePtr> entriesD;
	entriesD.push_back(NamedType::get(manager, identA, GenericType::get(manager,"A")));
	entriesD.push_back(NamedType::get(manager, identB, GenericType::get(manager,"A")));
	entriesD.push_back(NamedType::get(manager, identA, GenericType::get(manager,"A")));
	EXPECT_THROW ( StructType::get(manager, entriesD), std::invalid_argument );

	// .. same type should not be a problem
	entriesD.pop_back();
	EXPECT_NO_THROW ( StructType::get(manager, entriesD) );

	// perform basic type tests
	basicTypeTests(structA, true, convertList(entriesA));
	basicTypeTests(structB, true, convertList(entriesB));
	basicTypeTests(structC, false, convertList(entriesC));
}

TEST(TypeTest, RecStructType) {
	// create a manager for this test
	NodeManager manager;

	// TODO: test whether order of definitions is important ... (it should not)

	StringValuePtr identA = StringValue::get(manager, "a");
	StringValuePtr identB = StringValue::get(manager, "b");

	// create a simple recursive type uX.X (no actual type)
	TypeVariablePtr varX = TypeVariable::get(manager, "X");

	vector<NamedTypePtr> entriesA;
	entriesA.push_back(NamedType::get(manager, identA, GenericType::get(manager, "A")));
	entriesA.push_back(NamedType::get(manager, identB, RefType::get(manager, varX)));

	StructTypePtr structA = StructType::get(manager, entriesA);

	// create definition
	vector<RecTypeBindingPtr> definitions;
	definitions.push_back(RecTypeBinding::get(manager, varX, structA));
	RecTypeDefinitionPtr definition = RecTypeDefinition::get(manager, definitions);
	EXPECT_EQ ( "{'X=struct<a:A,b:ref<'X>>}", toString(*definition) );

	RecTypePtr type = RecType::get(manager, varX, definition);
	EXPECT_EQ ( "rec 'X.{'X=struct<a:A,b:ref<'X>>}", toString(*type) );
	basicTypeTests(type, true, toList(toVector<NodePtr>(varX, definition)));

	EXPECT_EQ("struct<a:A,b:ref<'X>>", toString(*type->getDefinition()->getDefinitionOf(varX)));
}


TEST(TypeTest, UnionType) {

	NodeManager manager;

	StringValuePtr identA = StringValue::get(manager, "a");
	StringValuePtr identB = StringValue::get(manager, "b");

	UnionType::Entries entriesA;
	entriesA.push_back(NamedType::get(manager, identA, GenericType::get(manager, "A")));
	entriesA.push_back(NamedType::get(manager, identB, GenericType::get(manager, "B")));

	UnionTypePtr unionA = UnionType::get(manager, entriesA);
	EXPECT_EQ ( "union<a:A,b:B>", toString(*unionA) );

	UnionType::Entries entriesB;
	UnionTypePtr unionB = UnionType::get(manager, entriesB);
	EXPECT_EQ ( "union<>", toString(*unionB) );

	UnionType::Entries entriesC;
	entriesC.push_back(NamedType::get(manager, identA, TypeVariable::get(manager,"alpha")));
	entriesC.push_back(NamedType::get(manager, identB, GenericType::get(manager, "B")));
	UnionTypePtr unionC = UnionType::get(manager, entriesC);
	EXPECT_EQ ( "union<a:'alpha,b:B>", toString(*unionC) );


	// perform basic type tests
	basicTypeTests(unionA, true, convertList(entriesA));
	basicTypeTests(unionB, true, convertList(entriesB));
	basicTypeTests(unionC, false, convertList(entriesC));
}

TEST(TypeTest, ArrayType) {

	// create type manager and element types
	NodeManager manager;
	TypePtr elementTypeA = GenericType::get(manager,"A");
	TypePtr elementTypeB = TypeVariable::get(manager,"a");

	// obtain array types
	IntTypeParamPtr param1 = ConcreteIntTypeParam::get(manager, 1);
	IntTypeParamPtr param3 = ConcreteIntTypeParam::get(manager, 3);
	ArrayTypePtr arrayTypeA = ArrayType::get(manager, elementTypeA);
	ArrayTypePtr arrayTypeB = ArrayType::get(manager, elementTypeB, param3);

	// check names
	EXPECT_EQ ( "array<A,1>", toString(*arrayTypeA) );
	EXPECT_EQ ( "array<'a,3>", toString(*arrayTypeB) );

	// check element types
	EXPECT_EQ ( elementTypeA, arrayTypeA->getElementType() );
	EXPECT_EQ ( elementTypeB, arrayTypeB->getElementType() );

	// check dimensions
	ASSERT_TRUE ( arrayTypeA->getDimension()->getNodeType()==NT_ConcreteIntTypeParam);
	ASSERT_TRUE ( arrayTypeB->getDimension()->getNodeType()==NT_ConcreteIntTypeParam);
	EXPECT_EQ ( static_cast<unsigned>(1), static_pointer_cast<ConcreteIntTypeParamPtr>(arrayTypeA->getDimension())->getParam()->getValue());
	EXPECT_EQ ( static_cast<unsigned>(3), static_pointer_cast<ConcreteIntTypeParamPtr>(arrayTypeB->getDimension())->getParam()->getValue());

	// check remaining type properties
	basicTypeTests(arrayTypeA, true, toList(arrayTypeA->getElementType(), arrayTypeA->getDimension()));
	basicTypeTests(arrayTypeB, false, toList(arrayTypeB->getElementType(), arrayTypeB->getDimension()));
}

TEST(TypeTest, VectorType) {

	// create type manager and element types
	NodeManager manager;
	TypePtr elementTypeA = GenericType::get(manager,"A");
	TypePtr elementTypeB = TypeVariable::get(manager,"a");

	// obtain array types
	IntTypeParamPtr param1 = ConcreteIntTypeParam::get(manager, 1);
	IntTypeParamPtr param3 = ConcreteIntTypeParam::get(manager, 3);
	VectorTypePtr vectorTypeA = VectorType::get(manager, elementTypeA, param1);
	VectorTypePtr vectorTypeB = VectorType::get(manager, elementTypeB, param3);

	// check names
	EXPECT_EQ ( "vector<A,1>", toString(*vectorTypeA) );
	EXPECT_EQ ( "vector<'a,3>", toString(*vectorTypeB) );

	// check element types
	EXPECT_EQ ( elementTypeA, vectorTypeA->getElementType() );
	EXPECT_EQ ( elementTypeB, vectorTypeB->getElementType() );

	// check dimensions
	ASSERT_TRUE ( vectorTypeA->getSize()->getNodeType()==NT_ConcreteIntTypeParam);
	ASSERT_TRUE ( vectorTypeB->getSize()->getNodeType()==NT_ConcreteIntTypeParam);
	EXPECT_EQ ( static_cast<unsigned>(1), static_pointer_cast<const ConcreteIntTypeParam>(vectorTypeA->getSize())->getValue());
	EXPECT_EQ ( static_cast<unsigned>(3), static_pointer_cast<const ConcreteIntTypeParam>(vectorTypeB->getSize())->getValue());

	// check remaining type properties
	basicTypeTests(vectorTypeA, true, toList(vectorTypeA->getElementType(), vectorTypeA->getSize()));
	basicTypeTests(vectorTypeB, false, toList(vectorTypeB->getElementType(), vectorTypeB->getSize()));
}

TEST(TypeTest, ChannelType) {

	// create type manager and element types
	NodeManager manager;
	TypePtr elementTypeA = GenericType::get(manager,"A");
	TypePtr elementTypeB = TypeVariable::get(manager,"a");

	// obtain array types
	IntTypeParamPtr param1 = ConcreteIntTypeParam::get(manager, 1);
	IntTypeParamPtr param3 = ConcreteIntTypeParam::get(manager, 3);
	ChannelTypePtr channelTypeA = ChannelType::get(manager, elementTypeA, param1);
	ChannelTypePtr channelTypeB = ChannelType::get(manager, elementTypeB, param3);

	// check names
	EXPECT_EQ ( "channel<A,1>", toString(*channelTypeA) );
	EXPECT_EQ ( "channel<'a,3>", toString(*channelTypeB) );

	// check element types
	EXPECT_EQ ( elementTypeA, channelTypeA->getElementType() );
	EXPECT_EQ ( elementTypeB, channelTypeB->getElementType() );

	// check dimensions
	ASSERT_TRUE ( channelTypeA->getSize()->getNodeType()==NT_ConcreteIntTypeParam);
	ASSERT_TRUE ( channelTypeB->getSize()->getNodeType()==NT_ConcreteIntTypeParam);
	EXPECT_EQ ( static_cast<unsigned>(1), static_pointer_cast<const ConcreteIntTypeParam>(channelTypeA->getSize())->getValue());
	EXPECT_EQ ( static_cast<unsigned>(3), static_pointer_cast<const ConcreteIntTypeParam>(channelTypeB->getSize())->getValue());

	// check remaining type properties
	basicTypeTests(channelTypeA, true, toList(channelTypeA->getElementType(), channelTypeA->getSize()));
	basicTypeTests(channelTypeB, false, toList(channelTypeB->getElementType(), channelTypeB->getSize()));
}

TEST(TypeTest, RefType) {

	// create type manager and element types
	NodeManager manager;
	TypePtr elementTypeA = GenericType::get(manager,"A");
	TypePtr elementTypeB = TypeVariable::get(manager,"a");

	// obtain array types
	RefTypePtr refTypeA = RefType::get(manager, elementTypeA);
	RefTypePtr refTypeB = RefType::get(manager, elementTypeB);

	// check names
	EXPECT_EQ ( "ref<A>", toString(*refTypeA) );
	EXPECT_EQ ( "ref<'a>", toString(*refTypeB) );

	// check element types
	EXPECT_EQ ( elementTypeA, refTypeA->getElementType() );
	EXPECT_EQ ( elementTypeB, refTypeB->getElementType() );

	// check remaining type properties
	basicTypeTests(refTypeA, true, toList(toVector(elementTypeA)));
	basicTypeTests(refTypeB, false, toList(toVector(elementTypeB)));
}


TEST(TypeTest, IntTypeParam) {

	NodeManager manager;

	// test toString format
	ConcreteIntTypeParamPtr p12 = ConcreteIntTypeParam::get(manager, 12);
	EXPECT_EQ (toString(*p12), "12");

	InfiniteIntTypeParamPtr inf = InfiniteIntTypeParam::get(manager);
	EXPECT_EQ (toString(*inf), "inf");

	VariableIntTypeParamPtr pvp = VariableIntTypeParam::get(manager, 'p');
	EXPECT_EQ (toString(*pvp), "#p");

	// test == operator
	IntTypeParamPtr params[] = {p12, inf, pvp};
	for (int i=0; i<3; i++) {
		for (int j=0; j<3; j++) {
			EXPECT_EQ(params[i]==params[j], i==j);
		}
	}

	IntTypeParamPtr p12b = ConcreteIntTypeParam::get(manager, 12);
	EXPECT_TRUE (p12 == p12b);

	IntTypeParamPtr pvpb = VariableIntTypeParam::get(manager, 'p');
	EXPECT_TRUE (pvp == pvpb);

	IntTypeParamPtr infb = InfiniteIntTypeParam::get(manager);
	EXPECT_TRUE (inf == infb);

	// conduct basic node checks
	basicNodeTests(p12, toList(p12->getParam()));
	basicNodeTests(inf);
	basicNodeTests(pvp, toList(pvp->getSymbol()));
}


template<typename PT>
void basicTypeTests(PT type, bool concrete, const NodeList& children) {

	typedef typename PT::element_type T;

	// ------------- Basic Node Tests ----------------

	basicNodeTests(type, children);

	// ------------ Type Ptr based tests -------------

	// check concrete flag
	// TODO: implement alternative is-concrete check
	//EXPECT_EQ( concrete, type->isConcrete() );

	// ------------ Type Token based tests -------------

	// copy and clone the type
	NodeManager manager;
	T copy = T(*type);
	T* clone = &*manager.get(type);

	// check whether all are equal
	T* all[] = { &*type, &copy, clone };
	for (int i=0; i<3; i++) {
		for (int j=0; j<3; j++) {

			T* a = all[i];
			T* b = all[j];

			EXPECT_EQ ( *a , *b );
			EXPECT_EQ ( a->hash(), b->hash() );
			EXPECT_EQ ( toString(*a), toString(*b) );

		}
	}

	// check type properties
	for (int i=0; i<3; i++) {

		T* cur = all[i];

		// check concrete flag
		// TODO: implement alternative is-concrete check
		//EXPECT_EQ( concrete, cur->isConcrete() );

		// check children
		EXPECT_TRUE( equals(children, cur->getChildList(), equal_target<NodePtr>()) );
	}
}

} // end namespace core
} // end namespace insieme

