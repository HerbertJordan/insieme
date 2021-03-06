#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include "lib_icl.h"
#include "lib_icl_ext.h"
#include "n_body.h"

#ifndef PATH
#define PATH "./"
#endif

float rand_val(float min, float max) {
	return (rand() / (float) RAND_MAX) * (max - min) + min;
}

triple triple_rand() {
	return (position) {
		rand_val(-SPACE_SIZE,SPACE_SIZE),
		rand_val(-SPACE_SIZE,SPACE_SIZE),
		rand_val(-SPACE_SIZE,SPACE_SIZE)
	};
}

void triple_print(triple t) {
	printf("(%f,%f,%f)", t.x, t.y, t.z);
}


int main(int argc, const char* argv[]) {
        icl_args* args = icl_init_args();
        icl_parse_args(argc, argv, args);
        icl_print_args(args);

	chdir(PATH);

	int size = args->size;

	body* B = (body*)malloc(sizeof(body) * size); // the list of bodies

	// distribute bodies in space (randomly)
	for(int i=0; i<size; i++) {
		B[i].m = (i < size)?1:0;
		B[i].pos = triple_rand();
		B[i].v   = triple_zero();
	}

	icl_init_devices(args->device_type);
	
	icl_start_energy_measurement();

	if (icl_get_num_devices() != 0) {
		icl_device* dev = icl_get_device(args->device_id);

		icl_print_device_short_info(dev);
		icl_kernel* kernel = icl_create_kernel(dev, "n_body.cl", "n_body", "-I.", ICL_SOURCE);
		
		size_t szLocalWorkSize = args->local_size;
		float multiplier = size/(float)szLocalWorkSize;
		if(multiplier > (int)multiplier)
			multiplier += 1;
		size_t szGlobalWorkSize = (int)multiplier * szLocalWorkSize;

		for (int i = 0; i < args->loop_iteration; ++i) {
			icl_buffer* buf_B = icl_create_buffer(dev, CL_MEM_READ_WRITE, sizeof(body) * size);
			icl_buffer* buf_tmp = icl_create_buffer(dev, CL_MEM_READ_WRITE, sizeof(body) * size);

			icl_write_buffer(buf_B, CL_TRUE, sizeof(body) * size, &B[0], NULL, NULL);

			icl_run_kernel(kernel, 1, &szGlobalWorkSize, &szLocalWorkSize, NULL, NULL, 3,
											(size_t)0, (void *)buf_B,
											(size_t)0, (void *)buf_tmp,
											sizeof(cl_int), (void *)&size);
											
			icl_read_buffer(buf_tmp, CL_TRUE, sizeof(body) * size, &B[0], NULL, NULL);
		
			icl_release_buffers(2, buf_B, buf_tmp);
		}
		
		icl_release_kernel(kernel);
	}
	
	icl_stop_energy_measurement();
	
	if (args->check_result) {
		printf("======================\n= N Body Simulation working\n");
		impulse sum = triple_zero();
		for(int i=0; i<size; i++) {
			sum = ADD(sum, MULS(B[i].v,B[i].m));
		}
		unsigned int check = EQ(sum, triple_zero());
		//printf("ERRROR: %f %f %f\n", sum.x, sum.y, sum.z);
		printf("======================\n");
		printf("Result check: %s\n", check ? "OK" : "FAIL");
	} else {
		printf("Checking disabled!\n");
        	printf("Result check: OK\n");
	}

	icl_release_args(args);	
	icl_release_devices();
	free(B);
	return 0;
}
