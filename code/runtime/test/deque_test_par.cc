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

#include <gtest/gtest.h>
#include <pthread.h>
#include <omp.h>

#include <utils/deques.h>

#include <impl/error_handling.impl.h>

// horrible hack incoming
uint32 irt_g_error_key = 0;

#define TEST_ELEMS 777
#define PARALLEL_ITERATIONS 100

typedef struct _irt_deque_test {
	float data;
	struct _irt_deque_test* next_q;
	struct _irt_deque_test* prev_q;
} irt_deque_test;

IRT_DECLARE_DEQUE(deque_test);
IRT_DEFINE_DEQUE(deque_test, next_q, prev_q);

irt_deque_test* make_item(float val) {
	irt_deque_test* item = (irt_deque_test*)calloc(1,sizeof(irt_deque_test));
	item->data = val;
	return item;
}

TEST(deques, mass_parallel_ops) {
	for(int j=0; j<PARALLEL_ITERATIONS; ++j) {
		irt_deque_test_deque q;
		irt_deque_test_deque_init(&q);

		irt_deque_test* elems[TEST_ELEMS];

		#pragma omp parallel
		{
			#pragma omp for schedule(dynamic,1)
			for(int i=0; i<TEST_ELEMS; ++i) {
				elems[i] = make_item(i/10.0f);
				irt_deque_test_deque_insert_back(&q, elems[i]);
			}
			#pragma omp for schedule(dynamic,1)
			for(int i=0; i<TEST_ELEMS; ++i) {
				irt_deque_test_deque_pop_back(&q);
			}

			EXPECT_EQ(0 /* NULL */, q.start);
			EXPECT_EQ(0 /* NULL */, q.end);
		}

		// cleanup
		irt_deque_test_deque_cleanup(&q);
		for(int i=0; i<TEST_ELEMS; ++i) {
			free(elems[i]);
		}
	}
}