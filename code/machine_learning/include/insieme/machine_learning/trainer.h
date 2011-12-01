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

#include "KompexSQLitePrerequisites.h"
#include "KompexSQLiteDatabase.h"
#include "KompexSQLiteStatement.h"
#include "KompexSQLiteException.h"
//#include "KompexSQLiteStreamRedirection.h"
//#include "KompexSQLiteBlob.h"

#include "Array/Array.h"
#include "ReClaM/FFNet.h"

namespace insieme {
namespace ml {

#define POS  1
#define NEG 0

// enums defining how the measurement values should be mapped to the ml-algorithms output
enum GenNNoutput {
	ML_KEEP_INT,
	ML_MAP_FLOAT_LIN,
	ML_MAP_FLOAT_LOG,
	ML_MAP_FLOAT_HYBRID,
	ML_MAP_TO_N_CLASSES,
	ML_COMPARE_BINARY,
	size
};

namespace {
/*
 * calculates the index of the biggest element in an Array
 * @param
 * arr The array
 * @return
 * the index of the biggest element in the array arr
 */
template<typename T>
size_t arrayMaxIdx(const Array<T>& arr) {
	size_t idx = 0, cnt = 0;
	T currMax = 0;

	for(typename Array<T>::const_iterator I = arr.begin(); I != arr.end(); ++I) {
		if(currMax < *I)
			idx = cnt;
		++cnt;
	}
	return idx;
}
}// end anonymous namespace

class MachineLearningException : public std::exception {
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
	enum GenNNoutput genOut;
protected:
	Kompex::SQLiteDatabase *pDatabase;
	Kompex::SQLiteStatement *pStmt;

	std::vector<std::string> features;
	std::string trainForName, query;

	Model& model;
	Array<double> featureNormalization;

private:
	/*
	 * Queries the maximum value for the given parameter in table measurements for the used features
	 * @param
	 * param the name of the column to query for
	 * @return
	 * the maximum of the queried column with the current features set
	 */
	double getMaximum(const std::string& param);

	/*
	 * Queries the minimum value for the given parameter in table measurements for the used features
	 * @param
	 * param the name of the column to query for
	 * @return
	 * the minimum of the queried column with the current features set
	 */
	double getMinimum(const std::string& param);

	/*
	 * Converts the value read from the database to an index in one of n coding, according to the policy defined in the variable genOut.
	 * The returned should be set to POS, the rest to NEG
	 * @param
	 * stmt the SQLiteStatement with a prepared query to read the value from position index
	 * index the index of the value to be trained in the database joint table
	 * max the maximum of the values in the columns. Will be ignored if genOut is set to ML_KEEP_INT
	 * @return
	 * the index for the one of n coding of the current query
	 */
	size_t valToOneOfN(Kompex::SQLiteStatement* stmt, size_t index, double max, double min);


    /*
     * puts the first numPatterns/n patterns in the first class, the second in the second etc
     * @param
     * measurements a vector containing a pair of one double, representing the measured value, and a size_t, holding the index of the measurement
     * n the number of classes to map to
     * neg the lower target value
     * pos the upper target value
     * target an empty array which will be filled with the one-of-n coding for all patterns
     */
    void mapToNClasses(std::list<std::pair<double, size_t> >& measurements, size_t n, double neg, double pos, Array<double>& target);

protected:
	/*
	 * Returns the index of the maximum of all elements in coded
	 * @param
	 * coded An array with a one-of-n coding
	 * @return
	 * the 'the one'
	 */
	size_t oneOfNtoIdx(Array<double> coded);

	double earlyStopping(Optimizer& optimizer, ErrorFunction& errFct, Array<double>& in, Array<double>& target, size_t validatonSize);

	/*
	 * Splits the training dataset in two pieces of validationSieze% for validaton and 100-validatonSize% for validation
	 * @param
	 * errFct the Error function to be use
	 * in the input to the model
	 * targed the desired outputs for the given inputs
	 * validationSize the size of the validation size in percent
	 * nBatches the number of batches to train at once to be generated out of the entire training dataset
	 * @return
	 * the current error on the validation
	 */
	double myEarlyStopping(Optimizer& optimizer, ErrorFunction& errFct, Array<double>& in, Array<double>& target, size_t validationSize, size_t nBatches = 5);

	/*
	 * Reads an entry for the training values form the database and appends it to the Array target in one-of-n coding
	 * @param
	 * target an Array where the target values should be written to
	 * stmt An sql statement holdin the line with de desired target value
	 * queryIdx the index of the desired target value in the sql statement
	 * max the maximum value of all targets (needed for one-to-n coding)
	 * min the minimum value of all targets (needed for one-to-n coding)
	 * oneOfN an Array of size number-of-classes containing only NEG. The Array will be reset during the call, only needed for performance reasons
	 */
	virtual void appendToTrainArray(Array<double>& target, Kompex::SQLiteStatement* stmt, size_t queryIdx, double max, double min, Array<double>& oneOfN);

