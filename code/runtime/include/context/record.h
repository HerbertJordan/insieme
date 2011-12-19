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


// ------------------------------------------------------------------------------------
//    Type Definitions
// ------------------------------------------------------------------------------------


/**
 * A struct summarizing the information stored per data block. Every block has an ID,
 * a pointer to its beginning and a size.
 */
typedef struct {
	uint32 id;			// the ID of this data block (auto-generated by the context capturing)
	void* base;			// the base pointer referencing the begin of the block
	uint32 size;		// the size of the represented block
} irt_cap_data_block_info;


/**
 * A struct summarizing the usage of a block being accessed by a region. For every region,
 * the read / write operations on every block element is recorded.
 */
typedef struct _irt_cap_block_usage_info {

	const irt_cap_data_block_info* block;		// the block this usage info is covering

	int32 tag;					// the user defined tag identifying this region (negative if untagged)

	char* life_in_values;		// the life_in_values of the block when being read the first time when entering a region
	bool* read;					// a flag mask marking read values

	bool* is_pointer;			// a flag mask marking data being interpreted as pointers

	char* life_out_values;		// the data written to the memory cells within the region
	bool* last_written;			// a flag mask marking values been written last by the corrsponding region

	bool* is_life_out;			// a flag mask marking written values being read after the region

	struct _irt_cap_block_usage_info* next; 	// enables this struct to be used within a list

} irt_cap_block_usage_info;


/**
 * A struct representing a code region within the context capturing. A region
 * is labeled by the user using a simple id. This will allow to associate recorded
 * data to certain code sections.
 */
typedef struct {
	uint32 id;							// the ID of this region (as selected by the user, not necessarily unique)
	bool active;						// a flag indicating wheather this region is currently active or not
	irt_cap_block_usage_info* usage;	// a list of data block usage information maintained per accessed block
} irt_cap_region;


// ------------------------------------------------------------------------------------
//    Function Definitions
// ------------------------------------------------------------------------------------

// Block Handling:

/**
 * Registers a new data block within the system. The block is
 * starting at the given base and covering the given number of bytes.
 *
 * @param base the base pointer of the block to be registered
 * @param size the size of the block to be registered (in bytes)
 * @return the information internally created for the data block.
 */
const irt_cap_data_block_info* irt_cap_dbi_register_block(void* base, uint32 size);

/**
 * Obtains a pointer to the information stored for the data block referenced
 * by the given pointer. In case there is no such block, a null pointer will
 * be returned.
 *
 * @param ptr a pointer pointing on one of the elements of the requested block
 * @return the information maintained for the corresponding data block
 */
const irt_cap_data_block_info* irt_cap_dbi_lookup(void* ptr);

/**
 * Initializes the data block register. No request can be served before this
 * function has been invoked.
 */
void irt_cap_dbi_init();

/**
 * Finalizes the data block register upon shutdown. After this function
 * has been invoked, no more requests can be served.
 */
void irt_cap_dbi_finalize();


/**
 * Marks the beginning of a region within the code.
 *
 * @param id the user defined ID of the starting region. This ID is used to match the starting and stopping of regions only.
 */
void irt_cap_region_start(uint32 id);

/**
 * Marks the end of a region within the code.
 *
 * @param id the user defined ID of the ending region.
 */
void irt_cap_region_stop(uint32 id);

/**
 * Tags a data block with a given ID. This tag will only be valid for the innermost
 * active region and allows the data block to be identified when being reloaded within the
 * isolated code.
 *
 * @param pos a pointer to one of the elements within the data block
 * @param id the tag to be attached
 */
void irt_cap_tag_block(void* pos, uint16 id);

/**
 * Starting up this module.
 */
void irt_cap_region_init();

/**
 * Shuting down this module.
 */
void irt_cap_region_finalize();

/**
 * Informs the context capture module that the given value has been read.
 */
void irt_cap_read_value(void* pos, uint16 size);

/**
 * Informs the context capture module that the given value has been written.
 */
void irt_cap_written_value(void* pos, uint16 size);

/**
 * Informs the context capture module that the given pointer has been read.
 */
void irt_cap_read_pointer(void** pos);

/**
 * Informs the context capture module that the given pointer has been written.
 */
void irt_cap_written_pointer(void** pos);


// -- IO -----------------------------------

/**
 * This function should be used by the profiling run to save the collected information within a file.
 */
void irt_cap_profile_save();
