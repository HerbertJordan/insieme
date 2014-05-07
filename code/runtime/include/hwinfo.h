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
#ifndef __GUARD_HWINFO_H
#define __GUARD_HWINFO_H

#include <unistd.h>

#ifdef IRT_USE_PAPI
#include "papi_helper.h"
#endif
#include "error_handling.h"

static uint32 __irt_g_cached_cpu_count = 0;
static uint32 __irt_g_cached_threads_per_core_count = 0;
static uint32 __irt_g_cached_cores_per_socket_count = 0;
static uint32 __irt_g_cached_sockets_count = 0;
static uint32 __irt_g_cached_numa_nodes_count = 0;

uint32 irt_get_num_cpus() {
	if(__irt_g_cached_cpu_count!=0) return __irt_g_cached_cpu_count;
	uint32 ret = 1;
#ifdef _SC_NPROCESSORS_ONLN
	// Linux
	ret = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_WIN32)
	// Windows
	SYSTEM_INFO sysinfo; 
	GetSystemInfo( &sysinfo ); 
	ret = sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROC_ONLN)
	// Irix
	ret = sysconf(_SC_NPROC_ONLN);
#elif defined(MPC_GETNUMSPUS)
	// HPUX
	ret = mpctl(MPC_GETNUMSPUS, NULL, NULL);
#endif
	if(ret<1) ret = 1;
	__irt_g_cached_cpu_count = ret;
	return ret;
}

// to be used only for testing
void _irt_set_num_cpus(uint32 num) {
	__irt_g_cached_cpu_count = num;
}

int32 _irt_setup_hardware_info() {
#ifdef IRT_USE_PAPI
	irt_papi_init();

	const PAPI_hw_info_t* hwinfo = PAPI_get_hardware_info();

	if(hwinfo == NULL) {
		IRT_DEBUG("hwinfo: Error trying to get hardware information from PAPI! %p\n", hwinfo);
		return -1;
	}

	if(hwinfo->threads > 0)
		__irt_g_cached_threads_per_core_count = hwinfo->threads;
	if(hwinfo->cores > 0)
		__irt_g_cached_cores_per_socket_count = hwinfo->cores;
	if(hwinfo->sockets > 0)
		__irt_g_cached_sockets_count = hwinfo->sockets;
	if(hwinfo->nnodes > 0)
		__irt_g_cached_numa_nodes_count = hwinfo->nnodes;
#else
	IRT_DEBUG("hwinfo: papi not available, reporting dummy values")
#endif

	return 0;
}

uint32 irt_get_num_threads_per_core() {
	if(__irt_g_cached_threads_per_core_count == 0)
		_irt_setup_hardware_info();

	IRT_ASSERT(__irt_g_cached_threads_per_core_count != 0, IRT_ERR_HW_INFO, "Hardware information only available when runtime compiled with PAPI!")

	return __irt_g_cached_threads_per_core_count;
}

uint32 irt_get_num_cores_per_socket() {
	if(__irt_g_cached_cores_per_socket_count == 0)
		_irt_setup_hardware_info();

	IRT_ASSERT(__irt_g_cached_threads_per_core_count != 0, IRT_ERR_HW_INFO, "Hardware information only available when runtime compiled with PAPI!")

	return __irt_g_cached_cores_per_socket_count;
}

uint32 irt_get_num_sockets() {
	if(__irt_g_cached_sockets_count == 0)
		_irt_setup_hardware_info();

	IRT_ASSERT(__irt_g_cached_threads_per_core_count != 0, IRT_ERR_HW_INFO, "Hardware information only available when runtime compiled with PAPI!")

	return __irt_g_cached_sockets_count;
}

uint32 irt_get_num_numa_nodes() {
	if(__irt_g_cached_numa_nodes_count == 0)
		_irt_setup_hardware_info();

	IRT_ASSERT(__irt_g_cached_threads_per_core_count != 0, IRT_ERR_HW_INFO, "Hardware information only available when runtime compiled with PAPI!")

	return __irt_g_cached_numa_nodes_count;
}

uint32 irt_get_sibling_hyperthread(uint32 coreid) {
	// should work for Intel HyperThreading and sanely set up Linux systems
	uint32 num_threads_per_core = irt_get_num_threads_per_core();
	if(num_threads_per_core > 1) {
		uint32 cores_total = irt_get_num_sockets() * irt_get_num_cores_per_socket();
		return (coreid + cores_total)%(cores_total*num_threads_per_core);
	} else
		return coreid;
}

bool irt_get_hyperthreading_enabled() {
	return (irt_get_num_threads_per_core() > 1);
}


#endif // ifndef __GUARD_HWINFO_H
