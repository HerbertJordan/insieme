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

#include <locale.h> // needed to use thousands separator
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include "utils/timing.h"
#include "utils/memory.h"
#include "instrumentation.h"
#include "impl/error_handling.impl.h"

#ifdef IRT_ENABLE_INDIVIDUAL_REGION_INSTRUMENTATION
#include "hwinfo.h"
#include "papi_helper.h"
#include "utils/impl/energy.impl.h"
#include "utils/impl/temperature.impl.h"
#endif

#ifdef IRT_ENABLE_REGION_INSTRUMENTATION
#include "utils/impl/timing.impl.h"
#endif

#ifdef IRT_ENABLE_INSTRUMENTATION
// global function pointers to switch instrumentation on/off
void (*irt_inst_insert_wi_event)(irt_worker* worker, irt_instrumentation_event event, irt_work_item_id subject_id) = &_irt_inst_insert_no_wi_event;
void (*irt_inst_insert_wg_event)(irt_worker* worker, irt_instrumentation_event event, irt_work_group_id subject_id) = &_irt_inst_insert_no_wg_event;
void (*irt_inst_insert_di_event)(irt_worker* worker, irt_instrumentation_event event, irt_data_item_id subject_id) = &_irt_inst_insert_no_di_event;
void (*irt_inst_insert_wo_event)(irt_worker* worker, irt_instrumentation_event event, irt_worker_id subject_id) = &_irt_inst_insert_no_wo_event;
void (*irt_inst_insert_db_event)(irt_worker* worker, irt_instrumentation_event event, irt_worker_id subject_id) = &_irt_inst_insert_no_db_event;
bool irt_g_instrumentation_event_output_is_enabled = false;
bool irt_g_instrumentation_event_output_is_binary = false;

// ============================ dummy functions ======================================
// dummy functions to be used via function pointer to disable
// instrumentation even if IRT_ENABLE_INSTRUMENTATION is set

void _irt_inst_insert_no_wi_event(irt_worker* worker, irt_instrumentation_event event, irt_work_item_id subject_id) { }
void _irt_inst_insert_no_wg_event(irt_worker* worker, irt_instrumentation_event event, irt_work_group_id subject_id) { }
void _irt_inst_insert_no_wo_event(irt_worker* worker, irt_instrumentation_event event, irt_worker_id subject_id) { }
void _irt_inst_insert_no_di_event(irt_worker* worker, irt_instrumentation_event event, irt_data_item_id subject_id) { }
void _irt_inst_insert_no_db_event(irt_worker* worker, irt_instrumentation_event event, irt_worker_id subject_id) { }

// resizes table
void _irt_inst_event_data_table_resize(irt_instrumentation_event_data_table* table) {
	table->size = table->size * 2;
	table->data = (irt_instrumentation_event_data*)realloc(table->data, sizeof(irt_instrumentation_event_data)*table->size);
	IRT_ASSERT(table->data != NULL, IRT_ERR_INSTRUMENTATION, "Instrumentation: Could not perform realloc for event instrumentation data table: %s", strerror(errno))
}

// =============== functions for creating and destroying performance tables ===============

// allocates memory for performance data, sets all fields
irt_instrumentation_event_data_table* irt_inst_create_event_data_table() {
	irt_instrumentation_event_data_table* table = (irt_instrumentation_event_data_table*)malloc(sizeof(irt_instrumentation_event_data_table));
	table->size = IRT_INST_WORKER_PD_BLOCKSIZE * 2;
	table->number_of_elements = 0;
	table->data = (irt_instrumentation_event_data*)malloc(sizeof(irt_instrumentation_event_data) * table->size);
	return table;
}

// frees allocated memory
void irt_inst_destroy_event_data_table(irt_instrumentation_event_data_table* table) {
	if(table != NULL) {
		if(table->data != NULL)
			free(table->data);
		free(table);
	}
}

void _irt_inst_event_insert_time(irt_worker* worker, const int event, const uint64 id, const uint64 time) {
	irt_instrumentation_event_data_table* table = worker->instrumentation_event_data;

	IRT_ASSERT(table->number_of_elements <= table->size, IRT_ERR_INSTRUMENTATION, "Instrumentation: Number of event table entries larger than table size")

	if(table->number_of_elements >= table->size)
		_irt_inst_event_data_table_resize(table);

	irt_instrumentation_event_data* pd = &(table->data[table->number_of_elements]);

	pd->timestamp = time;
	pd->event_id = event;
	pd->index = ((irt_work_item_id*)&id)->index;
	pd->thread = ((irt_work_item_id*)&id)->thread;
	++table->number_of_elements;
}


// commonly used internal function to record events and timestamps
void _irt_inst_event_insert(irt_worker* worker, const int event, const uint64 id) {
	uint64 time = irt_time_ticks();

	IRT_ASSERT(worker != NULL, IRT_ERR_INSTRUMENTATION, "Instrumentation: Trying to add event for worker NULL!");
	IRT_ASSERT(worker == irt_worker_get_current(), IRT_ERR_INSTRUMENTATION, "Instrumentation: Trying to insert event for different worker");

	_irt_inst_event_insert_time(worker, event, id, time);
}

// =========== private event handlers =================================

void _irt_inst_insert_wi_event(irt_worker* worker, irt_instrumentation_event event, irt_work_item_id subject_id) {
	_irt_inst_event_insert(worker, event, subject_id.full);
}

void _irt_inst_insert_wg_event(irt_worker* worker, irt_instrumentation_event event, irt_work_group_id subject_id) {
	_irt_inst_event_insert(worker, event, subject_id.full);
}

void _irt_inst_insert_wo_event(irt_worker* worker, irt_instrumentation_event event, irt_worker_id subject_id) {
	_irt_inst_event_insert(worker, event, subject_id.full);
}

void _irt_inst_insert_di_event(irt_worker* worker, irt_instrumentation_event event, irt_data_item_id subject_id) {
	_irt_inst_event_insert(worker, event, subject_id.full);
}

void _irt_inst_insert_db_event(irt_worker* worker, irt_instrumentation_event event, irt_worker_id subject_id) {
	_irt_inst_event_insert(worker, event, subject_id.full);
}

// ================= debug output functions ==================================

