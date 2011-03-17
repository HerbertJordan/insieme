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

#pragma once

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <ostream>
#include <vector>
#include <sstream>

#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>

#include "insieme/utils/container_utils.h"
#include "insieme/utils/instance_manager.h"
#include "insieme/utils/string_utils.h"

#include "insieme/core/ast_node.h"
#include "insieme/core/identifier.h"
#include "insieme/core/int_type_param.h"

using std::string;
using std::vector;
using std::map;

namespace insieme {
namespace core {

// ---------------------------------------- A token for an abstract type ------------------------------

/**
 * The base type for all type tokens. Type tokens are immutable instances of classes derived from this base
 * class and are used to represent the type of data elements and functions (generally types) within the IR.
 */
class Type : public Node {

	/**
	 * The name of this type as a string (the result of the toString).
	 */
	mutable boost::optional<string> typeName;

protected:

	/**
	 * Creates a new type using the given name. The constructor is public, however, since
	 * this class is an abstract class, no actual instance can be created.
	 *
	 * @param name the unique name for this type
	 * @param concrete a flag indicating whether this type is a concrete type or represents a family of types
	 * 					due to type variables. Default: true
	 * @param functionType a flag indicating whether this type is a function type or not. Default: false
	 */
	Type(NodeType type, const std::size_t& hashCode)
		: Node(type, NC_Type, hashCode) { }

public:

	/**
	 * A simple, virtual destructor for this abstract class.
	 */
	virtual ~Type() { }

	/**
	 * A default implementation of the equals operator comparing the actual
	 * names of the types.
	 */
	bool equals(const Node& other) const;

	/**
	 * Requests an actual type implementation to print a string representation of
	 * itself into the given output stream.
	 */
	virtual std::ostream& printTypeTo(std::ostream& out) const = 0;

	/**
	 * Implements the printTo method as requested by the basic node definition. However,
	 * for performance reasons the string-variant of a type is cached within a lazy evaluated
	 * internal string.
	 */
	virtual std::ostream& printTo(std::ostream& out) const {
		if (!typeName) {
			std::stringstream res;
			printTypeTo(res);
			typeName.reset(res.str());
		}
		return out << *typeName;
	}


protected:

	/**
	 * An abstract method to be implemented by sub-types to realize the actual type
	 * comparison.
	 */
	virtual bool equalsType(const Type& type) const = 0;

};


/**
 * A type definition for a list of types, frequently used throughout the type definitions.
 */
typedef std::vector<TypePtr> TypeList;


// ---------------------------------------- A class for type variables  ------------------------------

/**
 * Tokens of this type are used to represent type variables. Instances them-self represent types,
 * yet no concrete ones.
 */
class TypeVariable: public Type {

	/**
	 * The name of the represented variable.
	 */
	std::string varName;

public:

	/**
	 * Creates a new type variable using the given name.
	 *
	 * @param name the name of the type variable to be created
	 */
	TypeVariable(const string& name);

protected:

	/**
	 * Creates the list of child nodes of this type. Since the type variable is a
	 * terminal node, the resulting list will always be empty.
	 */
	virtual OptionChildList getChildNodes() const {
		// return an option child list filled with an empty list
		return OptionChildList(new ChildList());
	}

	/**
	 * Compares this type variable with the given type.
	 */
	virtual bool equalsType(const Type& type) const;

public:

	/**
	 * This method provides a static factory method for this type of node. It will return
	 * a type variable pointer pointing toward a variable with the given name maintained by the
	 * given manager.
	 */
	static TypeVariablePtr get(NodeManager& manager, const string& name);

	/**
	 * This static factory method allows to construct a type variable based on an identifier.
	 *
	 * @param manager the manager used for maintaining instances of this class
	 * @param id the identifier defining the name of the resulting type variable
	 * @return the requested type instance managed by the given manager
	 */
	static TypeVariablePtr getFromId(NodeManager& manager, const IdentifierPtr& id);

	/**
	 * Prints a string-representation of this type to the given output stream.
	 */
	virtual std::ostream& printTypeTo(std::ostream& out) const;

	/**
	 * Obtains the name of the variable represented by this instance.
	 */
	const std::string getVarName() const {
		return varName;
	}

