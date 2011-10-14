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

#include <boost/functional/hash.hpp>

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_base_of.hpp>
#include <boost/type_traits/is_convertible.hpp>

#include <insieme/utils/functional_utils.h>

namespace insieme {
namespace utils {

/**
 * A base type of plain, immutable data elements which should be maintained within a hash
 * based container.
 *
 * It provides efficient implementations of hash functions and comparison operations. Further
 * it provides means for using instances of this type within unordered sets derived from the
 * boost or std library.
 */
template<class Derived>
class HashableImmutableData {

	/**
	 * The hash value of this data element derived once during its construction. This value will be required
	 * frequently, hence evaluating it once and reusing it helps reducing computational overhead. Since
	 * instances are immutable, the hash does not have to be altered after the creation of a instance.
	 */
	std::size_t hashCode;

protected:

	/**
	 * A hooker method to be implemented by sub-classes to compare instances with other
	 * node instances.
	 *
	 * @param other the instance to be compared to. The handed in element will already be checked for
	 * 				identity and its hash value. Hence, simple checks may be omitted within
	 * 				the implementation of this method.
	 * @return true if equivalent, false otherwise.
	 */
	virtual bool equals(const Derived& other) const = 0;

public:

	/**
	 * Creates a new instance of this base type build upon the given hash code.
	 *
	 * @param hashCode the hash code of this immutable hashable data element
	 */
	HashableImmutableData(std::size_t hashCode) : hashCode(hashCode) {};

	/**
	 * A virtual destructor to clean up objects properly after usage.
	 */
	virtual ~HashableImmutableData() {};

	/**
	 * Computes a hash code for this node. The actual computation has to be conducted by
	 * subclasses. This class will only return the hash code passed on through the constructor.
	 *
	 * Note: this function is not virtual, so it should not be overridden in sub-classes.
	 *
	 * @return the hash code derived for this type.
	 */
	std::size_t hash() const {
		// retrieve cached hash code
		return hashCode;
	}


	/**
	 * Compares two instances of this type by checking for their identity (same
	 * memory location) and hash code. If those tests are not conclusive, the virtual
	 * function equals will be invoked.
	 */
	bool operator==(const Derived& other) const {
		// test for identity
		if (this == &other) {
			return true;
		}

		// fast hash code test
		if (hashCode != other.hash()) {
			return false;
		}

		// use virtual equals method
		return equals(other);
	}

	/**
	 * Implementing the not-equal operator for AST nodes by negating the equals
	 * operator.
	 */
	bool operator!=(const Derived& other) const {
		return !(*this == other);
	}
};

/**
 * Integrates the hash code computation for nodes into the boost hash code framework.
 *
 * @param instance the instance for which a hash code should be obtained.
 * @return the hash code of the given instance
 */
template<typename Derived>
inline std::size_t hash_value(const insieme::utils::HashableImmutableData<Derived>& instance) {
	return instance.hash();
}


/**
 * The terminal case for the hash combine operation (where no values are left).
 *
 * @param seed the seed to which no additional hash value should be appended to
 * @return the resulting hash value (the handed in seed)
 */
inline std::size_t appendHash(std::size_t& seed) {
	// nothing to do
	return seed;
}

/**
 * An alternative terminal case for the hash append function accepting specified
 * template parameters.
 */
template<
	typename T,
	typename Extractor = id<T>
>
inline std::size_t appendHash(std::size_t& seed) {
	// nothing to do
	return seed;
}

/**
 * The generic implementation of the hash combine operation.
 * @param seed the hash seed to which the hash values of the given arguments should be appended to
 * @param first the first of the elements to be hashed and appended
 * @param rest the remaining elements to be hashed and appended
 * @return the resulting hash value
 */
template<
	typename T,
	typename Extractor = id<T>,
	typename... Args
>
inline std::size_t appendHash(std::size_t& seed, const T& first, const Args&... rest) {
	static Extractor ext;
	boost::hash_combine(seed, ext(first));
	appendHash(seed, rest...);
	return seed;
}


/**
 * The terminal case for the variadic template based implementation of the combineHashes function.
 *
 * @return the initial hash seed corresponding to an empty tuple (0)
 */
inline std::size_t combineHashes() {
	return 0;
}

/**
 * A generic hash combining function computing a hash value for the given list of elements.
 *
 * @param first the first of the elements to be hashed
 * @param rest the remaining elements to be hashed
 * @return the resulting hash value
 */
template<
	typename T,
	typename Extractor = id<T>,
	typename ... Args
>
inline std::size_t combineHashes(const T& first, const Args&... rest) {
	// initialize hash seed
	std::size_t seed = 0;

	// append all the hash values
	appendHash<T,Extractor>(seed, first, rest...);
	return seed;
}

// --------------------------------------------------------------------------------------------------------------
// 												Hashing Containers
// --------------------------------------------------------------------------------------------------------------

/**
 * This functor can be used to print elements to an output stream.
 */
template<typename Extractor>
struct hash : public std::unary_function<const typename Extractor::argument_type&, std::size_t> {
	Extractor extractor;
	std::size_t operator()(const typename Extractor::argument_type& cur) const {
		boost::hash<typename Extractor::argument_type> hasher;
		return hasher(cur);
	}
};

/**
 * A generic method capable of computing a hash value for a ordered container (list, trees, ...).
 *
 * @param container the container for which a hash value should be computed
 * @return the computed hash value
 */
template<typename Container>
std::size_t hashList(const Container& container) {
	typedef typename Container::value_type Element;
	return hashList(container, hash<id<const Element&>>());
}

/**
 * A generic method capable of computing a hash value for a ordered container (list, trees, ...).
 *
 * @param container the container for which a hash value should be computed
 * @param hasher the hasher used for deriving a hash value for each element within the container
 * @return the computed hash value
 */
template<typename Container, typename Hasher>
std::size_t hashList(const Container& container, Hasher hasher) {
	typedef typename Container::value_type Element;

	std::size_t seed = 0;
	hashList(seed, container, hasher);
	return seed;
}

/**
 * A generic method capable of computing a hash value for a ordered container (list, trees, ...).
 *
 * @param seed the hash value to be manipulated by introducing the elements of the list
 * @param container the container for which a hash value should be computed
 * @param hasher the hasher used for deriving a hash value for each element within the container
 */
template<typename Container, typename Hasher>
void hashList(std::size_t& seed, const Container& container, Hasher hasher) {
	typedef typename Container::value_type Element;

	// combine hashes of all elements within the container
	std::for_each(container.begin(), container.end(), [&](const Element& cur) {
		boost::hash_combine(seed, hasher(cur));
	});
}

} // end namespace utils
} // end namespace insieme

namespace std
{

	// NOTE: isn't working - and solution is ugly
	// see: http://stackoverflow.com/questions/1032973/how-to-partially-specialize-a-class-template-for-all-derived-types
	// TODO: find a work-around

	/**
	 * A specialization of the std-hash struct to be used for realizing hashing of
	 * HashableImmutableData within the std-based hashing data structures.
	 */
//	template <typename Derived>
//	struct hash<Derived, typename boost::enable_if<boost::is_base_of<insieme::utils::Hashable,Derived>, Derived>::type> : public std::unary_function<insieme::utils::Hashable, size_t> {
//		size_t operator()(const insieme::utils::Hashable& instance) {
//			return instance.hash();
//		}
//	};
}