#if defined (USE_OPENCL) && defined (IRT_OCL_INSTR)
    bool irt_g_ocl_temp_event_dump_already_done = false;
#endif

void irt_inst_event_data_output_single(irt_instrumentation_event_data data, FILE* outputfile, bool readable) {
	irt_work_item_id temp_id;
	temp_id.cached = NULL;
	temp_id.index = data.index;
	temp_id.thread = data.thread;
	temp_id.node = 0;
	if(readable) {
		setlocale(LC_ALL, "");
		fprintf(outputfile, "%s,%'16lu,%20s,%'32lu\n", irt_g_instrumentation_group_names[data.event_id], temp_id.full, irt_g_instrumentation_event_names[data.event_id], irt_time_convert_ticks_to_ns(data.timestamp));
	} else
		fprintf(outputfile, "%s,%lu,%s,%lu\n", irt_g_instrumentation_group_names[data.event_id], temp_id.full, irt_g_instrumentation_event_names[data.event_id], irt_time_convert_ticks_to_ns(data.timestamp));
}

void irt_inst_event_data_output_all(bool binary_format) {
	for(int i = 0; i < irt_g_worker_count; ++i)
		irt_inst_event_data_output(irt_g_workers[i], binary_format);
}

// writes csv files
void irt_inst_event_data_output(irt_worker* worker, bool binary_format) {
	// necessary for thousands separator
	//setlocale(LC_ALL, "");
	//
	
	char outputfilename[IRT_INST_OUTPUT_PATH_CHAR_SIZE];
	char defaultoutput[] = ".";
	char* outputprefix = defaultoutput;
	if(getenv(IRT_INST_OUTPUT_PATH_ENV)) outputprefix = getenv(IRT_INST_OUTPUT_PATH_ENV);

	struct stat st;
	int stat_retval = stat(outputprefix,&st);
	if(stat_retval != 0)
		mkdir(outputprefix, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	IRT_ASSERT(stat(outputprefix,&st) == 0, IRT_ERR_INSTRUMENTATION, "Instrumentation: Error creating directory for performance log writing: %s", strerror(errno));

	sprintf(outputfilename, "%s/worker_event_log.%04u", outputprefix, worker->id.thread);

	FILE* outputfile = fopen(outputfilename, "w");
	IRT_ASSERT(outputfile != 0, IRT_ERR_INSTRUMENTATION, "Instrumentation: Unable to open file for event log writing: %s", strerror(errno));
/*	if(outputfile == 0) {
		IRT_DEBUG("Instrumentation: Unable to open file for event log writing\n");
		IRT_DEBUG_ONLY(strerror(errno));
		return;
	}*/
	irt_instrumentation_event_data_table* table = worker->instrumentation_event_data;
	IRT_ASSERT(table != NULL, IRT_ERR_INSTRUMENTATION, "Instrumentation: Worker has no event data!")
	//fprintf(outputfile, "INSTRUMENTATION: %10u events for worker %4u\n", table->number_of_elements, worker->id.thread);

#if defined (USE_OPENCL) && defined (IRT_OCL_INSTR)
	irt_ocl_event_table* ocl_table = worker->event_data;
	IRT_ASSERT(ocl_table != NULL, IRT_ERR_INSTRUMENTATION, "Instrumentation: Worker has no OpenCL event data!")
	int64 ocl_offset = 0;
	_irt_inst_ocl_performance_helper* ocl_helper_table = (_irt_inst_ocl_performance_helper*)malloc(ocl_table->num_events * 4 * sizeof(_irt_inst_ocl_performance_helper));
	int ocl_helper_table_number_of_entries = ocl_table->num_events * 4;
	int helper_counter = 0;

	char ocl_filename[IRT_INST_OUTPUT_PATH_CHAR_SIZE];
	sprintf(ocl_filename, "%s/ocl_event_log", outputprefix);
	FILE* opencl_logfile;

	if(irt_g_ocl_temp_event_dump_already_done)
		opencl_logfile = fopen(ocl_filename, "a");
	else {
		opencl_logfile = fopen(ocl_filename, "w");
		irt_g_ocl_temp_event_dump_already_done = true;
	}

	for(int i = 0; i < ocl_table->num_events; ++i) {
		cl_command_type retval;
		cl_int err_code = clGetEventInfo(ocl_table->event_array[i].cl_event, CL_EVENT_COMMAND_TYPE, sizeof(cl_command_type), &retval, NULL);
		IRT_ASSERT(err_code  == CL_SUCCESS, IRT_ERR_OCL,"Error getting \"event command type\" info: \"%d\"", err_code);

		cl_ulong events[4];
		err_code = clGetEventProfilingInfo(ocl_table->event_array[i].cl_event, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &events[IRT_INST_OCL_QUEUED], NULL);
		err_code |= clGetEventProfilingInfo(ocl_table->event_array[i].cl_event, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &events[IRT_INST_OCL_SUBMITTED], NULL);
		err_code |= clGetEventProfilingInfo(ocl_table->event_array[i].cl_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &events[IRT_INST_OCL_STARTED], NULL);
		err_code |= clGetEventProfilingInfo(ocl_table->event_array[i].cl_event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &events[IRT_INST_OCL_FINISHED], NULL);
		IRT_ASSERT(err_code == CL_SUCCESS, IRT_ERR_OCL, "Error getting profiling info: \"%d\"",  err_code);

		// convert all ocl event information into a flat table, only the ocl_events[j] changes over this run
		for(int j = IRT_INST_OCL_QUEUED; j <= IRT_INST_OCL_FINISHED; ++j) {
			ocl_helper_table[helper_counter].workitem_id = ocl_table->event_array[i].workitem_id;
			ocl_helper_table[helper_counter].timestamp = events[j];
			ocl_helper_table[helper_counter].origin = retval;
			ocl_helper_table[helper_counter].event = j;

			fprintf(opencl_logfile, "Worker: %u %hu %hu, KN,%14lu,\t", worker->id.index, worker->id.thread, worker->id.node, ocl_helper_table[helper_counter].workitem_id);
			switch(ocl_helper_table[helper_counter].origin) {
				case CL_COMMAND_NDRANGE_KERNEL:
					fprintf(opencl_logfile, "ND_");
					break;
				case CL_COMMAND_WRITE_BUFFER:
					fprintf(opencl_logfile, "WRITE_");
					break;
				case CL_COMMAND_READ_BUFFER:
					fprintf(opencl_logfile, "READ_");
					break;
				default:
					fprintf(opencl_logfile, "UNKNOWN_");
			}
			switch(ocl_helper_table[helper_counter].event) {
				case IRT_INST_OCL_QUEUED:
					   fprintf(opencl_logfile, "QUEUED");
					   break;
				case IRT_INST_OCL_SUBMITTED:
					   fprintf(opencl_logfile, "SUBMITTED");
					   break;
				case IRT_INST_OCL_STARTED:
					   fprintf(opencl_logfile, "STARTED");
					   break;
				case IRT_INST_OCL_FINISHED:
					   fprintf(opencl_logfile, "FINISHED");
					   break;
				default:
					   fprintf(opencl_logfile, "UNNKOWN");
					   break;
		}

		fprintf(opencl_logfile, ",\t%18lu\n", ocl_helper_table[helper_counter].timestamp);
		helper_counter++;
		}
	}

	fclose(opencl_logfile);

	IRT_ASSERT(ocl_helper_table_number_of_entries == helper_counter, IRT_ERR_INSTRUMENTATION, "OCL event counts do not match: helper_counter: %d, table_entries: %d", helper_counter, ocl_helper_table_number_of_entries);
	int ocl_helper_table_counter = 0;
#endif


	if(binary_format) {

		/*
		 * dump everything in binary according to the following format:
		 * (note: the strings are written without the termination character '\0'!)
		 *
		 *  8 byte: char, file version identifier, must read "INSIEME1"!
		 * -------------------------------------------------------------
		 *  4 byte: uint32, number of event name table entries (=n)
		 *  4 byte: char, event group identifier 1
		 * 60 byte: char, event name identifier 1
		 *  4 byte: char, event group identifier 2
		 * 60 byte: char, event name identifier 2
		 * ...
		 * ...
		 * ...
		 *  4 byte: char, event group identifier n
		 * 60 byte: char, event name identifier n
		 * -------------------------------------------------------------
		 *  8 byte: uint64, number of events (=m)
		 *  8 byte: uint64, timestamp (nanoseconds) 1
		 *  2 byte: uint16, event id 1
		 *  2 byte: uint16, thread id 1
		 *  4 byte: uint32, target index 1
		 *  2 byte: uint16, event id 2
		 *  2 byte: uint16, thread id 2
		 *  4 byte: uint32, target index 2
		 *  ...
		 *  ...
		 *  ...
		 *  2 byte: uint16, event id m
		 *  2 byte: uint16, thread id m
		 *  4 byte: uint32, target index m
		 * -------------------------------------------------------------
		 * EOF
		 */

		// write version
		const char* header = "INSIEME1";
		fprintf(outputfile, "%s", header);

		// write number of event name table entries followed by group and event names
		fwrite(&(irt_g_inst_num_event_types), sizeof(uint32), 1, outputfile);
		for(int i = 0; i < irt_g_inst_num_event_types; ++i) {
			fprintf(outputfile, "%-4s", irt_g_instrumentation_group_names[i]);
			fprintf(outputfile, "%-60s", irt_g_instrumentation_event_names[i]);
		}

		// write number of events
		const uint64 temp_num_of_elements = (uint64)(table->number_of_elements);
		fwrite(&temp_num_of_elements, sizeof(uint64), 1, outputfile);

		// write event table
		fwrite(table->data, sizeof(irt_instrumentation_event_data), table->number_of_elements, outputfile);

	} else {
		for(int i = 0; i < table->number_of_elements; ++i) {
			irt_inst_event_data_output_single(table->data[i], outputfile, false);
		}
	}

	fclose(outputfile);
#if defined (USE_OPENCL) && defined (IRT_OCL_INSTR)
	free(ocl_helper_table);
#endif
}

// ================= instrumentation function pointer toggle functions =======================

void irt_inst_set_wi_instrumentation(bool enable) {
	if(enable)
		irt_inst_insert_wi_event = &_irt_inst_insert_wi_event;
	else 
		irt_inst_insert_wi_event = &_irt_inst_insert_no_wi_event;
}

void irt_inst_set_wg_instrumentation(bool enable) {
	if(enable)
		irt_inst_insert_wg_event = &_irt_inst_insert_wg_event;
	else
		irt_inst_insert_wg_event = &_irt_inst_insert_no_wg_event;
}

void irt_inst_set_wo_instrumentation(bool enable) {
	if(enable)
		irt_inst_insert_wo_event = &_irt_inst_insert_wo_event;
	else
		irt_inst_insert_wo_event = &_irt_inst_insert_no_wo_event;
}

void irt_inst_set_di_instrumentation(bool enable) {
	if(enable)
		irt_inst_insert_di_event = &_irt_inst_insert_di_event;
	else
		irt_inst_insert_di_event = &_irt_inst_insert_no_di_event;
}

void irt_inst_set_db_instrumentation(bool enable) {
	if(enable)
		irt_inst_insert_db_event = &_irt_inst_insert_db_event;
	else
		irt_inst_insert_db_event = &_irt_inst_insert_no_db_event;
}

void irt_inst_set_all_instrumentation(bool enable) {
	irt_inst_set_wi_instrumentation(enable);
	irt_inst_set_wg_instrumentation(enable);
	irt_inst_set_wo_instrumentation(enable);
	irt_inst_set_di_instrumentation(enable);
	irt_inst_set_db_instrumentation(enable);
	irt_g_instrumentation_event_output_is_enabled = enable;
}

void irt_inst_set_all_instrumentation_from_env() {
	// set whether worker event logging is enabled, and if so, what event types will be logged
	if (getenv(IRT_INST_WORKER_EVENT_LOGGING_ENV) && strcmp(getenv(IRT_INST_WORKER_EVENT_LOGGING_ENV), "true") == 0) {
		irt_log_setting_s("IRT_INST_WORKER_EVENT_LOGGING", "enabled");
		
		// set whether binary format is enabled
		if(getenv(IRT_INST_BINARY_OUTPUT_ENV) && (strcmp(getenv(IRT_INST_BINARY_OUTPUT_ENV), "true") == 0)) {
			irt_g_instrumentation_event_output_is_binary = true;
			irt_log_setting_s("IRT_INST_BINARY_OUTPUT", "enabled");
		} else {
			irt_g_instrumentation_event_output_is_binary = false;
			irt_log_setting_s("IRT_INST_BINARY_OUTPUT", "disabled");
		}

		char* types = getenv(IRT_INST_WORKER_EVENT_TYPES_ENV);
		if(!types) {
			irt_inst_set_all_instrumentation(true);
			irt_inst_set_db_instrumentation(false);
			irt_log_setting_s("IRT_INST_WORKER_EVENT_TYPES", "WI,WO,WG,DI");
			return;
		}

		char* tok = strtok(types, ",");
		char log_output[128];
		uint32 log_output_counter = 0;

		do {
			if(strcmp(tok, "WI") == 0) {
				irt_inst_set_wi_instrumentation(true);
				irt_g_instrumentation_event_output_is_enabled = true;
				log_output_counter += sprintf(&(log_output[log_output_counter]), "WI,");
			} else if(strcmp(tok, "WO") == 0) {
				irt_inst_set_wo_instrumentation(true);
				irt_g_instrumentation_event_output_is_enabled = true;
				log_output_counter += sprintf(&(log_output[log_output_counter]), "WO,");
			} else if(strcmp(tok, "WG") == 0) {
				irt_inst_set_wg_instrumentation(true);
				irt_g_instrumentation_event_output_is_enabled = true;
				log_output_counter += sprintf(&(log_output[log_output_counter]), "WG,");
			} else if(strcmp(tok, "DI") == 0) {
				irt_inst_set_di_instrumentation(true);
				irt_g_instrumentation_event_output_is_enabled = true;
				log_output_counter += sprintf(&(log_output[log_output_counter]), "DI,");
			} else if(strcmp(tok, "DB") == 0) {
				irt_inst_set_db_instrumentation(true);
				irt_g_instrumentation_event_output_is_enabled = true;
				log_output_counter += sprintf(&(log_output[log_output_counter]), "DB,");
			}
		} while((tok = strtok(NULL, ",")) != NULL);
		log_output[log_output_counter-1] = '\0';  // remove the last comma and replace with termination symbol
		irt_log_setting_s("IRT_INST_WORKER_EVENT_TYPES", log_output);
		return;
	}
	irt_inst_set_all_instrumentation(false);
	irt_log_setting_s("IRT_INST_WORKER_EVENT_LOGGING", "disabled");
}

#else // if not IRT_ENABLE_INSTRUMENTATION

// ============ to be used if IRT_ENABLE_INSTRUMENTATION is not set ==============

irt_instrumentation_event_data_table* irt_inst_create_event_data_table() { return NULL; }
void irt_inst_destroy_event_data_table(irt_instrumentation_event_data_table* table) {}

void irt_inst_insert_wi_event(irt_worker* worker, irt_instrumentation_event event, irt_work_item_id subject_id) {}
void irt_inst_insert_wg_event(irt_worker* worker, irt_instrumentation_event event, irt_work_group_id subject_id) {}
void irt_inst_insert_wo_event(irt_worker* worker, irt_instrumentation_event event, irt_worker_id subject_id) {}
void irt_inst_insert_di_event(irt_worker* worker, irt_instrumentation_event event, irt_data_item_id subject_id) {}
void irt_inst_insert_db_event(irt_worker* worker, irt_instrumentation_event event, irt_worker_id subject_id) {}

void irt_inst_event_data_output(irt_worker* worker, bool binary_format) {}

#endif // IRT_ENABLE_INSTRUMENTATION







// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//																				Regions
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#ifndef IRT_ENABLE_REGION_INSTRUMENTATION

typedef struct _irt_inst_region_data {} irt_inst_region_data;

void irt_inst_region_start(irt_context* context, irt_worker* wi, region_id id) { }
void irt_inst_region_end(irt_context* context, irt_worker* wi, region_id id) { }

void irt_inst_region_start_pfor(irt_context* context, region_id id) { }
void irt_inst_region_end_pfor(irt_context* context, region_id id, uint64 walltime, uint64 cputime) { }

void irt_inst_region_suspend(irt_work_item* wi) { }
void irt_inst_region_continue(irt_work_item* wi) { }

void irt_inst_region_init(irt_context* context) {};
void irt_inst_region_finalize(irt_context* context) {};

void irt_inst_region_set_mode(irt_context* context, irt_inst_region_mode mode) { }
void irt_inst_region_set_mode_for_region(irt_context* context, region_id id, irt_inst_region_mode mode) { }

#else

// make sure scheduling policy is fixed to static
#if !(IRT_SCHED_POLICY == IRT_SCHED_POLICY_STATIC)
	#error "IRT INSTRUMENTATION ONLY SUPPORTS STATIC SCHEDULING AT THIS POINT"
#endif


// ------------------------------------------------------------- type definitions


typedef struct __irt_inst_region_setup {
	void (*region_start)(irt_context* context, irt_worker* worker, region_id id);
	void (*region_end)(irt_context* context, irt_worker* worker, region_id id);
	void (*region_start_pfor)(irt_context* context, region_id id);
	void (*region_end_pfor)(irt_context* context, region_id id, uint64 walltime, uint64 cputime);
} _irt_inst_region_setup;

typedef struct __irt_inst_aggregated_data {
	uint64 cputime;
	uint64 walltime;
	uint64 num_exec;
} _irt_inst_aggregated_data;

typedef struct _irt_inst_region_data {
	_irt_inst_region_setup setup;
	_irt_inst_aggregated_data data;
} irt_inst_region_data;

// ------------------------------------------------------------- utilities

#define IRT_INST_REGION_CHECK_ID(context, id) \
		IRT_ASSERT(0 <= id && id < context->num_regions, IRT_ERR_INSTRUMENTATION, "Instrumentation: Invalid region specified!");



static inline void _irt_inst_region_set_timestamp(irt_work_item* wi) {
	wi->last_timestamp = irt_time_ticks();
}

static inline void _irt_inst_region_add_time(irt_work_item* wi) {
	if(wi->region) {
		uint64 temp = irt_time_ticks();
		irt_atomic_fetch_and_add(&(wi->region->cputime), temp - wi->last_timestamp);		// @Philipp: this might not be necessary
	}
}


// ---------- region list management --------------

/**
 * The region list is the stack maintained by workers to realize support for nested regions.
 */


irt_region_list* irt_inst_create_region_list() {
	irt_region_list* list = (irt_region_list*)malloc(sizeof(irt_region_list));
	list->head = NULL;
	return list;
}

void irt_inst_destroy_region_list(irt_region_list* list) {
	irt_region* temp;
	irt_region* current = list->head;
	while(current != NULL) {
		temp = current->next;
		free(current);
		current = temp;
	}
	free(list);
}

irt_region* irt_inst_region_list_new_item(irt_worker* worker) {
	irt_region* retval;
	if(worker->region_reuse_list->head) {
		retval = worker->region_reuse_list->head;
		worker->region_reuse_list->head = retval->next;
	} else {
		retval = (irt_region*)malloc(sizeof(irt_region));
	}
	return retval;
}

void irt_inst_region_list_recycle_item(irt_worker* worker, irt_region* region) {
	region->next = worker->region_reuse_list->head;
	worker->region_reuse_list->head = region;
}



// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//																		aggregated data table (efficiency log)
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


void _irt_inst_region_aggregated_data_insert(irt_context* context, int64 id, uint64 walltime, uint64 cputime) {

	// check region id
	IRT_INST_REGION_CHECK_ID(context, id);

	// get entry
	_irt_inst_aggregated_data* apd = &(context->inst_region_data[id].data);

	// update values (atomic operations to synchronize access)
	irt_atomic_fetch_and_add(&apd->walltime, walltime);
	irt_atomic_fetch_and_add(&apd->cputime, cputime);
	irt_atomic_inc(&apd->num_exec);
}


void _irt_inst_region_start_aggregated(irt_context* context, irt_worker* worker, region_id id) {

	uint64 startTime = irt_time_ticks();

	// account time so far to parent region (if present)
	if (worker->cur_wi->region) {
		worker->cur_wi->region->cputime += startTime - worker->cur_wi->last_timestamp;
	}
	worker->cur_wi->last_timestamp = startTime;

	// add parent region
	irt_region* region = irt_inst_region_list_new_item(worker);
	region->cputime = 0;
	region->start_time = startTime;
	region->next = worker->cur_wi->region;
	worker->cur_wi->region = region;
}

void _irt_inst_region_end_aggregated_internal(irt_worker* worker) {
	// pop top element of region stack
	irt_region* ending_region = worker->cur_wi->region;
	worker->cur_wi->region = ending_region->next;

	// update cpu time counter of surrounding region if present
	if(worker->cur_wi->region) {
		// if the ended region was a nested one, add execution time to outer region
		worker->cur_wi->region->cputime += ending_region->cputime;
	}

	// recycle region
	irt_inst_region_list_recycle_item(worker, ending_region);
}

void _irt_inst_region_end_aggregated(irt_context* context, irt_worker* worker, region_id id) {

	uint64 endTime = irt_time_ticks();

	// account cpu time
	worker->cur_wi->region->cputime += endTime - worker->cur_wi->last_timestamp;
	worker->cur_wi->last_timestamp = endTime;		// update last-timestamp field

	// record data
	_irt_inst_region_aggregated_data_insert(context, id, endTime - worker->cur_wi->region->start_time, worker->cur_wi->region->cputime);

	// pop nested region stack
	_irt_inst_region_end_aggregated_internal(worker);
}

void _irt_inst_region_start_pfor_aggregated(irt_context* context, region_id id) {
	// forward call to standard implementation using current worker
	_irt_inst_region_start_aggregated(context, irt_worker_get_current(), id);
}

void _irt_inst_region_end_pfor_aggregated(irt_context* context, region_id id, uint64 walltime, uint64 cputime) {

	// accumulated data
	_irt_inst_region_aggregated_data_insert(context, id, walltime, cputime);

	// pop nested region stack
	_irt_inst_region_end_aggregated_internal(irt_worker_get_current());
}

void _irt_inst_region_aggregated_data_output(uint32 num_regions, irt_inst_region_data* table) {

	IRT_ASSERT(table != NULL, IRT_ERR_INSTRUMENTATION, "Instrumentation: Worker has no performance data!")

	// environmental variable can hold the output path for the performance logs, default is .
	char outputfilename[IRT_INST_OUTPUT_PATH_CHAR_SIZE];
	char defaultoutput[] = ".";
	char* outputprefix = defaultoutput;
	if(getenv(IRT_INST_OUTPUT_PATH_ENV)) outputprefix = getenv(IRT_INST_OUTPUT_PATH_ENV);

	struct stat st;
	int stat_retval = stat(outputprefix,&st);
	if(stat_retval != 0)
		mkdir(outputprefix, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	IRT_ASSERT(stat(outputprefix,&st) == 0, IRT_ERR_INSTRUMENTATION, "Instrumentation: Error creating directory for efficiency log writing: %s", strerror(errno));

	sprintf(outputfilename, "%s/worker_efficiency_log", outputprefix);

	FILE* outputfile = fopen(outputfilename, "w");
	IRT_ASSERT(outputfile != 0, IRT_ERR_INSTRUMENTATION, "Instrumentation: Unable to open file for efficiency log writing: %s", strerror(errno));


	fprintf(outputfile, "#subject,id,wall_time(ns),cpu_time(ns),num_executions\n");

	for(int i = 0; i < num_regions; ++i) {
		fprintf(outputfile, "RG,%d,%lu,%lu,%lu\n",
			i,
			irt_time_convert_ticks_to_ns(table[i].data.walltime),
			irt_time_convert_ticks_to_ns(table[i].data.cputime),
			table[i].data.num_exec);
	}
	fclose(outputfile);
}



// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//																		detailed instrumentation log (performance log)
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


// =============== initialization functions ===============

void irt_instrumentation_init_energy_instrumentation() { }

void _irt_inst_region_data_table_resize(irt_instrumentation_region_data_table* table) {
	table->size = table->size * 2;
	table->data = (irt_instrumentation_region_data*)realloc(table->data, sizeof(irt_instrumentation_region_data)*table->size);
	IRT_ASSERT(table->data != NULL, IRT_ERR_INSTRUMENTATION, "Instrumentation: Could not perform realloc for region instrumentation data table: %s", strerror(errno))
}

irt_instrumentation_region_data_table* irt_inst_create_region_data_table() {
	irt_instrumentation_region_data_table* table = (irt_instrumentation_region_data_table*)malloc(sizeof(irt_instrumentation_region_data_table));
	table->size = IRT_INST_WORKER_PD_BLOCKSIZE * 2;
	table->number_of_elements = 0;
	table->data = (irt_instrumentation_region_data*)malloc(sizeof(irt_instrumentation_region_data) * table->size);
	return table;
}

void irt_inst_destroy_region_data_table(irt_instrumentation_region_data_table* table) {
	if(table != NULL) {
		if(table->data != NULL)
			free(table->data);
		free(table);
	}
}


void _irt_inst_region_detail_data_insert(irt_worker* worker, const int event, const uint64 id) {

	irt_instrumentation_region_data_table* table = worker->instrumentation_region_data;
		
	IRT_ASSERT(table->number_of_elements <= table->size, IRT_ERR_INSTRUMENTATION, "Instrumentation: Number of region event table entries larger than table size")
	
	if(table->number_of_elements >= table->size)
		_irt_inst_region_data_table_resize(table);
	irt_instrumentation_region_data* epd = &(table->data[table->number_of_elements++]);

	rapl_energy_data data;
	data.number_of_cpus = irt_get_num_sockets();
	double package[data.number_of_cpus];
	double mc[data.number_of_cpus];
	double cores[data.number_of_cpus];
	data.package = package;
	data.mc = mc;
	data.cores = cores;

	switch(event) {
		case IRT_INST_REGION_START:
			epd->event = event;
			epd->subject_id = id;
//			irt_get_energy_consumption(&(epd->data[PERFORMANCE_DATA_ENTRY_ENERGY].value_double));
			irt_get_energy_consumption(&data);
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_1].value_double = data.package[0];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_1].value_double = data.mc[0];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_1].value_double = data.cores[0];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_2].value_double = data.package[1];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_2].value_double = data.mc[1];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_2].value_double = data.cores[1];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_3].value_double = data.package[2];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_3].value_double = data.mc[2];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_3].value_double = data.cores[2];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_4].value_double = data.package[3];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_4].value_double = data.mc[3];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_4].value_double = data.cores[3];

			epd->data[PERFORMANCE_DATA_ENTRY_TEMPERATURE_CORE].value_uint64 = irt_get_temperature_core(worker);
			epd->data[PERFORMANCE_DATA_ENTRY_TEMPERATURE_PACKAGE].value_uint64 = irt_get_temperature_package(worker);

