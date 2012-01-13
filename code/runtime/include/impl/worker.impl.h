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

#include "worker.h"

#include <stdlib.h>

#include "globals.h"
#include "impl/irt_mqueue.impl.h"
#include "impl/irt_context.impl.h"
#include "impl/work_item.impl.h"
#include "utils/impl/minlwt.impl.h"
#include "utils/affinity.h"
#include "impl/error_handling.impl.h"
#include "impl/instrumentation.impl.h"

#ifdef IRT_VERBOSE
void _irt_worker_print_debug_info(irt_worker* self) {
	IRT_INFO("======== Worker %p debug info:\n", self);
#ifdef USING_MINLWT	
	IRT_INFO("== Base ptr: %p\n", (void*)self->basestack); // casting to void* would break 32 bit compatibility
#else
	IRT_INFO("== Base ptr: %p\n", &(self->basestack)); // casting to void* would break 32 bit compatibility
#endif
	IRT_INFO("== Current wi: %p\n", self->cur_wi);
	//IRT_INFO("==== Pool:\n");
	//irt_work_item* next_wi = self->pool.start;
	//while(next_wi != NULL) {
	//	IRT_INFO("--- Work item %p:\n", (void*)next_wi);
	//	IRT_INFO("- stack ptr: %p\n", (void*)next_wi->stack_ptr);
	//	IRT_INFO("- start ptr: %p\n", (void*)next_wi->stack_start);
	//	next_wi = next_wi->work_deque_next;
	//}
	//IRT_INFO("==== Queue:\n");
	//next_wi = self->queue.start;
	//while(next_wi != NULL) {
	//	IRT_INFO("--- Work item %p:\n", (void*)next_wi);
	//	IRT_INFO("- stack ptr: %p\n", (void*)next_wi->stack_ptr);
	//	IRT_INFO("- start ptr: %p\n", (void*)next_wi->stack_start);
	//	next_wi = next_wi->work_deque_next;
	//}
	IRT_INFO("========\n");
}
#endif

typedef struct __irt_worker_func_arg {
	irt_worker *generated;
	volatile bool ready;
	irt_affinity_mask affinity;
	uint16 index;
} _irt_worker_func_arg;

void* _irt_worker_func(void *argvp) {
	_irt_worker_func_arg *arg = (_irt_worker_func_arg*)argvp;
	irt_set_affinity(arg->affinity, pthread_self());
	arg->generated = (irt_worker*)calloc(1, sizeof(irt_worker));
	irt_worker* self = arg->generated;
	self->pthread = pthread_self();
	self->id.value.components.index = 1;
	self->id.value.components.thread = arg->index;
	self->id.value.components.node = 0; // TODO correct node id
	self->id.cached = self;
	self->generator_id = self->id.value.full;
	self->affinity = arg->affinity;
	self->cur_context = irt_context_null_id();
	self->cur_wi = NULL;
#ifdef IRT_ENABLE_INSTRUMENTATION
	self->performance_data = irt_create_performance_table(IRT_WORKER_PD_BLOCKSIZE);
	self->extended_performance_data = irt_create_extended_performance_table(IRT_WORKER_PD_BLOCKSIZE);
#else
	self->performance_data = 0;
	self->extended_performance_data = 0;
#endif
#ifdef IRT_OCL_INSTR
	self->event_data = irt_ocl_create_event_table();
#endif
	self->state = IRT_WORKER_STATE_CREATED;
	irt_worker_instrumentation_event(self, WORKER_CREATED, self->id);
	irt_scheduling_init_worker(self);
	IRT_ASSERT(pthread_setspecific(irt_g_worker_key, arg->generated) == 0, IRT_ERR_INTERNAL, "Could not set worker threadprivate data");
	// init lazy wi
	memset(&self->lazy_wi, 0, sizeof(irt_work_item));
	self->lazy_wi.id.cached = &self->lazy_wi;
	self->lazy_wi.state = IRT_WI_STATE_DONE;
	self->lazy_count = 0;
	// init reuse lists
	self->wi_ev_register_list = NULL; // prepare some?
	self->wg_ev_register_list = NULL; // prepare some?
	self->wi_reuse_stack = NULL; // prepare some?
	self->stack_reuse_stack = NULL;

	arg->ready = true;

	while(!self->state == IRT_WORKER_STATE_START) { pthread_yield(); } // MARK busy wait
	irt_worker_instrumentation_event(self, WORKER_RUNNING, self->id);
	self->state = IRT_WORKER_STATE_RUNNING;
	irt_scheduling_loop(self);

	return NULL;
}

