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

#include "declarations.h"

#include <pthread.h>

#include "work_item.h"
#include "irt_scheduling.h"
#include "utils/minlwt.h"

/* ------------------------------ data structures ----- */

IRT_MAKE_ID_TYPE(worker);

typedef enum _irt_worker_state {
	IRT_WORKER_STATE_CREATED, IRT_WORKER_STATE_START, IRT_WORKER_STATE_RUNNING, IRT_WORKER_STATE_STOP
} irt_worker_state;

struct _irt_worker {
	irt_worker_id id;
	uint64 generator_id;
	irt_affinity_mask affinity;
	pthread_t pthread;
	minlwt_context basestack;
	irt_context_id cur_context;
	irt_work_item* cur_wi;
	irt_worker_state state; // used to ensure all workers start at the same time
	irt_worker_scheduling_data sched_data;
};

/* ------------------------------ operations ----- */

static inline irt_worker* irt_worker_get_current() {
	return (irt_worker*)pthread_getspecific(irt_g_worker_key);
}

irt_worker* irt_worker_create(uint16 index, irt_affinity_mask affinity);

void _irt_worker_switch_to_wi(irt_worker* self, irt_work_item *wi);