//			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY].value_double = -1;
			// set all papi counter fields for REGION_START to -1 since we don't use them here
			for(int i = PERFORMANCE_DATA_ENTRY_PAPI_COUNTER_1; i < PERFORMANCE_DATA_ENTRY_PAPI_COUNTER_1 + worker->irt_papi_number_of_events; ++i)
				epd->data[i].value_uint64 = UINT_MAX;
			PAPI_start(worker->irt_papi_event_set);

			irt_get_memory_usage(&(epd->data[PERFORMANCE_DATA_ENTRY_MEMORY_VIRT].value_uint64), &(epd->data[PERFORMANCE_DATA_ENTRY_MEMORY_RES].value_uint64));
		
			// do time as late as possible to exclude overhead of remaining instrumentation/measurements
			epd->timestamp = irt_time_ticks();
			//epd->timestamp = PAPI_get_virt_cyc(); // counts only since process start and does not include other scheduled processes, but decreased accuracy
			break;

		case IRT_INST_REGION_END:
			; // do not remove! bug in some versions of gcc!
			// do time as early as possible to exclude overhead of remaining instrumentation/measurements
			//uint64 time = PAPI_get_virt_cyc(); // counts only since process start and does not include other scheduled processes, but decreased accuracy
			uint64 time = irt_time_ticks();
			uint64 papi_temp[IRT_INST_PAPI_MAX_COUNTERS];
			PAPI_read(worker->irt_papi_event_set, (long long*)papi_temp);
			PAPI_reset(worker->irt_papi_event_set);
			
			irt_get_memory_usage(&(epd->data[PERFORMANCE_DATA_ENTRY_MEMORY_VIRT].value_uint64), &(epd->data[PERFORMANCE_DATA_ENTRY_MEMORY_RES].value_uint64));

			//double energy_consumption = -1;

			epd->timestamp = time;
			epd->event = event;
			epd->subject_id = id;