	/*
	 * Reads values form the database and stores the features in in, the targets (mapped according to the set policy) in targets as one-of-n coding
	 * @param
	 * in An empty Array which will hold the features read form the database
	 * target An empty Array which will hold the target values as one-of-n codung
	 * @ return
	 * the number of training patterns
	 */
	size_t readDatabase(Array<double>& in, Array<double>& target) throw(Kompex::SQLiteException);
public:
	Trainer(const std::string& myDbPath, Model& myModel, enum GenNNoutput genOutput = ML_MAP_TO_N_CLASSES) :
		genOut(genOutput), pDatabase(new Kompex::SQLiteDatabase(myDbPath, SQLITE_OPEN_READONLY, 0)), pStmt(new Kompex::SQLiteStatement(pDatabase)),
		trainForName("time"), model(myModel) {
/*		query = std::string("SELECT \
			m.id AS id, \
			m.ts AS ts, \
			d1.value AS FeatureA, \
			d2.value AS FeatureB, \getMaximum
			m.copyMethod AS method \
				FROM measurement m \
				JOIN data d1 ON m.id=d1.mid AND d1.fid=1 \
				JOIN data d2 ON m.id=d2.mid AND d2.fid=2 ");
*/

	}

	~Trainer() {
		delete pStmt;
		pDatabase->Close();
// FIXME find a way to avoid stack corruption in certain cases (-f2 -f1)
		delete pDatabase;
	}

	/*
	 * trains the model using the patterns returned by the given query or the default query if none is given
	 * @param
	 * the Shark optimizer to be used, eg. Quickprop, Bfgs etc.
	 * errFct the Shark error function to be used, eg. MeanSquaredError,
	 * iterations the number of training operations to perform. If a number >0 is given, the trainer performs this
	 * number of training iterations on the whole dataset and returns the error on it. If 0 is passed, the trainer
	 * will use a customized early stopping approach:
	 * - splits data in training and validation set in ration 10:1 randomly
	 * - training is only done on training set
	 * - maximum of training iterations is set to 1000
	 * - stopping training earlier if there is no improvement for 5 iterations
	 * - the returned error is the one on the validation set only (the error on the training set and classification
	 *   error on the validation set is printed to LOG(INFO)
	 * @return
	 * the error after training
	 */
	virtual double train(Optimizer& optimizer, ErrorFunction& errFct, size_t iterations = 0) throw(MachineLearningException);

	/*
	 * Reads data form the database according to the current query, tests all patterns with the current model
	 * and returns the error according to the error function
	 * @param
	 * errFct the error function to be used
	 * @return
	 * the error calculated with the given error function
	 */
	virtual double evaluateDatabase(ErrorFunction& errFct) throw(MachineLearningException);

	/*
	 * Evaluates a pattern using the internal model.
	 * @param
	 * pattern An Array holding the features of the pattern to be evaluated
	 * @return
	 * the index of the winning class
	 */
	virtual size_t evaluate(Array<double>& pattern);

	/*
	 * Evaluates a pattern using the internal model
	 * WARNING size of pointer is not checked
	 * @param
	 * pattern A C pointer holding the features of the pattern to be evaluated
	 * @return
	 * the index of the winning class
	 */
	virtual size_t evaluate(const double* pattern);

	/*
	 * adds a vector of features indices to the internal feature vector
	 * @param
	 * featureIndices a vector holding the column (in the database) indices of some features
	 */
	void setFeaturesByIndex(const std::vector<std::string>& featureIndices);
	/*
	 * adds one feature index to the internal feature vector
	 * @param
	 * featureIndex the index of the column (in the database) holding a feature
	 */
	void setFeatureByIndex(const std::string featureIndex);

	/*
	 * adds a vector of features to the internal feature vector by name
	 * @param
	 * featureNames a vector holding the name (in the database) of some features
	 */
	void setFeaturesByName(const std::vector<std::string>& featureNames);
	/*
	 * adds one feature to the internal feature vector by name
	 * @param
	 * featureName the name of a feature (in the database)
	 */
	void setFeatureByName(const std::string featureName);

	/*
	 * sets the name of the column from which to read the target values form the database
	 * @param
	 * featureName the name of the column in the database which holds the training target
	 */
	void setTargetByName(const std::string& targetName){ trainForName = targetName; }

	/*
	 * generates the default query, querying for all patterns which have all features set and using the column
	 * targetName as target, using the current values for the features and targetName. If no query is set before
	 * the training is started, this function will be used to generate a query
	 */
	void genDefaultQuery();

	/*
	 * sets the query to a custom string. The query must return one line for each pattern of the following form
	 * [measurmentId, featue1, ..., featureN, measurement
	 * @param
	 * a string to be used as database query
	 */
	void setCustomQuery(std::string& customQuery) { query = customQuery; }

	/*
	 * return the query to be used
	 * @return
	 * the current value of the feeld query
	 */
	std::string& getQuery() { return query; }

	/*
	 * Gives back an array of size (3 x nFeatures) holding the average, the min and the max of all features
	 * @return
	 * The array holding the data needed for feature normalization
	 */
	Array<double> getFeatureNormalization() { return featureNormalization; }

	/*
	 * Generates two files:
	 * The model is stored in path/filename.mod
	 * The feature normalization data is stored in path/filename.fnd
	 * @param
	 * filename The name of the files to store the data. The appropriate file extension will be added automatically
	 * path The path were to store the two files, the current directory is the default
	 */
	void saveModel(const std::string filename, const std::string path = ".");
	/*
	 * Loads stored values from disk:
	 * The model is loaded from in path/filename.mod
	 * The feature normalization data is loaded from in path/filename.fnd
	 * The trainer must have been constructed with model of the same type/size as the stored one
	 * @param
	 * filename The name of the files to load the data from. The appropriate file extension will be added automatically
	 * path The path were to load the two files from, the current directory is the default
	 * @return
	 * The number of features for this model (not takeing into account doubling for binary compare trainers)
	 */
	size_t loadModel(const std::string filename, const std::string path = ".");
};

} // end namespace ml
} // end namespace insieme