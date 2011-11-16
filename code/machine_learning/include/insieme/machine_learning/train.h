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

#include "KompexSQLitePrerequisites.h"
#include "KompexSQLiteDatabase.h"
#include "KompexSQLiteStatement.h"
#include "KompexSQLiteException.h"
//#include "KompexSQLiteStreamRedirection.h"
//#include "KompexSQLiteBlob.h"

#include "Array/Array.h"
#include "ReClaM/FFNet.h"

#define POS 1
#define NEG 0

namespace ml {

class MachineLearningException : std::exception {
    std::string err;
public :
	const char* what() const throw() {
		return ("Machine Learning Error! \n" + err).c_str();
	}

	MachineLearningException() : err("") {}

    MachineLearningException(std::string errMsg) : err(errMsg) {}

    ~MachineLearningException() throw() {}
};


class Trainer {
	Kompex::SQLiteDatabase *pDatabase;
	Kompex::SQLiteStatement *pStmt;

	std::vector<std::string> features;
	Model& model;

	unsigned int getMaximum();

	double sharkEarlyStopping(Optimizer& optimizer, ErrorFunction& errFct, Array<double>& in, Array<double>& target, size_t validatonSize);


	/* Splits the training dataset in two pieces of validationSieze% for validaton and 100-validatonSize% for validation
	 * @param
	 * errFct the Error function to be use
	 * in the input to the model
	 * targed the desired outputs for the given inputs
	 * validationSize the size of the validation size in percent
	 */
	double earlyStopping(Optimizer& optimizer, ErrorFunction& errFct, Array<double>& in, Array<double>& target, size_t validatonSize);

public:
	Trainer(const std::string& myDbPath, Model& myModel) :
		pDatabase(new Kompex::SQLiteDatabase(myDbPath, SQLITE_OPEN_READONLY, 0)), pStmt(new Kompex::SQLiteStatement(pDatabase)), model(myModel) {
/*		query = std::string("SELECT \
			m.id AS id, \
			m.ts AS ts, \
			d1.value AS FeatureA, \
			d2.value AS FeatureB, \
			m.copyMethod AS method \
				FROM measurement m \
				JOIN data d1 ON m.id=d1.mid AND d1.fid=1 \
				JOIN data d2 ON m.id=d2.mid AND d2.fid=2 ");
*/

	}

	~Trainer() {
		delete pStmt;
// FIXME find a way to avoid stack corruption in certain cases (-f2 -f1)
//		delete pDatabase;
	}

	double train(Optimizer& optimizer, ErrorFunction& errFct, size_t iterations);

	void setFeaturesByIndex(const std::vector<std::string>& featureIndices);
	void setFeatureByIndex(const std::string featureIndex);

	void setFeaturesByName(const std::vector<std::string>& featureNames);
	void setFeatureByName(const std::string featureName);
};

} // end namespace ml