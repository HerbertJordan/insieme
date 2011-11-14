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
#include "irt_scheduling.h"
#include "utils/timing.h"
#include "impl/instrumentation.impl.h"

#if IRT_SCHED_POLICY == IRT_SCHED_POLICY_STATIC
#include "sched_policies/impl/irt_sched_static.impl.h"
#endif

#if IRT_SCHED_POLICY == IRT_SCHED_POLICY_LAZY_BINARY_SPLIT
#include "sched_policies/impl/irt_sched_lazy_binary_splitting.impl.h"
#endif

#include <time.h>

/*static inline unsigned long long irt_cur_ticks() {
	//volatile unsigned long long a, d;
	//__asm__ volatile("rdtsc" : "=a" (a), "=d" (d));
	//return (a | (d << 32));
	return 0;
}*/
void irt_scheduling_loop(irt_worker* self) {
	static const long sched_start_nsecs = 1000l; // 1 nanos 
	static const long sched_threshold_nsecs = 5l * 1000l * 1000l; // 5 millis 
	static const long sched_max_nsecs = 100l * 1000l * 1000l; // 100 millis 
	struct timespec wait_time;
	wait_time.tv_sec = 0;
	wait_time.tv_nsec = sched_max_nsecs;
	while(self->state != IRT_WORKER_STATE_STOP) {
		if(irt_scheduling_iteration(self)) {
			wait_time.tv_nsec = sched_start_nsecs;
			continue;
		}
		// nothing to schedule, sleep with fallback
		//wait_time.tv_nsec *= 2;
		wait_time.tv_nsec += 1000;
		if(wait_time.tv_nsec > sched_max_nsecs) wait_time.tv_nsec = sched_max_nsecs;
		if(wait_time.tv_nsec <= sched_threshold_nsecs) { // short wait, busy
			irt_worker_instrumentation_event(self, WORKER_SLEEP_BUSY_START);
			unsigned long long end = irt_time_ticks() + wait_time.tv_nsec;
			while(irt_time_ticks() < end);
			irt_worker_instrumentation_event(self, WORKER_SLEEP_BUSY_END);
		} else {										// long wait, sleep
			self->state = IRT_WORKER_STATE_WAITING;
			irt_worker_instrumentation_event(self, WORKER_SLEEP_START);
			if(irt_nanosleep(&wait_time) != 0) {
				wait_time.tv_nsec = sched_start_nsecs;
			}
			irt_worker_instrumentation_event(self, WORKER_SLEEP_END);
			self->state = IRT_WORKER_STATE_RUNNING;
		}
	}
}

void irt_scheduling_notify(irt_worker* from) {
	for(int i=0; i<irt_g_worker_count; ++i) {
		irt_worker *w = irt_g_workers[i];
		if(w != from && w->state == IRT_WORKER_STATE_WAITING) 
			pthread_kill(w->pthread, IRT_SIG_INTERRUPT);
	}
}
