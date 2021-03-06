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
#include <cassert>

#include "ReClaM/PCA.h"
#include "ReClaM/LinearModel.h"

#include "KompexSQLitePrerequisites.h"
#include "KompexSQLiteDatabase.h"
#include "KompexSQLiteStatement.h"
#include "KompexSQLiteException.h"

namespace insieme {
namespace ml {

/*
 * This class is designed to read a set of features from a database and generate a certain number of principal components from them
 * The principal components will be written back to the database
 */

class PcaExtractor {

	/*
	 * checks if an entry with id already exists in table tableName
	 * @param id the id of the entry to be checked
	 * @param featrueName the name of the entry to be checked
	 * @return true if id can be found in tableName, false otherwise
	 */
	bool alreadyThere(const int64_t id, const std::string& featureName, const std::string& tableName);

	/*
	 * writes the principal components in pcs to the database
	 * @param pcs an Array containing the principal components
	 * @param ids the ids of the patterns
	 */
	void writeToDatabase(Array<double>& pcs, Array<int64>& ids, const std::string& nameTbl, const std::string& dataTbl, bool checkBeforeInsert = true)
		throw(Kompex::SQLiteException);

protected:
	Kompex::SQLiteDatabase *pDatabase;
	std::string dbPath;
	Kompex::SQLiteStatement *pStmt;
	std::string mangling;

	std::vector<std::string> staticFeatures, dynamicFeatures;
	std::string query;

	size_t sizeIn, sizeOut;

//	Array<double> featureNormalization;
//	std::ostream& out;

	/*
	 * writes the principal components in pcs to the code/static_features table in the database
	 * @param pcs an Array containing the principal components
	 * @param ids the ids of the patterns
	 */
	void writeToCode(Array<double>& pcs, Array<int64>& ids, bool checkBeforeInsert = true) throw(MachineLearningException);

	/*
	 * writes the principal components in pcs to the setup/dynnamic_features table in the database
	 * @param pcs an Array containing the principal components
	 * @param ids the ids of the patterns
	 */
	void writeToSetup(Array<double>& pcs, Array<int64>& ids, bool checkBeforeInsert = true) throw(MachineLearningException);

	/*
	 * writes the principal components in pcs to the pca/pc_features table in the database
	 * @param pcs an Array containing the principal components
	 * @param ids the ids of the patterns
	 */
	void writeToPca(Array<double>& pcs, Array<int64>& ids, bool checkBeforeInsert = true) throw(MachineLearningException);

	/*
	 * applies the passed query on the given database and stores the read data in in
	 * @param in an Array to store the data read from the database in a 2D-way (patterns x features)
	 * @param ids an Array to store the ids of the patterns
	 * @param nFeatures the number of features to be read from the database
	 * @param the query to be used
	 * @param nIds the number of ids that should be stored in ids (expanding its second dimension). This number must be appended after the features by the query
	 * @return the number of patterns read from the database
	 */
	size_t readDatabase(Array<double>& in, Array<int64>& ids, size_t nFeatures, std::string query, size_t nIds = 1) throw(ml::MachineLearningException);

	/*
	 * applies the query from the field query on the given database and stores the read data in in
	 * @param in an Array to store the data read from the database in a 2D-way (patterns x features)
	 * @param ids an Array to store the ids of the patterns
	 * @param nIds the number of ids that should be stored in ids (expanding its second dimension). This number must be appended after the features by the query
	 * @return the number of patterns read from the database
	 */
	size_t readDatabase(Array<double>& in, Array<int64>& ids, size_t nFeatures, size_t nIds = 1) throw(ml::MachineLearningException) {
		return readDatabase(in, ids, nFeatures, query, nIds);
	}

	/*
	 * generates a model of type AffineLinearMap that is initialized with the feature's eigenvectors and
	 * can be used to generate the PCs
	 * @param model the model which will be initialized to generate PCs
	 * @param data the data in 2D shape (patterns x features) to initialize the model
	 * @param eignevalues will be overwritten with the eignevalues of the data
	 * @param eigenvectors will be overwritten with the ?eigenvectors? of data
	 */
	void genPCAmodel(AffineLinearMap& model, Array<double>& data, Array<double>& eigenvalues, Array<double>& eigenvectors);

	/*
	 * generates a model of type AffineLinearMap that is initialized with the feature's eigenvectors and
	 * can be used to generate the PCs
	 * @param model the model which will be initialized to generate PCs
	 * @param data the data in 2D shape (patterns x features) to initialize the model
	 * @param eignevalues will be overwritten with the eignevalues of the data
	 * @param eigenvectors the ?eigenvectors? of data
	 */
	void genPCAmodel(AffineLinearMap& model, Array<double>& data, Array<double>& eigenvalues) {
		Array<double> trans;
		genPCAmodel(model, data, eigenvalues, trans);
	}