//			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY].value_double = energy_consumption;
//			irt_get_energy_consumption(&(epd->data[PERFORMANCE_DATA_ENTRY_ENERGY].value_double));
			irt_get_energy_consumption(&data);
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_1].value_double = data.package[0];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_1].value_double = data.mc[0];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_1].value_double = data.cores[0];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_2].value_double = data.package[1];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_2].value_double = data.mc[1];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_2].value_double = data.cores[1];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_3].value_double = data.package[2];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_3].value_double = data.mc[2];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_3].value_double = data.cores[2];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_4].value_double = data.package[3];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_4].value_double = data.mc[3];
			epd->data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_4].value_double = data.cores[3];

			epd->data[PERFORMANCE_DATA_ENTRY_TEMPERATURE_CORE].value_uint64 = irt_get_temperature_core(worker);
			epd->data[PERFORMANCE_DATA_ENTRY_TEMPERATURE_PACKAGE].value_uint64 = irt_get_temperature_package(worker);

			for(int i=PERFORMANCE_DATA_ENTRY_PAPI_COUNTER_1; i<(worker->irt_papi_number_of_events+PERFORMANCE_DATA_ENTRY_PAPI_COUNTER_1); ++i)
				epd->data[i].value_uint64 = papi_temp[i-PERFORMANCE_DATA_ENTRY_PAPI_COUNTER_1];
			break;
	}

}


