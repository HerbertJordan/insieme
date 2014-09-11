/**
 * Copyright (c) 2002-2014 Distributed and Parallel Systems Group,
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

#include "insieme/analysis/cba/framework/analysis_type.h"
#include "insieme/analysis/cba/framework/entities/job.h"
#include "insieme/analysis/cba/framework/generator/basic_data_flow.h"

#include "insieme/core/forward_decls.h"
#include "insieme/core/analysis/ir_utils.h"

namespace insieme {
namespace analysis {
namespace cba {

	// ----------------- jobs ---------------

	template<typename Context> class JobConstraintGenerator;

	struct job_analysis_data : public dependent_data_analysis<Job, JobConstraintGenerator> {};
	struct job_analysis_var  : public dependent_data_analysis<Job, JobConstraintGenerator> {};

	extern const job_analysis_data Jobs;
	extern const job_analysis_var  jobs;


	template<typename Context>
	class JobConstraintGenerator : public DataFlowConstraintGenerator<job_analysis_data, job_analysis_var, Context> {

		typedef DataFlowConstraintGenerator<job_analysis_data, job_analysis_var, Context> super;

		CBA& cba;

	public:

		JobConstraintGenerator(CBA& cba) : super(cba, Jobs, jobs), cba(cba) { };

		using super::elem;

		void visitJobExpr(const JobExprInstance& job, const Context& ctxt, Constraints& constraints) {

			// this expression is creating a job
			auto value = getJobFromConstructor(job, ctxt);
			auto J_res = cba.getVar(Jobs, job, ctxt);

			// add constraint fixing this job
			constraints.add(elem(value, J_res));

		}

	};

} // end namespace cba
} // end namespace analysis
} // end namespace insieme