	/*
	 * generates a model of type AffineLinearMap that is initialized with the feature's eigenvectors and
	 * can be used to generate the PCs
	 * @param model the model which will be initialized to generate PCs
	 * @param data the data in 2D shape (patterns x features) to initialize the model
	 */
	void genPCAmodel(AffineLinearMap& model, Array<double>& data) {
		Array<double> eigenvectors;
		genPCAmodel(model, data, eigenvectors);
	}

	/*
	 * calculates the principal components of the 2D shaped (patterns x features) data using a model
	 * which has been previously initialized with PcaExtractor::genPCAmodel
	 * @param reductionModel an AffineLinearMap initilaized with PcaExtractor::genPCAmodel
	 * @param data the data in 2D shape (patterns x features) to etract the PCs from
	 * @param manglingPostfix a postfix that will be added to every feature name in order to distinguish this PCs from others
	 * @return a 2D Array (patterns x features) containing the PCs of data. The number of PCs is determined by the model
	 */
	Array<double> genPCs(AffineLinearMap& model, Array<double>& data);

public:
	/*
	 * Constructs a class to generate principal components form features in a database
	 * @param myDbPath the path to the SQLite database file
	 * @param nInFeatures the number of features to be analyzed/combined
	 * @param nOutFeatures the number to which the features should be reduced
	 */
	PcaExtractor(const std::string& myDbPath, const std::string& manglingPostfix = "");

	/*
	 * constructor specifying the variance (in %) which should be covered by the PCs. The program then
	 * writes as many PCs which are needed to cover the specified variance on the dataset
	 * @param myDbPath the path to the database to read from and write the PCs to
	 * @param the percentage of variance that should be covered by the PCs
	 * @param manglingPostfix a postfix that will be added to every feature name in order to distinguish this PCs from others
	 */
	PcaExtractor(const std::string& myDbPath, double toBeCovered, const std::string& manglingPostfix = "");

	~PcaExtractor();

	/*
	 * generates the default query, querying for all patterns which have all features set
	 * using the current values for the features
	 */
	virtual void genDefaultQuery() =0;

	/*
	 * calculates the principal components of static features based on the given query and stores them in the database
	 * @param toBeCovered the percentage of variance that should be covered by the PCs
	 * @return the number of PCs generated
	 */
	virtual size_t calcPca(double toBeCovered) =0;

	/*
	 * calculates the principal components of static features based on the given query and stores them in the database
	 * @param nOutFeatures1 the number to which the features should be reduced
	 * @param nOutFeatures2 used in some sublcasses, e.g. to distingush between numbers of features in dynamic and static features
	 * @return the percentabe of the variance covered by the first nOutFeatures PCs
	 */
	virtual double calcPca(size_t nOuFeatures1, size_t nOutFeatures2) =0;

	/**
	 * adds a vector of static features indices to the internal feature vector
	 * @param featureIndices a vector holding the column (in the database) indices of some static features
	 */
	void setStaticFeaturesByIndex(const std::vector<std::string>& featureIndices);
	/**
	 * adds one feature index to the internal static feature vector
	 * @param featureIndex the index of the column (in the database) holding a feature
	 */
	void setStaticFeatureByIndex(const std::string featureIndex);

	/**
	 * adds a vector of static features to the internal feature vector by name
	 * @param featureNames a vector holding the name (in the database) of some static features
	 */
	void setStaticFeaturesByName(const std::vector<std::string>& featureNames);
	/**
	 * adds one feature to the internal static feature vector by name
	 * @param featureName the name of a feature (in the database)
	 */
	void setStaticFeatureByName(const std::string featureName);

	/**
	 * adds a vector of dynamic features indices to the internal feature vector
	 * @param featureIndices a vector holding the column (in the database) indices of some dynamic features
	 */
	void setDynamicFeaturesByIndex(const std::vector<std::string>& featureIndices);
	/**
	 * adds one feature index to the internal dynamic feature vector
	 * @param featureIndex the index of the column (in the database) holding a feature
	 */
	void setDynamicFeatureByIndex(const std::string featureIndex);

	/**
	 * adds a vector of dynamic features to the internal feature vector by name
	 * @param featureNames a vector holding the name (in the database) of some dynamic features
	 */
	void setDynamicFeaturesByName(const std::vector<std::string>& featureNames);
	/**
	 * adds one feature to the internal dynamic feature vector by name
	 * @param featureName the name of a feature (in the database)
	 */
	void setDynamicFeatureByName(const std::string featureName);

	/*
	 * sets the query for data
	 * @param customQuery the query as a string
	 */
	void setQuery(const std::string customQuery) { query = customQuery; }

	/**
	 * returns the number of all (static + dynamic) features
	 * @return the number of features
	 */
	size_t nFeatures() { return staticFeatures.size() + dynamicFeatures.size(); }

	/**
	 * resets the feature arrays as well as the stored query
	 */
	void restetFeatures();

};


} // end namespace ml
} // end namespace insieme
