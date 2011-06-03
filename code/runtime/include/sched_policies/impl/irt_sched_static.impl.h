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

#include "sched_policies/utils/impl/irt_sched_queue_pool_base.impl.h"
#include "sched_policies/utils/impl/irt_sched_ipc_base.impl.h"
#include "impl/worker.impl.h"

void irt_scheduling_init_worker(irt_worker* self) {
	irt_work_item_cdeque_init(&self->sched_data.queue);
	irt_work_item_deque_init(&self->sched_data.pool);
}

void irt_scheduling_loop(irt_worker* self) {
	while(self->state != IRT_WORKER_STATE_STOP) {
		// try to take a ready WI from the pool
		irt_work_item* next_wi = _irt_get_ready_wi_from_pool(&self->sched_data.pool);
		if(next_wi != NULL) {
			_irt_worker_switch_to_wi(self, next_wi);
			continue;
		}
		// if that failed, try to take a work item from the queue
		irt_work_item* new_wi = irt_work_item_cdeque_pop_front(&self->sched_data.queue);
		if(new_wi != NULL) {
			_irt_worker_switch_to_wi(self, new_wi);
			continue;
		}
		// if that failed as well, look in the IPC message queue
		_irt_sched_check_ipc_queue(self);
		pthread_yield();
	}
}

void irt_scheduling_assign_wi(irt_worker* target, irt_work_item* wi) {
	// split wis equally among workers
	if(irt_wi_range_get_size(&wi->range) >= irt_g_worker_count) {
		irt_work_item **split_wis = (irt_work_item**)alloca(irt_g_worker_count * sizeof(irt_work_item*));
		irt_wi_split_uniform(wi, irt_g_worker_count, split_wis);
		for(int i=0; i<irt_g_worker_count; ++i) {
			irt_work_item_cdeque_insert_back(&irt_g_workers[i]->sched_data.queue, split_wis[i]);
		}
	} else {
		irt_work_item_cdeque_insert_back(&target->sched_data.queue, wi);
	}
}

void irt_scheduling_yield(irt_worker* self, irt_work_item* yielding_wi) {
	IRT_DEBUG("Worker yield, worker: %p,  wi: %p", self, yielding_wi);
	irt_work_item_deque_insert_back(&self->sched_data.pool, yielding_wi);
	self->cur_wi = NULL;
	lwt_continue(&self->basestack, &yielding_wi->stack_ptr);
}