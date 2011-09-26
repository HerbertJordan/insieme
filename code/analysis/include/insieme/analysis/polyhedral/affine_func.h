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

#include <iterator>
#include <stdexcept>

#include "insieme/analysis/polyhedral/iter_vec.h"
#include "insieme/utils/printable.h"

namespace insieme {
namespace analysis {
namespace poly {

//===== Exceptions =================================================================================
struct NotAffineExpr : public std::logic_error {
	const core::ExpressionPtr expr;
	std::string msg;

	NotAffineExpr(const core::ExpressionPtr& expr) : std::logic_error(""), expr(expr) {
		std::ostringstream ss;
		ss << "Expression '" << *expr << "' is not a linear affine function";
		msg = ss.str();
	}
	
	virtual const char* what() const throw() { return msg.c_str(); }

	~NotAffineExpr() throw () { }
};

struct VariableNotFound : public std::logic_error {
	const core::VariablePtr var;
	VariableNotFound(const core::VariablePtr& var);
	~VariableNotFound() throw () { }
};

// Forward Declarations for Constraints 
class Constraint;

class ConstraintCombiner;

typedef std::shared_ptr<ConstraintCombiner> ConstraintCombinerPtr; 

/**************************************************************************************************
 * AffineFunction represents an affine function based on an iteration vector. An
 * affine linear function is a function in the form:
 *
 * 3 * a + 2 * b ... + 0 
 *
 * Variables are either iterators or parameters. The representation is done keeping a vector of
 * coefficients referring to an iteration vector. For example considering the iteration vector of
 * the form (a, b, ... 1), the coefficient matrix we need to store to represent the previous affine
 * function is: (3, 2, ... 0). 
 *
 * Because we want to be able to change the dimensions of the iteration vector during the
 * construction of a SCoP, the affine function should refer to an iteration vector which size may
 * change. But because new iterators or parameters are always append, we can easily create the new
 * coefficient matrix for the mutated iteration vector, thanks to the sep member.  
 *************************************************************************************************/
class AffineFunction : public boost::noncopyable, public utils::Printable, 
	public boost::equality_comparable<AffineFunction> { 
	// Iteration Vector to which this function refers to 
	const IterationVector& iterVec;

	// List of integer coefficients (the polyhedral model does not allow to represent non integer
	// coefficients)
	std::vector<int> coeffs;

	// Keeps the information of the number of iterators the iteration vector had when this affine
	// function was created. This will allow us to produce the updated coefficient matrix in the
	// case new parameters or iterators are added to the iterVec. 
 	size_t sep;

	/**
	 * Converts an index in the iteration vector pointing to element E to an index on the
	 * coefficient vector which points to the coefficient value associated to element E.
	 *
	 * Returns -1 in the case E was created after this instance of affine function is generated. In
	 * that case the coefficient value for the index is by default zero.  
	 */
	int idxConv(size_t idx) const;

	void setCoeff(size_t idx, int coeff) { 
		assert(idx < coeffs.size()); 
		coeffs[idx] = coeff; 
	}

	int getCoeff(size_t idx) const;

public:

	static const unsigned PRINT_ZEROS = 0x01;
	static const unsigned PRINT_VARS  = 0x10;

	typedef std::pair<const Element&, int> Term;
	/**
	 * Class utilized to build iterators over Affine Functions. 
	 *
	 * The iterator returns a pair<Element,int> containing the element and the coefficient
	 * associated to it. 
	 */
	struct iterator : public boost::forward_iterator_helper<iterator, Term> {

		const IterationVector& iterVec;

		const AffineFunction& af;
		size_t iterPos;

		iterator(const IterationVector& iterVec, const AffineFunction& af, size_t iterPos=0) : 
			iterVec(iterVec), af(af), iterPos(iterPos) { }

		Term operator*() const; 
		iterator& operator++();

		bool operator==(const iterator& rhs) const { 
			return &iterVec == &rhs.iterVec && &af == &rhs.af && iterPos == rhs.iterPos;
		}
	};

	AffineFunction(const IterationVector& iterVec) : 
		iterVec(iterVec), coeffs( iterVec.size() ), sep( iterVec.getIteratorNum() ) { }

	AffineFunction(IterationVector& iterVec, const insieme::core::ExpressionPtr& expr);

	AffineFunction(IterationVector& iterVec, const std::vector<int>& coeffVec) : 
		iterVec(iterVec), coeffs(coeffVec), sep( iterVec.getIteratorNum() ) {
		assert(coeffVec.size() == iterVec.size());
	}

	// This constructor is defined private because client of this class should not 
	// be able to invoke it. Only the Constraint class makes use of it therefore 
	// it is defined friend 
	AffineFunction(const AffineFunction& other) : 
		iterVec(other.iterVec), coeffs(other.coeffs), sep(other.sep) { }

	inline const IterationVector& getIterationVector() const { return iterVec; }

	// Setter and Getter for coefficient values. 
	inline void setCoeff(const Element& iter, int coeff) {
		setCoeff(iterVec.getIdx(iter), coeff);
	}
	void setCoeff(const core::VariablePtr& var, int coeff);

	int getCoeff(const Element& elem) const;
	int getCoeff(const core::VariablePtr& var) const;

	inline iterator begin() const { return iterator(iterVec, *this); }
	inline iterator end() const { return iterator(iterVec, *this, iterVec.size()); }

	inline size_t size() const { return iterVec.size(); } 

	// Implements the Printable interface 
	std::ostream& printTo(std::ostream& out) const;

	std::string toStr(unsigned policy=PRINT_VARS | PRINT_ZEROS) const;

	bool operator==(const AffineFunction& other) const;

	/**
	 * Creates a copy of this affine function using another iteration vector as a base. This method
	 * can be invoked both providing a transformation map. In the case the transfomration map is not
	 * provided, it will be recomputed by the method. 
	 *
	 * The created affine function will be based on the iteration vector (iterVec), meaning that the
	 * user is responsable for the instance of iterVec to remain alive as long as the created Affine
	 * function is utilized. 
	 */
	AffineFunction 
	toBase(const IterationVector& iterVec, const IndexTransMap& idxMap = IndexTransMap()) const; 
};

} // end poly namespace
} // end analysis namespace 
} // end insieme namespace 

namespace std {
std::ostream& operator<<(std::ostream& out, const insieme::analysis::poly::AffineFunction::Term& c);
} // end std namespace 