void irt_inst_region_detail_data_output(irt_worker* worker) {
	// environmental variable can hold the output path for the performance logs, default is .
	char outputfilename[IRT_INST_OUTPUT_PATH_CHAR_SIZE];
	char defaultoutput[] = ".";
	char* outputprefix = defaultoutput;
	if(getenv(IRT_INST_OUTPUT_PATH_ENV)) outputprefix = getenv(IRT_INST_OUTPUT_PATH_ENV);

	struct stat st;
	int stat_retval = stat(outputprefix,&st);
	if(stat_retval != 0)
		mkdir(outputprefix, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	IRT_ASSERT(stat(outputprefix,&st) == 0, IRT_ERR_INSTRUMENTATION, "Instrumentation: Error creating directory for performance log writing: %s", strerror(errno));

	sprintf(outputfilename, "%s/worker_performance_log.%04u", outputprefix, worker->id.thread);

	// used to print papi names to the header of the performance logs
	int number_of_papi_events = IRT_INST_PAPI_MAX_COUNTERS;
	int papi_events[IRT_INST_PAPI_MAX_COUNTERS];
	char event_name_temp[PAPI_MAX_STR_LEN];
	int retval = 0;

	if((retval = PAPI_list_events(worker->irt_papi_event_set, papi_events, &number_of_papi_events)) != PAPI_OK) {
		IRT_DEBUG("Instrumentation: Error listing papi events, there will be no performance counter measurement data! Reason: %s\n", PAPI_strerror(retval));
		number_of_papi_events = 0;
	}

	FILE* outputfile = fopen(outputfilename, "w");
	IRT_ASSERT(outputfile != 0, IRT_ERR_INSTRUMENTATION, "Instrumentation: Unable to open file for performance log writing: %s\n", strerror(errno));
	//fprintf(outputfile, "#subject,id,timestamp_start_(ns),timestamp_end_(ns),virt_memory_start_(kb),virt_memory_end_(kb),res_memory_start_(kb),res_memory_end_(kb),energy_start_(j),energy_end_(j)");
	fprintf(outputfile, "#subject,"
			"id,timestamp_start_(ns),"
			"timestamp_end_(ns),"
			"virt_memory_start_(kb),"
			"virt_memory_end_(kb),"
			"res_memory_start_(kb),"
			"res_memory_end_(kb),"
			"energy_package_1_start_(j),"
			"energy_package_1_end_(j),"
			"energy_mc_1_start_(j),"
			"energy_mc_1_end_(j),"
			"energy_cores_1_start_(j),"
			"energy_cores_1_end_(j),"
			"energy_package_2_start_(j),"
			"energy_package_2_end_(j),"
			"energy_mc_2_start_(j),"
			"energy_mc_2_end_(j),"
			"energy_cores_2_start_(j),"
			"energy_cores_2_end_(j),"
			"energy_package_3_start_(j),"
			"energy_package_3_end_(j),"
			"energy_mc_3_start_(j),"
			"energy_mc_3_end_(j),"
			"energy_cores_3_start_(j),"
			"energy_cores_3_end_(j),"
			"energy_package_4_start_(j),"
			"energy_package_4_end_(j),"
			"energy_mc_4_start_(j),"
			"energy_mc_4_end_(j),"
			"energy_cores_4_start_(j),"
			"energy_cores_4_end_(j),"
			"temperature_core_start_(c),"
			"temperature_core_end_(c),"
			"temperature_pkg_start_(c),"
			"temperature_pkg_end_(c)");

	// get the papi event names and print them to the header
	for(int i = 0; i < number_of_papi_events; ++i) {
		PAPI_event_code_to_name(papi_events[i], event_name_temp);
		fprintf(outputfile, ",%s", event_name_temp);
	}
	fprintf(outputfile, "\n");

	irt_instrumentation_region_data_table* table = worker->instrumentation_region_data;
	IRT_ASSERT(table != NULL, IRT_ERR_INSTRUMENTATION, "Instrumentation: Worker has no performance data!")

	// this loop iterates through the table: if REGION_END is found, search reversely for matching REGION_START and output corresponding values in a single line
	for(int i = 0; i < table->number_of_elements; ++i) {
		if(table->data[i].event == IRT_INST_REGION_END) {
			// holds data of matching REGION_START
			irt_instrumentation_region_data start_data = {};
			// iterate back through performance entries to find the matching start with this id
			for(int j = i-1; j >= 0; --j) {
				if(table->data[j].subject_id == table->data[i].subject_id)
					if(table->data[j].event == IRT_INST_REGION_START) {
						start_data = table->data[j]; //memcpy(&start_data, &(table->data[j]), sizeof(irt_inst_region_data));
						break;
					}
			}

			IRT_ASSERT(start_data.timestamp != 0, IRT_ERR_INSTRUMENTATION, "Instrumentation: Cannot find a matching start statement")

			// single fprintf for performance reasons
			// outputs all data in pairs: value_when_entering_region, value_when_exiting_region
	//		fprintf(outputfile, "RG,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%1.8f,%1.8f",
			fprintf(outputfile, "RG,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%1.8f,%lu,%lu,%lu,%lu",
					table->data[i].subject_id,
					irt_time_convert_ticks_to_ns(start_data.timestamp), 
					irt_time_convert_ticks_to_ns(table->data[i].timestamp),
					start_data.data[PERFORMANCE_DATA_ENTRY_MEMORY_VIRT].value_uint64, 
					table->data[i].data[PERFORMANCE_DATA_ENTRY_MEMORY_VIRT].value_uint64, 
					start_data.data[PERFORMANCE_DATA_ENTRY_MEMORY_RES].value_uint64, 
					table->data[i].data[PERFORMANCE_DATA_ENTRY_MEMORY_RES].value_uint64, 
//					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY].value_double,
//					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY].value_double);

					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_1].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_1].value_double,
					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_1].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_1].value_double,
					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_1].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_1].value_double,

					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_2].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_2].value_double,
					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_2].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_2].value_double,
					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_2].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_2].value_double,

					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_3].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_3].value_double,
					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_3].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_3].value_double,
					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_3].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_3].value_double,

					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_4].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_PACKAGE_4].value_double,
					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_4].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_MC_4].value_double,
					start_data.data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_4].value_double,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_ENERGY_CORES_4].value_double,

					start_data.data[PERFORMANCE_DATA_ENTRY_TEMPERATURE_CORE].value_uint64,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_TEMPERATURE_CORE].value_uint64,
					start_data.data[PERFORMANCE_DATA_ENTRY_TEMPERATURE_PACKAGE].value_uint64,
					table->data[i].data[PERFORMANCE_DATA_ENTRY_TEMPERATURE_PACKAGE].value_uint64);
			// prints all performance counters, assumes that the order of the enums is correct (contiguous from ...COUNTER_1 to ...COUNTER_N
			for(int j = PERFORMANCE_DATA_ENTRY_PAPI_COUNTER_1; j < (worker->irt_papi_number_of_events + PERFORMANCE_DATA_ENTRY_PAPI_COUNTER_1); ++j) {
				if( table->data[i].data[j].value_uint64 == UINT_MAX) // used to filter missing results, replace with -1 in output
					fprintf(outputfile, ",-1");
				else
					fprintf(outputfile, ",%lu", table->data[i].data[j].value_uint64);
			}
			fprintf(outputfile, "\n");
		}
	}
	fclose(outputfile);
}


