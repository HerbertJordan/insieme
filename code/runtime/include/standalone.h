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

#define IRT_NUM_WORKERS_ENV "IRT_NUM_WORKERS"

#include <pthread.h>

#include "client_app.h"
#include "irt_mqueue.h"
#include "instrumentation.h"
#include "utils/timing.h"
#include "irt_all_impls.h"
#ifdef IRT_ENABLE_ENERGY_INSTRUMENTATION
#include "../pmlib/CInterface.h"
#endif

/** Starts the runtime in standalone mode and executes work item impl_id.
  * Returns once that wi has finished.
  * worker_count : number of workers to start
  * init_context_fun : fills type tables in context
  * cleanup_context_fun : cleans up the context
  * impl_id : the id of the work-item implementation to be started
  * startup_params : parameter struct for the startup work item (impl_id)
  */
void irt_runtime_standalone(uint32 worker_count, init_context_fun* init_fun, cleanup_context_fun* cleanup_fun, irt_wi_implementation_id impl_id, irt_lw_data_item *startup_params);

// globals
pthread_key_t irt_g_error_key;
pthread_mutex_t irt_g_error_mutex;
pthread_key_t irt_g_worker_key;
mqd_t irt_g_message_queue;
uint32 irt_g_worker_count;
struct _irt_worker **irt_g_workers;
irt_runtime_behaviour_flags irt_g_runtime_behaviour;

IRT_CREATE_LOOKUP_TABLE(data_item, lookup_table_next, IRT_ID_HASH, IRT_DATA_ITEM_LT_BUCKETS);
IRT_CREATE_LOOKUP_TABLE(context, lookup_table_next, IRT_ID_HASH, IRT_CONTEXT_LT_BUCKETS);
IRT_CREATE_LOOKUP_TABLE(wi_event_register, lookup_table_next, IRT_ID_HASH, IRT_EVENT_LT_BUCKETS);
IRT_CREATE_LOOKUP_TABLE(wg_event_register, lookup_table_next, IRT_ID_HASH, IRT_EVENT_LT_BUCKETS);

void irt_init_globals() {
	// not using IRT_ASSERT since environment is not yet set up
	int err_flag = 0;
	err_flag |= pthread_key_create(&irt_g_error_key, NULL);
	err_flag |= pthread_key_create(&irt_g_worker_key, NULL);
	if(err_flag != 0) {
		fprintf(stderr, "Could not create pthread key(s). Aborting.\n");
		exit(-1);
	}
	pthread_mutex_init(&irt_g_error_mutex, NULL);
	if(irt_g_runtime_behaviour & IRT_RT_MQUEUE) irt_mqueue_init();
	irt_data_item_table_init();
	irt_context_table_init();
	irt_wi_event_register_table_init();
	irt_wg_event_register_table_init();
	irt_time_set_ticks_per_sec(); // sleeps for 100 ms, measures clock cycles, sets irt_g_time_ticks_per_sec
}
void irt_cleanup_globals() {
	if(irt_g_runtime_behaviour & IRT_RT_MQUEUE) irt_mqueue_cleanup();
	irt_data_item_table_cleanup();
	irt_context_table_cleanup();
	irt_wi_event_register_table_cleanup();
	irt_wg_event_register_table_cleanup();
	pthread_mutex_destroy(&irt_g_error_mutex);
	pthread_key_delete(irt_g_error_key);
	pthread_key_delete(irt_g_worker_key);
}

// exit handling
void irt_term_handler(int signal) {
	exit(0);
}
void irt_exit_handler() {
#ifdef USE_OPENCL
	irt_ocl_release_devices();	
#endif
	irt_cleanup_globals();
	for(int i = 0; i < irt_g_worker_count; ++i) {
		// TODO: add OpenCL events
#ifdef IRT_ENABLE_INSTRUMENTATION
		irt_instrumentation_output(irt_g_workers[i]); 
#endif

#ifdef IRT_ENABLE_REGION_INSTRUMENTATION
		irt_extended_instrumentation_output(irt_g_workers[i]);
#endif
	}
	PAPI_shutdown();
	free(irt_g_workers);
	//IRT_INFO("\nInsieme runtime exiting.\n");
}