	/**
	 * The less-than operation supported between type variables to allow those
	 * to be used within tree-based map implementations.
	 */
	bool operator<(const TypeVariable& other) const {
		return varName < other.varName;
	}

private:

	/**
	 * Creates a clone of this node.
	 */
	virtual TypeVariable* createCopyUsing(NodeMapping&) const;

};

// ---------------------------------------- A tuple type ------------------------------

/**
 * The tuple type represents a special kind of type representing a simple aggregation
 * (cross-product) of other types. It thereby forms the foundation for functions
 * accepting multiple input parameters.
 */
class TupleType: public Type {

	/**
	 * The list of element types this tuple is consisting of.
	 */
	const TypeList elementTypes;

private:

	/**
	 * Creates a new tuple type based on the given element types.
	 */
	TupleType(const TypeList& elementTypes);

protected:

	/**
	 * Creates the list of child nodes of this type.
	 */
	virtual OptionChildList getChildNodes() const;

	/**
	 * Compares this type with the given type.
	 */
	virtual bool equalsType(const Type& type) const;

public:

	/**
	 * Obtains the list of types constituting this tuple type.
	 */
	const TypeList& getElementTypes() const {
		return elementTypes;
	}

	/**
	 * This method provides a static factory method for an empty tuple type.
	 *
	 * @param manager the manager to obtain the new type reference from
	 */
	static TupleTypePtr getEmpty(NodeManager& manager);

	/**
	 * This method provides a static factory method for this type of node. It will return
	 * a tuple type pointer pointing toward a variable with the given name maintained by the
	 * given manager.
	 *
	 * @param manager the manager to obtain the new type reference from
	 * @param elementTypes the list of element types to be used to form the tuple
	 */
	static TupleTypePtr get(NodeManager& manager, const TypeList& elementTypes = TypeList());

	/**
	 * Prints a string-representation of this type to the given output stream.
	 */
	virtual std::ostream& printTypeTo(std::ostream& out) const;

private:

	/**
	 * Creates a clone of this node.
	 */
	virtual TupleType* createCopyUsing(NodeMapping& mapper) const;

};

// ---------------------------------------- Function Type ------------------------------

/**
 * This type corresponds to the type of a function. It specifies the argument types and the
 * value returned by the members of this type.
 */
class FunctionType: public Type {

	/**
	 * The list of captured types.
	 */
	const TypeList captureTypes;

	/**
	 * The list of argument types.
	 */
	const TypeList argumentTypes;

	/**
	 * The type of value produced by this function type.
	 */
	const TypePtr returnType;

private:

	/**
	 * Creates a new instance of this type based on the given in and output types.
	 *
	 * @param captureTypes a reference to the type captured by this function type
	 * @param argumentTypes a reference to the type used as argument types
	 * @param returnType a reference to the type used as return type
	 */
	FunctionType(const TypeList& captureTypes, const TypeList& argumentTypes, const TypePtr& returnType);

protected:

	/**
	 * Creates a empty child list since this node represents a terminal node.
	 */
	virtual OptionChildList getChildNodes() const;

	/**
	 * Compares this type with the given type.
	 */
	virtual bool equalsType(const Type& type) const;

public:

	/**
	 * This method provides a static factory method for this type of node. It will return
	 * a function type pointer pointing toward a variable with the given name maintained by the
	 * given manager.
	 *
	 * @param manager the manager to be used for handling the obtained type pointer
	 * @param argumentTypes the arguments accepted by the resulting function type
	 * @param returnType the type of value to be returned by the obtained function type
	 * @return a pointer to a instance of the required type maintained by the given manager
	 */
	static FunctionTypePtr get(NodeManager& manager, const TypeList& argumentTypes, const TypePtr& returnType);


	/**
	 * This method provides a static factory method for this type of node. It will return
	 * a function type pointer pointing toward a variable with the given name maintained by the
	 * given manager.
	 *
	 * @param manager the manager to be used for handling the obtained type pointer
	 * @param captureTypes the list of capture arguments accepted by the resulting function type
	 * @param argumentTypes the arguments accepted by the resulting function type
	 * @param returnType the type of value to be returned by the obtained function type
	 * @return a pointer to a instance of the required type maintained by the given manager
	 */
	static FunctionTypePtr get(NodeManager& manager, const TypeList& captureTypes, const TypeList& argumentTypes, const TypePtr& returnType);