// the region start / end routines for aggregated and detailed modes
void _irt_inst_region_start_detail(irt_context* context, irt_worker* worker, region_id id) {

	// add entry within detail log
	_irt_inst_region_detail_data_insert(worker, IRT_INST_REGION_START, id);

	// .. and within the aggregated log
	_irt_inst_region_start_aggregated(context, worker, id);

}

void _irt_inst_region_end_detail(irt_context* context, irt_worker* worker, region_id id) {

	// include the aggregated log ...
	_irt_inst_region_end_aggregated(context, worker, id);

	// ... and the details
	_irt_inst_region_detail_data_insert(worker, IRT_INST_REGION_END, id);

}

void _irt_inst_region_start_pfor_detail(irt_context* context, region_id id) {

	// add entry within detail log
	_irt_inst_region_detail_data_insert(irt_worker_get_current(), IRT_INST_REGION_START, id);

	// .. and within the aggregated log
	_irt_inst_region_start_pfor_aggregated(context, id);

}

void _irt_inst_region_end_pfor_detail(irt_context* context, region_id id, uint64 walltime, uint64 cputime) {

	// include the aggregated log ...
	_irt_inst_region_end_pfor_aggregated(context, id, walltime, cputime);

	// ... and the details
	_irt_inst_region_detail_data_insert(irt_worker_get_current(), IRT_INST_REGION_END, id);

}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//																			region marking
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


