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

#include "work_group.h"

#include "impl/work_item.impl.h"
#include "irt_atomic.h"


static inline irt_work_group* _irt_wg_new() {
	return (irt_work_group*)malloc(sizeof(irt_work_group));
}
static inline void _irt_wg_recycle(irt_work_group* wg) {
	free(wg);
}

irt_work_group* irt_wg_create() {
	irt_work_group* wg = _irt_wg_new();
	wg->id = irt_generate_work_group_id(IRT_LOOKUP_GENERATOR_ID_PTR);
	wg->id.cached = wg;
	wg->distributed = false;
	wg->local_member_count = 0;
	wg->cur_barrier_count_up = 0;
	wg->cur_barrier_count_down = 0;
	return wg;
}
void irt_wg_destroy(irt_work_group* wg) {
	_irt_wg_recycle(wg);
}

//static inline void irt_wg_join(irt_work_group* wg) {
//	irt_wg_insert(wg, irt_wi_get_current());
//}
//static inline void irt_wg_leave(irt_work_group* wg) {
//	irt_wg_remove(wg, irt_wi_get_current());
//}

void irt_wg_insert(irt_work_group* wg, irt_work_item* wi) {
	// Todo distributed
	irt_atomic_inc(&wg->local_member_count);
	if(wi->work_groups == NULL) _irt_wi_allocate_wgs(wi);
	wi->work_groups[wi->num_groups++] = wg->id;
}
void irt_wg_remove(irt_work_group* wg, irt_work_item* wi) {
	// Todo distributed
	irt_atomic_dec(&wg->local_member_count);
	// cleaning up group membership in wi is not necessary, wis may only be removed from groups when they end
}

bool _irt_wg_barrier_check_down(irt_work_item* wi) {
	irt_work_group *wg = (irt_work_group*)wi->ready_check.data;
	return wg->cur_barrier_count_down == 0;
}

bool _irt_wg_barrier_check(irt_work_item* wi) {
	irt_work_group *wg = (irt_work_group*)wi->ready_check.data;
	return wg->cur_barrier_count_down != 0;
}

void irt_wg_barrier(irt_work_group* wg) {
	// Todo distributed
	// check if barrier down count is 0, otherwise wait for it to be
	if(wg->cur_barrier_count_down != 0) {
		irt_worker* self = irt_worker_get_current();
		irt_work_item* swi = self->cur_wi;
		swi->ready_check.fun = &_irt_wg_barrier_check_down;
		swi->ready_check.data = wg;
		irt_scheduling_yield(self, swi);
	}
	// enter barrier
	if(irt_atomic_add_and_fetch(&wg->cur_barrier_count_up, 1) < wg->local_member_count) {
		irt_worker* self = irt_worker_get_current();
		irt_work_item* swi = self->cur_wi;
		swi->ready_check.fun = &_irt_wg_barrier_check;
		swi->ready_check.data = wg;
		irt_scheduling_yield(self, swi);
		irt_atomic_dec(&wg->cur_barrier_count_down);
	} else {
		// last wi to reach barrier, set down count
		wg->cur_barrier_count_up = 0;
		wg->cur_barrier_count_down = wg->local_member_count-1;
	}
}

void irt_wg_distribute(irt_work_group* wg, irt_distribute_id dist /*, ???*/);