	/**
	 * Obtains a reference to the internally maintained list of capture types.
	 *
	 * @return a reference to the list of capture types.
	 */
	const TypeList& getCaptureTypes() const {
		return captureTypes;
	}

	/**
	 * Obtains a reference to the internally maintained list of argument types.
	 *
	 * @return a reference to the list of argument types.
	 */
	const TypeList& getArgumentTypes() const {
		return argumentTypes;
	}

	/**
	 * Obtains a reference to the internally maintained result type.
	 *
	 * @return a reference to the result type.
	 */
	const TypePtr& getReturnType() const {
		return returnType;
	}

	/**
	 * Prints a string-representation of this type to the given output stream.
	 */
	virtual std::ostream& printTypeTo(std::ostream& out) const;

private:

	/**
	 * Creates a clone of this node.
	 */
	virtual FunctionType* createCopyUsing(NodeMapping& mapper) const;

};


// ---------------------------------------- Generic Type ------------------------------

/**
 * This type represents a generic type which can be used to represent arbitrary user defined
 * or derived types. Each generic type can be equipped with a number of generic type and integer
 * parameters. Those are represented using other types and IntTypeParam instances.
 */
class GenericType: public Type {

	/**
	 * The name of this generic type.
	 */
	const string familyName;

	/**
	 * The list of type parameters being part of this type specification.
	 */
	const vector<TypePtr> typeParams;

	/**
	 * The list of integer type parameter being part of this type specification.
	 */
	const vector<IntTypeParamPtr> intParams;

	/**
	 * The base type of this type if there is any. The pointer is pointing toward
	 * the base type or is NULL in case there is no base type (hence, it would be
	 * an abstract type).
	 */
	const TypePtr baseType;

protected:

	/**
	 * Creates an new generic type instance based on the given parameters.
	 *
	 * @param name 			the name of the new type (only the prefix)
	 * @param typeParams	the type parameters of this type, concrete or variable
	 * @param intTypeParams	the integer-type parameters of this type, concrete or variable
	 * @param baseType		the base type of this generic type
	 */
	GenericType(const string& name,
			const vector<TypePtr>& typeParams = vector<TypePtr> (),
			const vector<IntTypeParamPtr>& intTypeParams = vector<IntTypeParamPtr>(),
			const TypePtr& baseType = NULL);

	/**
	 * A special constructor which HAS to be used by all sub-classes to ensure
	 * that the node type token is matching the actual class type.
	 *
	 * @param nodeType		the token to be used to identify the type of this node
	 * @param hashSeed		the seed to be used for computing a hash value
	 * @param name 			the name of the new type (only the prefix)
	 * @param typeParams	the type parameters of this type, concrete or variable
	 * @param intTypeParams	the integer-type parameters of this type, concrete or variable
	 * @param baseType		the base type of this generic type
	 */
	GenericType(NodeType nodeType,
			std::size_t hashSeed,
			const string& name,
			const vector<TypePtr>& typeParams = vector<TypePtr> (),
			const vector<IntTypeParamPtr>& intTypeParams = vector<IntTypeParamPtr>(),
			const TypePtr& baseType = NULL);


	/**
	 * Obtains a list of all type parameters and the optional base type
	 * referenced by this generic type.
	 */
	virtual OptionChildList getChildNodes() const;

	/**
	 * Compares this type with the given type.
	 */
	virtual bool equalsType(const Type& type) const;

public:

	/**
	 * This method provides a static factory method for this type of node. It will return
	 * a generic type pointer pointing toward a variable with the given name maintained by the
	 * given manager.
	 *
	 * @param manager		the manager to be used for creating the node (memory management)
	 * @param name 			the name of the new type (only the prefix)
	 * @param typeParams	the type parameters of this type, concrete or variable
	 * @param intTypeParams	the integer-type parameters of this type, concrete or variable
	 * @param baseType		the base type of this generic type
	 */
	static GenericTypePtr get(NodeManager& manager,
			const string& name,
			const vector<TypePtr>& typeParams = vector<TypePtr>(),
			const vector<IntTypeParamPtr>& intTypeParams = vector<IntTypeParamPtr>(),
			const TypePtr& baseType = NULL);

