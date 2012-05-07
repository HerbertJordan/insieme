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

#include "ReClaM/Svm.h"
#include "ReClaM/MeanSquaredError.h"

#include "insieme/utils/logging.h"
#include "insieme/machine_learning/myModel.h"

//#include "insieme/utils/string_utils.h"
#include "insieme/machine_learning/cmd_line_utils.h"
#include "insieme/machine_learning/trainer.h"

using namespace insieme::ml;

void writeModel(Trainer* trainer) {
	if(TrainCmdOptions::OutputPath.size() == 0)
		TrainCmdOptions::OutputPath = ".";

	if(TrainCmdOptions::OutputModel.size() == 0){
		std::stringstream str;
		str << "model_" << + trainer->getModel().getInputDimension() << "_" << trainer->getModel().getOutputDimension();
		TrainCmdOptions::OutputModel = str.str();
	}

	trainer->saveModel(TrainCmdOptions::OutputModel, TrainCmdOptions::OutputPath);
}

size_t numberOfFeatures() {
	return TrainCmdOptions::SFeatureNames.size() + TrainCmdOptions::SFeatures.size()
		 + TrainCmdOptions::DFeatureNames.size() + TrainCmdOptions::DFeatures.size()
		 + TrainCmdOptions::PFeatureNames.size() + TrainCmdOptions::PFeatures.size();
}

typedef std::shared_ptr<Optimizer> OptimizerPtr;

int main(int argc, char* argv[]) {
//TODO add flag for output class genreation, at the moment only keepInt is needed

	TrainCmdOptions::Parse(argc, argv);

	const std::string dbPath(TrainCmdOptions::DataBase != std::string() ? TrainCmdOptions::DataBase : std::string("linear.db"));
	Logger::get(std::cerr, ((insieme::utils::log::Level)TrainCmdOptions::Verbosity));

	// Create a connection matrix with 2 inputs, 1 output
	// and a single, fully connected hidden layer with
	// 8 neurons:
	Array<int> con;
	size_t nIn = numberOfFeatures(), nOut = TrainCmdOptions::NumClasses;

	if(nIn == 0) {
		LOG(ERROR) << "No features set. Use -h to see help";
		return -1;
	}

	RBFKernel kernel(1.0);

	// declare Machine
	MyC_SVM svm(&kernel, 100.0, 100.0);
	SVM_Optimizer optimizer;
	MeanSquaredError err;

	Trainer* qpnn;
	// create trainer
	try {
		// TODO remove hard coding of keep int
		qpnn = new Trainer(dbPath, svm, GenNNoutput::ML_KEEP_INT);
	} catch(Kompex::SQLiteException& sle) {
		LOG(ERROR) << "Cannot create trainer: \n";
		sle.Show();
	}

	if(TrainCmdOptions::SFeatureNames.size() > 0)
		qpnn->setStaticFeaturesByName(TrainCmdOptions::SFeatureNames);
	if(TrainCmdOptions::SFeatures.size() > 0)
		qpnn->setStaticFeaturesByIndex(TrainCmdOptions::SFeatures);

	if(TrainCmdOptions::DFeatureNames.size() > 0)
		qpnn->setDynamicFeaturesByName(TrainCmdOptions::DFeatureNames);
	if(TrainCmdOptions::DFeatures.size() > 0)
		qpnn->setDynamicFeaturesByIndex(TrainCmdOptions::DFeatures);

	if(TrainCmdOptions::PFeatureNames.size() > 0)
		qpnn->setPcaFeaturesByName(TrainCmdOptions::PFeatureNames);
	if(TrainCmdOptions::PFeatures.size() > 0)
		qpnn->setPcaFeaturesByIndex(TrainCmdOptions::PFeatures);

	if(TrainCmdOptions::ExcludeCids.size() > 0)
		qpnn->setExcludeCodes(TrainCmdOptions::ExcludeCids);

	if(TrainCmdOptions::FilterCids.size() > 0)
		qpnn->setFilterCodes(TrainCmdOptions::FilterCids);

	if(TrainCmdOptions::TargetName.size() == 0) {
		LOG(ERROR) << "No target set. Use -t to set the desired target";
		delete qpnn;
		return -1;
	}
	qpnn->setTargetByName(TrainCmdOptions::TargetName);


	LOG(INFO)<< "Error: " << qpnn->train(optimizer, err, 0) << std::endl;

	if(TrainCmdOptions::OutputModel.size() > 0 || TrainCmdOptions::OutputPath.size() > 0)
		writeModel(qpnn);

	return 0;
}