void irt_inst_region_start(irt_context* context, irt_worker* worker, region_id id) {
	IRT_INST_REGION_CHECK_ID(context, id);

	// check whether region-start function is present
	if (context->inst_region_data[id].setup.region_start) {
		(*context->inst_region_data[id].setup.region_start)(context, worker, id);		// call it
	}
}

void irt_inst_region_end(irt_context* context, irt_worker* worker, region_id id) {
	IRT_INST_REGION_CHECK_ID(context, id);

	// check whether region-start function is present
	if (context->inst_region_data[id].setup.region_end) {
		(*context->inst_region_data[id].setup.region_end)(context, worker, id);		// call it
	}
}

void irt_inst_region_start_pfor(irt_context* context, region_id id) {
	IRT_INST_REGION_CHECK_ID(context, id);

	// check whether region-start function is present
	if (context->inst_region_data[id].setup.region_start_pfor) {
		(*context->inst_region_data[id].setup.region_start_pfor)(context, id);		// call it
	}
}

void irt_inst_region_end_pfor(irt_context* context, region_id id, uint64 walltime, uint64 cputime) {
	IRT_INST_REGION_CHECK_ID(context, id);

	// check whether region-start function is present
	if (context->inst_region_data[id].setup.region_end_pfor) {
		(*context->inst_region_data[id].setup.region_end_pfor)(context, id, walltime, cputime);		// call it
	}
}