// error handling
void irt_error_handler(int signal) {
	pthread_mutex_lock(&irt_g_error_mutex); // not unlocked
	_irt_worker_cancel_all_others();
	irt_error* error = (irt_error*)pthread_getspecific(irt_g_error_key);
	fprintf(stderr, "Insieme Runtime Error recieved (thread %p): %s\n", (void*)pthread_self(), irt_errcode_string(error->errcode));
	fprintf(stderr, "Additional information:\n");
	irt_print_error_info(stderr, error);
	exit(-error->errcode);
}

void irt_interrupt_handler(int signal) {
	// do nothing
}

void irt_runtime_start(irt_runtime_behaviour_flags behaviour, uint32 worker_count) {
	irt_g_runtime_behaviour = behaviour;

	// initialize error and termination signal handlers
	signal(IRT_SIG_ERR, &irt_error_handler);
	signal(IRT_SIG_INTERRUPT, &irt_interrupt_handler);
	signal(SIGTERM, &irt_term_handler);
	signal(SIGINT, &irt_term_handler);
	atexit(&irt_exit_handler);
	// initialize globals
	irt_init_globals();
#ifdef IRT_ENABLE_REGION_INSTRUMENTATION
#ifdef IRT_ENABLE_ENERGY_INSTRUMENTATION
	irt_instrumentation_init_energy_instrumentation();
#endif
	// initialize PAPI and check version
	irt_initialize_papi();
	irt_region_toggle_instrumentation(true);
#endif
#ifdef IRT_ENABLE_INSTRUMENTATION
	irt_all_toggle_instrumentation(false);
	irt_wi_toggle_instrumentation(true);
#endif

#ifdef USE_OPENCL
	IRT_INFO("Running Insieme runtime with OpenCL!\n");
	irt_ocl_init_devices();
#endif

	IRT_DEBUG("!!! Starting worker threads");
	irt_g_worker_count = worker_count;
	irt_g_workers = (irt_worker**)malloc(irt_g_worker_count * sizeof(irt_worker*));
	// initialize workers
	__uint128_t aff = 1;
	for(int i=0; i<irt_g_worker_count; ++i) {
		irt_g_workers[i] = irt_worker_create(i, aff<<i);
	}
	// start workers
	for(int i=0; i<irt_g_worker_count; ++i) {
		irt_g_workers[i]->state = IRT_WORKER_STATE_START;
	}
}

uint32 irt_get_default_worker_count() {
	if(getenv(IRT_NUM_WORKERS_ENV)) {
		return atoi(getenv(IRT_NUM_WORKERS_ENV));
	}
	return irt_get_num_cpus();
}

bool _irt_runtime_standalone_end_func(irt_wi_event_register* source_event_register, void *mutexp) {
	pthread_mutex_t* mutex = (pthread_mutex_t*)mutexp;
	pthread_mutex_unlock(mutex);
	return false;
}

void irt_runtime_standalone(uint32 worker_count, init_context_fun* init_fun, cleanup_context_fun* cleanup_fun, irt_wi_implementation_id impl_id, irt_lw_data_item *startup_params) {
	irt_runtime_start(IRT_RT_STANDALONE, worker_count);
	pthread_setspecific(irt_g_worker_key, irt_g_workers[0]); // slightly hacky
	irt_context* context = irt_context_create_standalone(init_fun, cleanup_fun);
	for(int i=0; i<irt_g_worker_count; ++i) {
		irt_g_workers[i]->cur_context = context->id;
	}
	irt_work_item* main_wi = irt_wi_create(irt_g_wi_range_one_elem, impl_id, startup_params);
	// create work group for outermost wi
	irt_work_group* outer_wg = irt_wg_create();
	irt_wg_insert(outer_wg, main_wi);
	// event handling for outer work item [[
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_lock(&mutex);
	irt_wi_event_lambda handler;
	handler.next = NULL;
	handler.data = &mutex;
	handler.func = &_irt_runtime_standalone_end_func;
	irt_wi_event_register* ev_reg = (irt_wi_event_register*)calloc(1, sizeof(irt_wi_event_register));
	pthread_spin_init(&ev_reg->lock, PTHREAD_PROCESS_PRIVATE);
	ev_reg->handler[IRT_WI_EV_COMPLETED] = &handler;
	ev_reg->id.value.full = main_wi->id.value.full;
	ev_reg->id.cached = ev_reg;
	_irt_wi_event_register_only(ev_reg);
	// ]] event handling
	irt_scheduling_assign_wi(irt_g_workers[0], main_wi);
	pthread_mutex_lock(&mutex);
	_irt_worker_end_all();
}