	/**
	 * This method provides a static factory method for this type of node. It will return
	 * a generic type pointer pointing toward a variable with the given name maintained by the
	 * given manager.
	 *
	 * @param manager		the manager to be used for creating the node (memory management)
	 * @param name 			the name of the new type (only the prefix)
	 * @param typeParams	the type parameters of this type, concrete or variable
	 * @param intTypeParams	the integer-type parameters of this type, concrete or variable
	 * @param baseType		the base type of this generic type
	 */
	static GenericTypePtr getFromID(NodeManager& manager,
			const IdentifierPtr& name,
			const vector<TypePtr>& typeParams = vector<TypePtr> (),
			const vector<IntTypeParamPtr>& intTypeParams = vector<IntTypeParamPtr>(),
			const TypePtr& baseType = NULL);

	/**
	 * Obtains the family name of this generic type.
	 */
	const string& getFamilyName() const {
		return familyName;
	}

	/**
	 * Retrieves all type parameter associated to this generic type.
	 *
	 * @return a const reference to the internally maintained type parameter list.
	 */
	const vector<TypePtr>& getTypeParameter() const {
		return typeParams;
	}

	/**
	 * Retrieves a list of all integer type parameters associated to this type.
	 *
	 * @return a const reference to the internally maintained integer type parameter list.
	 */
	const vector<IntTypeParamPtr>& getIntTypeParameter() const {
		return intParams;
	}

	/**
	 * Retrieves a reference to the base type associated with this type.
	 *
	 * @return a reference to the base type of this type.
	 */
	const TypePtr& getBaseType() const {
		return baseType;
	}

	/**
	 * Prints a string-representation of this type to the given output stream.
	 */
	virtual std::ostream& printTypeTo(std::ostream& out) const;

private:

	/**
	 * Creates a clone of this node.
	 */
	virtual GenericType* createCopyUsing(NodeMapping& mapper) const;

};

// ---------------------------------------- Recursive Type ------------------------------


class RecTypeDefinition : public Node {

public:

	/**
	 * The data type used for maintaining recursive type definitions. The tree based
	 * std::map is required to fix the order of the contained elements.
	 */
	typedef std::map<TypeVariablePtr, TypePtr, compare_target<TypeVariablePtr>> RecTypeDefs;

private:

	/**
	 * The list of definitions this recursive type definition is consisting of.
	 */
	const RecTypeDefs definitions;

	/**
	 * A constructor for this type of node.
	 */
	RecTypeDefinition(const RecTypeDefs& definitions);

	/**
	 * Creates a clone of this node.
	 */
	RecTypeDefinition* createCopyUsing(NodeMapping& mapper) const;

protected:

	/**
	 * Compares this node with the given node.
	 */
	virtual bool equals(const Node& other) const;

	/**
	 * Obtains a list of child nodes.
	 */
	virtual OptionChildList getChildNodes() const;

public:

	/**
	 * Constructs a new node of this type based on the given type definitions.
	 */
	static RecTypeDefinitionPtr get(NodeManager& manager, const RecTypeDefs& definitions);

	/**
	 * Obtains the definitions constituting this type of node.
	 */
	const RecTypeDefs& getDefinitions() const{
		return definitions;
	}

	/**
	 * Obtains a specific definition maintained within this node.
	 */
	const TypePtr getDefinitionOf(const TypeVariablePtr& variable) const;

	/**
	 * Unrolls this definition once for the given variable.
	 *
	 * @param manager the manager to be used for maintaining the resulting type pointer
	 * @param variable the variable defining the definition to be unrolled once
	 * @return the resulting, unrolled type
	 */
	TypePtr unrollOnce(NodeManager& manager, const TypeVariablePtr& variable) const;

	/**
	 * Prints a string representation of this node to the given output stream.
	 */
	virtual std::ostream& printTo(std::ostream& out) const;

};

/**
 * This type connector allows to define recursive type within the IR language. Recursive
 * types are types which are defined by referencing to their own type. The definition of
 * a list may be consisting of a pair, where the first element is corresponding to a head
 * element and the second to the remaining list. Therefore, a list is defined using its own
 * type.
 *
 * This implementation allows to define mutually recursive data types, hence, situations
 * in which the definition of multiple recursive types are interleaved.
 */
class RecType: public Type {

	/**
	 * The name of the type variable describing this type.
	 */
	const TypeVariablePtr typeVariable;