#ifdef USING_MINLWT
// required for minlwt in gcc
__attribute__((optimize("-fno-optimize-sibling-calls")))
#endif
void _irt_worker_switch_to_wi(irt_worker* self, irt_work_item *wi) {
	IRT_ASSERT(self->cur_wi == NULL, IRT_ERR_INTERNAL, "Worker %p _irt_worker_switch_to_wi with non-null current WI", self);
	self->cur_context = wi->context_id;
	if(wi->state == IRT_WI_STATE_NEW) {
		// start WI from scratch
		wi->state = IRT_WI_STATE_STARTED;
		lwt_prepare(self->id.value.components.thread, wi, &self->basestack);

		self->cur_wi = wi;
#ifdef USING_MINLWT
		IRT_DEBUG("Worker %p _irt_worker_switch_to_wi - 1A, new stack ptr: %p.", self, (void*)wi->stack_ptr);
#else
		IRT_DEBUG("Worker %p _irt_worker_switch_to_wi - 1A, new stack ptr: %p.", self, &(wi->stack_ptr));
#endif
		IRT_VERBOSE_ONLY(_irt_worker_print_debug_info(self));
		irt_wi_instrumentation_event(self, WORK_ITEM_STARTED, wi->id);
		lwt_start(wi, &self->basestack, (irt_context_table_lookup(self->cur_context)->impl_table[wi->impl_id].variants[0].implementation));
		IRT_DEBUG("Worker %p _irt_worker_switch_to_wi - 1B.", self);
		IRT_VERBOSE_ONLY(_irt_worker_print_debug_info(self));
	} else { 
		// resume WI
		self->cur_wi = wi;
#ifdef USING_MINLWT
		IRT_DEBUG("Worker %p _irt_worker_switch_to_wi - 2A, new stack ptr: %p.", self, (void*)wi->stack_ptr);
#else
		IRT_DEBUG("Worker %p _irt_worker_switch_to_wi - 2A, new stack ptr: %p.", self, &(wi->stack_ptr));
#endif
		IRT_VERBOSE_ONLY(_irt_worker_print_debug_info(self));
		irt_wi_instrumentation_event(self, WORK_ITEM_RESUMED, wi->id);
		lwt_continue(&wi->stack_ptr, &self->basestack);
		IRT_DEBUG("Worker %p _irt_worker_switch_to_wi - 2B.", self);
		IRT_VERBOSE_ONLY(_irt_worker_print_debug_info(self));
	}
}

void _irt_worker_run_optional_wi(irt_worker* self, irt_work_item *wi) {
	irt_work_item *cur_wi = self->cur_wi;
	// store current wi data
	irt_lw_data_item* params = cur_wi->parameters;
	irt_work_item_range range = cur_wi->range;
	irt_wi_implementation_id impl_id = cur_wi->impl_id;
	// set new wi data
	cur_wi->parameters = wi->parameters;
	cur_wi->range = wi->range;
	cur_wi->impl_id = wi->impl_id;
	// call wi
	self->lazy_count++;
	(irt_context_table_lookup(self->cur_context)->impl_table[wi->impl_id].variants[0].implementation)(cur_wi);
	// restore data
	cur_wi->parameters = params;
	cur_wi->range = range;
	cur_wi->impl_id = impl_id;
}

irt_worker* irt_worker_create(uint16 index, irt_affinity_mask affinity) {
	_irt_worker_func_arg arg;
	arg.affinity = affinity;
	arg.index = index;
	arg.ready = false;

	pthread_t thread;

	IRT_ASSERT(pthread_create(&thread, NULL, &_irt_worker_func, &arg) == 0, IRT_ERR_INTERNAL, "Could not create worker thread");

	while(!arg.ready) { } // MARK busy wait

	return arg.generated;
}

void _irt_worker_cancel_all_others() {
	irt_worker *self = irt_worker_get_current(), *cur;
	for(uint32 i=0; i<irt_g_worker_count; ++i) {
		cur = irt_g_workers[i];
		if(cur != self && cur->state == IRT_WORKER_STATE_RUNNING) {
			cur->state = IRT_WORKER_STATE_STOP;
			irt_worker_instrumentation_event(self, WORKER_STOP, cur->id);
			pthread_cancel(cur->pthread);
		}
	}
	
}
