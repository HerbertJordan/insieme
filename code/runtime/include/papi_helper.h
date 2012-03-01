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

/*
 *
 * General instructions on how to use PAPI with the runtime instrumentation system:
 *
 * This file provides helper functions for the PAPI interface. The PAPI events 
 * to be instrumented can be supplied via an environment variable named 
 * IRT_INST_PAPI_PARAMS, separated via whitespaces, e.g. 
 * IRT_INST_PAPI_PARAMS="PAPI_TOT_CYC PAPI_L2_TCM PAPI_BR_MSP".
 *
 * To find out what events are present on a specific machine and what their names 
 * are, navigate to the PAPI installation directory and execute "./bin/papi_avail -a". 
 * This will give you hardware information including the maximum number of counters 
 * that can be measured in parallel, as well as all preset events that can be counted. 
 * Use the preset names like PAPI_TOT_CYC for the environment variable.
 *
 * Since not all events can be measured in arbitrary combinations, one should check 
 * first whether a chosen combination is possible. For this, execute 
 * "./bin/papi_event_chooser PRESET <papi_event> ...", e.g. 
 * "./bin/papi_event_chooser PRESET PAPI_TOT_CYC PAPI_L2_TCM". The program will tell 
 * you either what preset events can still be added or which event of your list cannot 
 * be counted with another already contained in your list.
 *
 */

#include "papi.h"

// environment variable holding the papi parameters, separated by whitespaces
#define IRT_INST_PAPI_PARAMS "IRT_INST_PAPI_PARAMS"
#define IRT_INST_PAPI_MAX_COUNTERS 16

uint32 irt_g_number_of_papi_parameters = 0;

/*
 * parses all papi event names in the environment variable (if not takes default events) to add them to the eventset
 */

void irt_parse_papi_env(int32* irt_papi_event_set) {
	const char papi_params_default[] = "PAPI_TOT_CYC PAPI_L2_TCM PAPI_L3_TCA PAPI_L3_TCM";
	char papi_params[IRT_INST_PAPI_MAX_COUNTERS*16]; // assuming max 16 chars per name

	// get papi counter names from environment variable if present, take default otherwise
	if(getenv(IRT_INST_PAPI_PARAMS))
		strcpy(papi_params, getenv(IRT_INST_PAPI_PARAMS));
	else
		strcpy(papi_params, papi_params_default);

	char* papi_param_toks[IRT_INST_PAPI_MAX_COUNTERS];
	char* cur_tok;
	uint32 number_of_params_supplied = 0;
	uint32 number_of_params_added = 0;

	// get the first parameter
	if((papi_param_toks[0] = strtok(papi_params, " ")) != NULL)
		number_of_params_supplied++;
	else
		return;
	
	// get all remaininc parameters
	while((cur_tok = strtok(NULL, " ")) != NULL)
		papi_param_toks[number_of_params_supplied++] = cur_tok;

	int event_code = 0;
	int retval = 0;

	// add all found parameters to the papi eventset
	for(uint32 j = 0; j < number_of_params_supplied; ++j) {
		if((retval = PAPI_event_name_to_code(papi_param_toks[j], &event_code)) != PAPI_OK)
			IRT_DEBUG("Instrumentation: Error trying to convert PAPI preset name to event code! Reason: %s\n", PAPI_strerror(retval));
		if(PAPI_add_event(*irt_papi_event_set, event_code) == PAPI_OK)
			number_of_params_added++;
		else
			IRT_DEBUG("Instrumentation: Error trying to add PAPI event! Reason: %s\n", PAPI_strerror(retval));
	}

	irt_g_number_of_papi_parameters = number_of_params_added;
}

/*
 * initializes general papi support, does not provide thread support yet
 */

void irt_initialize_papi() {
	int32 retval = 0;
	// initialize papi and check version
	retval = PAPI_library_init(PAPI_VER_CURRENT);
	if(retval > 0 && retval != PAPI_VER_CURRENT) {
		IRT_DEBUG("Instrumentation: PAPI version mismatch: require %d but found %d\n", PAPI_VER_CURRENT, retval);
	} else if (retval < 0) {
		IRT_DEBUG("Error while trying to initialize PAPI! Reason: %s\n", PAPI_strerror(retval));
		if(retval == PAPI_EINVAL)
			IRT_DEBUG("Instrumentation: papi.h version mismatch between compilation and execution!\n");
	}
}

/*
 * initialize papi's thread support, create eventset and add events to it
 */

void irt_initialize_papi_thread(uint64 pthread(void), int32* irt_papi_event_set ) {

	int32 retval = 0;
	if((retval = PAPI_thread_init(pthread)) != PAPI_OK)
		IRT_DEBUG("Instrumentation: Error while trying to initialize PAPI's thread support! Reason: %s\n", PAPI_strerror(retval));

	*irt_papi_event_set = PAPI_NULL; // necessary, otherwise PAPI_create_eventset() will fail

	if(PAPI_create_eventset(irt_papi_event_set) != PAPI_OK)
		IRT_DEBUG("Instrumentation: Error while trying to create PAPI event set! Reason: %s\n", PAPI_strerror(retval));

	// parse event names and add them	
	irt_parse_papi_env(irt_papi_event_set);
}