	/**
	 * The definition body of this recursive type. Identical definitions may be
	 * shared among recursive type definitions.
	 */
	const RecTypeDefinitionPtr definition;


	/**
	 * A constructor for creating a new recursive type.
	 */
	RecType(const TypeVariablePtr& typeVariable, const RecTypeDefinitionPtr& definition);

public:

	/**
	 * Obtains the definition-part of this recursive type.
	 */
	const RecTypeDefinitionPtr getDefinition() const {
		return definition;
	}

	/**
	 * Obtains the type variable referencing the represented representation within
	 * the associated definition.
	 */
	const TypeVariablePtr getTypeVariable() const { return typeVariable; }

	/**
	 * A factory method for obtaining a new recursive type instance.
	 *
	 * @param manager the manager which should be maintaining the new instance
	 * @param typeVariable the name of the variable used within the recursive type definition for representing the
	 * 					   recursive type to be defined by the resulting type instance.
	 * @param definition the definition of the recursive type.
	 */
	static RecTypePtr get(NodeManager& manager, const TypeVariablePtr& typeVariable, const RecTypeDefinitionPtr& definition);

	/**
	 * Unrolls this recursive type.
	 */
	TypePtr unroll(NodeManager& manager) const {
		return definition->unrollOnce(manager, typeVariable);
	}

	/**
	 * Prints a string-representation of this type to the given output stream.
	 */
	virtual std::ostream& printTypeTo(std::ostream& out) const;

protected:

	/**
	 * Obtains a list of all sub-nodes referenced by this AST node.
	 */
	virtual OptionChildList getChildNodes() const;

	/**
	 * Compares this type with the given type.
	 */
	virtual bool equalsType(const Type& type) const;

private:

	/**
	 * Creates a clone of this node.
	 */
	virtual RecType* createCopyUsing(NodeMapping& mapper) const;

};


// --------------------------------- Named Composite Type ----------------------------

/**
 * This abstract type is used to form a common basis for the struct and union types which
 * both consist of a list of typed elements.
 */
class NamedCompositeType: public Type {

public:

	/**
	 * Defines the type used for representing a element entry.
	 */
	typedef std::pair<IdentifierPtr, TypePtr> Entry;

	/**
	 * The type used to represent a list of element entries.
	 */
	typedef vector<Entry> Entries;

private:

	/**
	 * The type entries this composed type is based on.
	 */
	const Entries entries;

protected:

	/**
	 * Creates a new named composite type having the given name and set of entries.
	 *
	 * @param nodeType the token to identify the actual type of the resulting node
	 * @param hashSeed	the seed to be used for computing a hash value
	 * @param prefix the prefix to be used for the new type (union or struct)
	 * @param entries the entries of the new type
	 *
	 * @throws std::invalid_argument if the same identifier is used more than once within the type
	 */
	NamedCompositeType(NodeType nodeType, std::size_t hashSeed, const string& prefix, const Entries& entries);

	/**
	 * Obtains a list of all child sub-types used within this struct.
	 */
	virtual OptionChildList getChildNodes() const;

	/**
	 * Compares this type with the given type.
	 */
	virtual bool equalsType(const Type& type) const;

public:

	/**
	 * Retrieves the set of entries this composed type is based on.
	 *
	 * @return a reference to the internally maintained list of entries.
	 */
	const Entries& getEntries() const {
		return entries;
	}

	/**
	 * Retrieves the type of a member of this struct or a null pointer if there is no
	 * such entry.
	 */
	const TypePtr getTypeOfMember(const IdentifierPtr& member) const;

	/**
	 * Prints a string-representation of this type to the given output stream.
	 */
	virtual std::ostream& printTypeTo(std::ostream& out) const;

};

// --------------------------------- Struct Type ----------------------------

/**
 * The type used to represent structs / records.
 */
class StructType: public NamedCompositeType {

	/**
	 * Creates a new instance of this type based on the given entries.
	 *
	 * @param entries the entries the new type should consist of
	 * @see NamedCompositeType::NamedCompositeType(const string&, const Entries&)
	 */
	StructType(const Entries& entries);

	/**
	 * Creates a clone of this type within the given manager.
	 */
	virtual StructType* createCopyUsing(NodeMapping& mapper) const;

public:

	/**
	 * A factory method allowing to obtain a pointer to a struct type representing
	 * an instance managed by the given manager.
	 *
	 * @param manager the manager which should be responsible for maintaining the new
	 * 				  type instance and all its referenced elements.
	 * @param entries the set of entries the new type should consist of
	 * @return a pointer to a instance of the requested type. Multiple requests using
	 * 		   the same parameters will lead to pointers addressing the same instance.
	 */
	static StructTypePtr get(NodeManager& manager, const Entries& entries);

};


// --------------------------------- Union Type ----------------------------

/**
 * The type used to represent unions.
 */
class UnionType: public NamedCompositeType {

	/**
	 * Creates a new instance of this type based on the given entries.
	 *
	 * @param elements the elements the new type should consist of
	 * @see NamedCompositeType::NamedCompositeType(const string&, const Entries&)
	 */
	UnionType(const Entries& elements);

	/**
	 * Creates a clone of this type within the given manager.
	 */
	virtual UnionType* createCopyUsing(NodeMapping& mapper) const;

public:

	/**
	 * A factory method allowing to obtain a pointer to a union type representing
	 * an instance managed by the given manager.
	 *
	 * @param manager the manager which should be responsible for maintaining the new
	 * 				  type instance and all its referenced elements.
	 * @param entries the set of entries the new type should consist of
	 * @return a pointer to a instance of the requested type. Multiple requests using
	 * 		   the same parameters will lead to pointers addressing the same instance.
	 */
	static UnionTypePtr get(NodeManager& manager, const Entries& entries);

};


// --------------------------------- Single Element Type ----------------------------

/**
 * This abstract type is used as a common base class for a serious of intrinsic types
 * which all require a single element type. It thereby represents a specialized version
 * of a generic type.
 */
class SingleElementType : public GenericType {
protected:

	/**
	 * Creates a new instance of this class using the given name, element type and
	 * integer parameters.
	 *
	 * @param nodeType the node type of the concrete implementation of this abstract class
	 * @param hashSeed	the seed to be used for computing a hash value
	 * @param name the (prefix) of the name of the new type
	 * @param elementType its single type parameter
	 * @param intTypeParams additional integer type parameters. By default this parameters
	 * 						will be set to an empty set.
	 */
	SingleElementType(NodeType nodeType, std::size_t hashSeed,
			const string& name, const TypePtr& elementType,
			const vector<IntTypeParamPtr>& intTypeParams = vector<IntTypeParamPtr> ());

public:

	/**
	 * Retrieves the one element type associated with this type.
	 *
	 * @return the element type associated with this type
	 */
	const TypePtr& getElementType() const {
		return getTypeParameter()[0];
	}
};

// --------------------------------- Array Type ----------------------------

/**
 * This intrinsic array type used to represent multidimensional rectangular arrays
 * within the type system.
 */
class ArrayType: public SingleElementType {

public:

	/**
	 * Creates a new instance of this class using the given parameters.
	 *
	 * @param elementType the element type of this array
	 * @param dim the dimension of the represented array
	 */
	ArrayType(const TypePtr& elementType, const IntTypeParamPtr& dim);

private:

	/**
	 * Creates a clone of this type within the given manager.
	 */
	virtual ArrayType* createCopyUsing(NodeMapping& mapper) const;

public:

	/**
	 * A factory method allowing to obtain a pointer to a array type representing
	 * an instance managed by the given manager. The dimension of the resulting array is
	 * one.
	 *
	 * @param manager 		the manager which should be responsible for maintaining the new
	 * 				  		type instance and all its referenced elements.
	 * @param elementType 	the type of element to be maintained within the array
	 * @return a pointer to a instance of the requested type. Multiple requests using
	 * 		   the same parameters will lead to pointers addressing the same instance.
	 */
	static ArrayTypePtr get(NodeManager& manager, const TypePtr& elementType);


	/**
	 * A factory method allowing to obtain a pointer to a array type representing
	 * an instance managed by the given manager.
	 *
	 * @param manager 		the manager which should be responsible for maintaining the new
	 * 				  		type instance and all its referenced elements.
	 * @param elementType 	the type of element to be maintained within the array
	 * @param dim 			the dimension of the requested array
	 * @return a pointer to a instance of the requested type. Multiple requests using
	 * 		   the same parameters will lead to pointers addressing the same instance.
	 */
	static ArrayTypePtr get(NodeManager& manager, const TypePtr& elementType, const IntTypeParamPtr& dim);