void irt_inst_region_suspend(irt_work_item* wi) {
	_irt_inst_region_add_time(wi);
}

void irt_inst_region_continue(irt_work_item* wi) {
	_irt_inst_region_set_timestamp(wi);
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//																		instrumentation management
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void irt_inst_region_init(irt_context* context) {

	// ----- initialize region instrumentation table ---------

	// create table
	context->inst_region_data = (irt_inst_region_data*)malloc(sizeof(irt_inst_region_data) * context->num_regions);

	// init table
	for(uint32 i=0; i < context->num_regions; i++) {
		context->inst_region_data[i].data.cputime = 0;
		context->inst_region_data[i].data.walltime = 0;
		context->inst_region_data[i].data.num_exec = 0;
	}

	// init mode
	irt_inst_region_set_mode(context, IRT_INST_REGION_NONE);
}

void irt_inst_region_finalize(irt_context* context) {

	// dump data - aggregated log
	_irt_inst_region_aggregated_data_output(context->num_regions, context->inst_region_data);

	// free region instrumentation data
	free(context->inst_region_data);
}

void irt_inst_region_set_mode(irt_context* context, irt_inst_region_mode mode) {
	// just set the same mode for all individual regions
	for(uint32 i = 0; i<context->num_regions; i++) {
		irt_inst_region_set_mode_for_region(context, i, mode);
	}
}

void irt_inst_region_set_mode_for_region(irt_context* context, uint32 region_id, irt_inst_region_mode mode) {

	// obtain pointer to setup of requested region
	_irt_inst_region_setup* setup = &(context->inst_region_data[region_id].setup);

	switch(mode) {
		case IRT_INST_REGION_NONE: {
			setup->region_start = NULL;
			setup->region_end = NULL;
			setup->region_start_pfor = NULL;
			setup->region_end_pfor = NULL;
			break;
		}
		case IRT_INST_REGION_AGGREGATED: {
			setup->region_start = _irt_inst_region_start_aggregated;
			setup->region_end = _irt_inst_region_end_aggregated;
			setup->region_start_pfor = _irt_inst_region_start_pfor_aggregated;				// modified variant using current worker
			setup->region_end_pfor = _irt_inst_region_end_pfor_aggregated;					// modified variant
			break;
		}
		case IRT_INST_REGION_DETAIL: {
			setup->region_start = _irt_inst_region_start_detail;
			setup->region_end = _irt_inst_region_end_detail;
			setup->region_start_pfor = _irt_inst_region_start_pfor_detail;
			setup->region_end_pfor = _irt_inst_region_end_pfor_detail;
			break;
		}
		default: {
			IRT_ASSERT(false, IRT_ERR_INVALIDARGUMENT, "Invalid region instrumentation mode selected!");
			break;
		}
	}
}



#endif
