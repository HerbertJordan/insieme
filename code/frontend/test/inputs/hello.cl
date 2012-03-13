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

#include "ocl_device.h"
#pragma insieme mark
__kernel void hello(__global short *src, __global float4 *dst, __local float *l, int factor){
#pragma insieme datarange (dst = __insieme_ocl_globalId : __insieme_ocl_globalId), \
	                      (src = __insieme_ocl_globalId : __insieme_ocl_globalId), \
	                      (l = 0 : __insieme_ocl_globalSize)
{
	float4 a = (float4)(0.0);
	float4* b = (float4*)src;
	b = (float4*)src ;
	float f = 7.0f;
	float4 c = native_divide(a, b[3]);
	short t[5];
	short* x = t + 7lu;

	char4 d = convert_char4(a); 
	a = convert_float4(d);
	
	dst[0] = a - c;
	dst[1] = b[1] / c.x;
	dst[2] = (float)src[0] + b[0];
	dst[3] = 5.0f + c;
	dst[4] = c * (float)factor;
	int i = get_global_id(0);
	dst[i].x += src[i] * factor;
}}