	/**
	 * Retrieves the dimension of the represented array.
	 *
	 * @return the dimension of the represented array type
	 */
	const IntTypeParamPtr& getDimension() const;
};

// --------------------------------- Vector Type ----------------------------

/**
 * This intrinsic vector type used to represent fixed sized arrays (=vectors).
 */
class VectorType : public SingleElementType {

public:

	/**
	 * Creates a new instance of a vector type token using the given element and size parameter.
	 *
	 * @param elementType the element type of the new vector
	 * @param size the size of the new vector
	 */
	VectorType(const TypePtr& elementType, const IntTypeParamPtr& size);

private:

	/**
	 * Creates a clone of this type within the given manager.
	 */
	virtual VectorType* createCopyUsing(NodeMapping& mapper) const;

public:

	/**
	 * A factory method allowing to obtain a pointer to a vector type representing
	 * an instance managed by the given manager.
	 *
	 * @param manager 		the manager which should be responsible for maintaining the new
	 * 				  		type instance and all its referenced elements.
	 * @param elementType 	the type of element to be maintained within the vector
	 * @param size 			the size of the requested vector
	 * @return a pointer to a instance of the requested type. Multiple requests using
	 * 		   the same parameters will lead to pointers addressing the same instance.
	 */
	static VectorTypePtr get(NodeManager& manager, const TypePtr& elementType, const IntTypeParamPtr& size);

	/**
	 * Retrieves the size (=number of elements) of the represented vector type.
	 *
	 * @return the size of the represented array type
	 */
	const IntTypeParamPtr& getSize() const;
};


// --------------------------------- Reference Type ----------------------------

/**
 * This intrinsic reference type used to represent mutable memory locations.
 */
class RefType: public SingleElementType {

public:

	/**
	 * A private constructor to create a new instance of this type class based on the
	 * given element type.
	 *
	 * @param elementType the type the new type should reference to.
	 */
	RefType(const TypePtr& elementType);

private:

	/**
	 * Creates a clone of this type within the given manager.
	 */
	virtual RefType* createCopyUsing(NodeMapping& mapper) const;

public:

	/**
	 * A factory method allowing to obtain a pointer to a reference type representing
	 * an instance managed by the given manager.
	 *
	 * @param manager 		the manager which should be responsible for maintaining the new
	 * 				  		type instance and all its referenced elements.
	 * @param elementType 	the type of element the requested reference is addressing
	 * @return a pointer to a instance of the requested type. Multiple requests using
	 * 		   the same parameters will lead to pointers addressing the same instance.
	 */
	static RefTypePtr get(NodeManager& manager, const TypePtr& elementType);

};

// --------------------------------- Channel Type ----------------------------

/**
 * This intrinsic reference type used to represent communication channels.
 */
class ChannelType: public SingleElementType {

public:

	/**
	 * Creates a new channel.
	 *
	 * @param elementType	the type of data to be communicated through this channel
	 * @param size			the buffer size of this channel, hence the number of elements which can be
	 * 						obtained within this channel until it starts blocking writte operations. If
	 * 						set to 0, the channel will represent a handshake channel.
	 */
	ChannelType(const TypePtr& elementType, const IntTypeParamPtr& size);

private:

	/**
	 * Creates a clone of this type within the given manager.
	 */
	virtual ChannelType* createCopyUsing(NodeMapping& mapper) const;

public:

	/**
	 * A factory method allowing to obtain a pointer to a reference type representing
	 * an instance managed by the given manager.
	 *
	 * @param manager 		the manager which should be responsible for maintaining the new
	 * 				  		type instance and all its referenced elements.
	 * @param elementType 	the type of element the requested reference is addressing
	 * @param size			the size of the channel to be created
	 * @return a pointer to a instance of the requested type. Multiple requests using
	 * 		   the same parameters will lead to pointers addressing the same instance.
	 */
	static ChannelTypePtr get(NodeManager& manager, const TypePtr& elementType, const IntTypeParamPtr& size);

	/**
	 * Retrieves the (buffer) size of this channel.
	 *
	 * @return the buffer size of the channel
	 */
	const IntTypeParamPtr& getSize() const;
};

} // end namespace core
} // end namespace insieme



