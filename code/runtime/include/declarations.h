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

#include "irt_inttypes.h"
#include "id_generation.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* ------------------------------ config options ----- */

#define IRT_CONTEXT_LT_BUCKETS 7
#define IRT_DATA_ITEM_LT_BUCKETS 97
#define IRT_EVENT_LT_BUCKETS 72073

// ir interface
#define IRT_SANE_PARALLEL_MAX 128

// affinity
#define IRT_MAX_CORES ((uint64)2048)
#define IRT_AFFINITY_POLICY_ENV "IRT_AFFINITY_POLICY"

// maximum number of frequencies that can be stored
#define IRT_INST_MAX_CPU_FREQUENCIES 32

// message queues
#define IRT_MQUEUE_NAME "/irt_message_queue"
// Be aware that the following value often has a very low OS-dictated ceiling
#define IRT_MQUEUE_MAXMSGS 4
#define IRT_MQUEUE_MAXMSGSIZE 256

// instrumentation
#define IRT_INST_OUTPUT_PATH "IRT_INST_OUTPUT_PATH"
#define IRT_INST_OUTPUT_PATH_CHAR_SIZE 4096
#define IRT_INST_WORKER_EVENT_LOGGING "IRT_INST_WORKER_EVENT_LOGGING"
#define IRT_WORKER_PD_BLOCKSIZE	512
#define IRT_REGION_LIST_SIZE 1024

// performance counters
// environment variable holding the papi events, separated by colons
#define IRT_INST_PAPI_EVENTS "IRT_INST_PAPI_EVENTS"
#define IRT_INST_PAPI_MAX_COUNTERS 16
#define IRT_INST_PAPI_MAX_COUNTERS_COMBINATIONS 512
#define NUMBER_OF_EXTENDED_PERFORMANCE_DATA_ENTRIES 19


// standalone
#define IRT_NUM_WORKERS_ENV "IRT_NUM_WORKERS"

// work group
#define IRT_WG_RING_BUFFER_SIZE 1024

// worker
#define IRT_DEFAULT_VARIANT_ENV "IRT_DEFAULT_VARIANT"

// TODO : better configurability, maybe per-wi stack size set by compiler?
// updated to 8MB due to failing test cases (quicksort, jacobi)
// don't misalign!
#define IRT_WI_STACK_SIZE 8 * 1024 * 1024

#define IRT_MAX_WORK_GROUPS 4
#define IRT_MAX_WORKERS 2048

// Declarations of insieme runtime types in alphabetical lexicographic order

/* ------------------------------ channels ----- */

IRT_DECLARE_ID_TYPE(channel);
typedef struct _irt_channel irt_channel;

/* ------------------------------ client app ----- */

IRT_DECLARE_ID_TYPE(client_app);
typedef struct _irt_client_app irt_client_app;

/* ------------------------------ context ----- */

IRT_DECLARE_ID_TYPE(context);
typedef struct _irt_context irt_context;

/* ------------------------------ data items ----- */

IRT_DECLARE_ID_TYPE(data_item);
typedef struct _irt_data_range irt_data_range;
typedef struct _irt_data_block irt_data_block;
typedef struct _irt_data_item irt_data_item;
typedef struct _irt_lw_data_item irt_lw_data_item;

/* ------------------------------ error handling ----- */

typedef struct _irt_error irt_error;

/* ------------------------------ event handling ----- */

IRT_DECLARE_ID_TYPE(wi_event_register);
typedef struct _irt_wi_event_register irt_wi_event_register;
IRT_DECLARE_ID_TYPE(wg_event_register);
typedef struct _irt_wg_event_register irt_wg_event_register;

/* ------------------------------ locking ----- */

IRT_DECLARE_ID_TYPE(lock);
typedef struct _irt_lock irt_lock;

/* ------------------------------ loop scheduling ----- */

typedef struct _irt_loop_sched_policy irt_loop_sched_policy;
typedef struct _irt_loop_sched_data irt_loop_sched_data;

/* ------------------------------ performance table ----- */

typedef struct _irt_pd_table irt_pd_table;
typedef struct _irt_epd_table irt_epd_table;
typedef struct _irt_apd_table irt_apd_table;

/* ------------------------------ types ----- */

typedef uint32 irt_type_id;
typedef struct _irt_type irt_type;

/* ------------------------------ work groups ----- */

IRT_DECLARE_ID_TYPE(work_group);
//typedef uint32 irt_barrier_id;
typedef uint32 irt_distribute_id;
typedef struct _irt_work_group irt_work_group;
typedef struct _irt_wi_wg_membership irt_wi_wg_membership;

/* ------------------------------ work items ----- */

IRT_DECLARE_ID_TYPE(work_item);
typedef struct _irt_work_item_range irt_work_item_range;
typedef struct _irt_work_item irt_work_item;

/* ------------------------------ work item implementations ----- */

typedef uint32 irt_wi_implementation_id;
typedef struct _irt_wi_di_requirement irt_wi_di_requirement;
typedef struct _irt_wi_implementation irt_wi_implementation;
typedef struct _irt_wi_implementation_variant irt_wi_implementation_variant;
typedef struct _irt_wi_implementation_variant_features irt_wi_implementation_variant_features;
typedef struct _irt_wi_implementation_runtime_data irt_wi_implementation_runtime_data;
typedef void wi_implementation_func(irt_work_item*);
typedef uint64 wi_effort_estimation_func(int64 lower, int64 upper);
typedef void wi_di_req_func(irt_work_item*, irt_wi_di_requirement*);
typedef void wi_channel_req_func(irt_work_item*, irt_channel*);

/* ------------------------------ worker ----- */

IRT_DECLARE_ID_TYPE(worker);
typedef struct _irt_affinity_mask irt_affinity_mask;
typedef struct _irt_worker irt_worker;


