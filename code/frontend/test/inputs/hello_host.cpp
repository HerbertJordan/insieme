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

#include "CL/cl.h"
//#include "/home/klaus/NVIDIA_GPU_Computing_SDK/OpenCL/common/inc/oclUtils.h"

//#pragma insieme mark
int main(int argc, char **argv)
{
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_int err;

    cl_mem dev_ptr1;// = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_float) * 100, NULL, &err);
//    cl_mem dev_ptr2 = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_double) * 100, NULL, &err);
    float* host_ptr;


    dev_ptr1 = clCreateBuffer(context, CL_MEM_READ_ONLY, 100 * sizeof(cl_float), NULL, &err);

    clEnqueueWriteBuffer(queue, dev_ptr1, CL_TRUE, 0, sizeof(cl_float) * 100, host_ptr, 0, NULL, NULL);

    size_t kernelLength = 10;
/*    char* kernelSrc = oclLoadProgSource("/home/klaus/insieme/code/frontend/test/hello.cl", "", &kernelLength);

    kernel = clCreateKernel(program, "hello", &err);
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&dev_ptr1);

    size_t globalSize[] = {512};
    size_t localSize[] = {32};

    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, globalSize, localSize, 0, NULL, NULL);

    clEnqueueReadBuffer(queue, dev_ptr1, CL_TRUE, 0,  sizeof(cl_float) * 100, host_ptr, 0, NULL, NULL);
*/
    clReleaseMemObject(dev_ptr1);
//    clReleaseMemObject(dev_ptr2);

    return 0;
}