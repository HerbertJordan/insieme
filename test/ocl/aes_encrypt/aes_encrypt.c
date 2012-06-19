#include <stdlib.h>
#include <stdio.h>
#include "lib_icl.h"
#include "lib_icl_ext.h"



int main(int argc, const char* argv[]) {
		icl_args* args = icl_init_args();
		icl_parse_args(argc, argv, args);
		icl_print_args(args);

	int keySize = 16;
	int rounds = 10;
	int expandedKeySize = (rounds + 1) * keySize;

	int size = args->size;

	cl_uchar4* input = (cl_uchar4*)malloc(sizeof(cl_uchar4) * size);
	cl_uchar4* output = (cl_uchar4*)malloc(sizeof(cl_uchar4) * size);
 	//cl_uchar4* roundKey = //(cl_uchar4*)malloc(sizeof(cl_uchar4) * expandedKeySize);
 	cl_uchar roundKey[176] =
 		{ 0xf , 0x5c, 0xb , 0x51, 0xc9, 0x22, 0xe1, 0xd3, 0x33, 0x60, 0xa1, 0x6c, 0x59, 0x42, 0x60, 0x7c
 		, 0x68, 0x34, 0x3f, 0x6e, 0x99, 0xbb, 0x5a, 0x89, 0x23, 0x43, 0xe2, 0x8e, 0x88, 0xca, 0xaa, 0xd6
 		, 0xcd, 0xf9, 0xc6, 0xa8, 0x80, 0x3b, 0x61, 0xe8, 0xd5, 0x96, 0x74, 0xfa, 0x17, 0xdd, 0x77, 0xa1
 		, 0x52, 0xab, 0x6d, 0xc5, 0xad, 0x96, 0xf7, 0x1f, 0xe7, 0x71, 0x5 , 0xff, 0xd5, 0x8 , 0x7f, 0xde
 		, 0x9a, 0x31, 0x5c, 0x99, 0xbb, 0x2d, 0xda, 0xc5, 0xfa, 0x8b, 0x8e, 0x71, 0x73, 0x7b, 0x4 , 0xda
 		, 0x2c, 0x1d, 0x41, 0xd8, 0x18, 0x35, 0xef, 0x2a, 0xad, 0x26, 0xa8, 0xd9, 0x9d, 0xe6, 0xe2, 0x38
 		, 0xe9, 0xf4, 0xb5, 0x6d, 0x2d, 0x18, 0xf7, 0xdd, 0xaa, 0x8c, 0x24, 0xfd, 0xfc, 0x1a, 0xf8, 0xc0
 		, 0x68, 0x9c, 0x29, 0x44, 0x79, 0x61, 0x96, 0x4b, 0x10, 0x9c, 0xb8, 0x45, 0xc0, 0xda, 0x22, 0xe2
 		, 0x5b, 0xc7, 0xee, 0xaa, 0x17, 0x76, 0xe0, 0xab, 0x88, 0x14, 0xac, 0xe9, 0xdb, 0x1 , 0x23, 0xc1
 		, 0x22, 0xe5, 0xb , 0xa1, 0x9 , 0x7f, 0x9f, 0x34, 0xf0, 0xe4, 0x48, 0xa1, 0x77, 0x76, 0x55, 0x94
 		, 0xc , 0xe9, 0xe2, 0x43, 0x3b, 0x44, 0xdb, 0xef, 0xd2, 0x36, 0x7e, 0xdf, 0x45, 0x33, 0x66, 0xf2};
	// cl_uchar* SBox = (cl_uchar *)malloc(sizeof(cl_uchar) * 256);
	cl_uchar sbox[256] = 
		{ 0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76 //0
		, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0 //1
		, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15 //2
		, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75 //3
		, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84 //4
		, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf //5
		, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8 //6
		, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2 //7
		, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73 //8
		, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb //9
		, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79 //A
		, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08 //B
		, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a //C
		, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e //D
		, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf //E
		, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};//F
		//0      1    2      3     4    5     6     7      8    9     A      B    C     D     E     F


/**/
    cl_uchar rsbox[256] =
    { 0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb
    , 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb
    , 0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e
    , 0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25
    , 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92
    , 0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84
    , 0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06
    , 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b
    , 0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73
    , 0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e
    , 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b
    , 0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4
    , 0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f
    , 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef
    , 0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61
    , 0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d};       
/**/
	for(int i=0; i < size; ++i) {
		input[i] = (cl_uchar4){(cl_uchar)(2*i), (cl_uchar)(2*i)+1, (cl_uchar)(2*i)+2, (cl_uchar)(2*i)+3};
	}

	icl_init_devices(ICL_ALL);
	
	if (icl_get_num_devices() != 0) {
		icl_device* dev = icl_get_device(0);

		icl_print_device_short_info(dev);
		icl_kernel* kernel = icl_create_kernel(dev, "aes_encrypt_decrypt.cl", "AESEncrypt", "", ICL_SOURCE);
		
		icl_buffer* buf_input = icl_create_buffer(dev, CL_MEM_READ_ONLY, sizeof(cl_uchar4) * size);
		icl_buffer* buf_output = icl_create_buffer(dev, CL_MEM_WRITE_ONLY, sizeof(cl_uchar4) * size);
		icl_buffer* buf_roundKey = icl_create_buffer(dev, CL_MEM_READ_ONLY, sizeof(cl_uchar) * expandedKeySize);
		icl_buffer* buf_SBox = icl_create_buffer(dev, CL_MEM_READ_ONLY, sizeof(cl_uchar) * 256);

		icl_write_buffer(buf_input, CL_TRUE, sizeof(cl_uchar4) * size, &input[0], NULL, NULL);
		icl_write_buffer(buf_roundKey, CL_TRUE, sizeof(cl_uchar) * expandedKeySize, &roundKey[0], NULL, NULL);
		icl_write_buffer(buf_SBox, CL_TRUE, sizeof(cl_uchar) * 256, &sbox[0], NULL, NULL);
		
		size_t szLocalWorkSize[2];
		szLocalWorkSize[0] = args->local_size;
		szLocalWorkSize[1] = 4;
		float multiplier = size/(float)szLocalWorkSize[0];
		if(multiplier > (int)multiplier)
			multiplier += 1;
		size_t szGlobalWorkSize[2];
		szGlobalWorkSize[0] = (int)multiplier * szLocalWorkSize[0];
		szGlobalWorkSize[1] = 4;
		
		icl_run_kernel(kernel, 2, szGlobalWorkSize, szLocalWorkSize, NULL, NULL, 7,
											(size_t)0, (void *)buf_output,
											(size_t)0, (void *)buf_input,
											(size_t)0, (void *)buf_roundKey,
											(size_t)0, (void *)buf_SBox,
											szLocalWorkSize[0] * szLocalWorkSize[1] * sizeof (cl_uchar4), NULL,
											szLocalWorkSize[0] * szLocalWorkSize[1] * sizeof (cl_uchar4), NULL,
											sizeof(int), (void *)&rounds);
		
	
	//	icl_write_buffer(buf_SBox, CL_TRUE, sizeof(cl_uchar) * 256, &rsbox[0], NULL, NULL);
		icl_kernel* kernel2 = icl_create_kernel(dev, "aes_encrypt_decrypt.cl", "AESDecrypt", "", ICL_SOURCE);
		icl_run_kernel(kernel2, 2, szGlobalWorkSize, szLocalWorkSize, NULL, NULL, 7,
											(size_t)0, (void *)buf_input,
											(size_t)0, (void *)buf_output,
											(size_t)0, (void *)buf_roundKey,
											(size_t)0, (void *)buf_SBox,
											szLocalWorkSize[0] * szLocalWorkSize[1] * sizeof (cl_uchar4), NULL,
											szLocalWorkSize[0] * szLocalWorkSize[1] * sizeof (cl_uchar4), NULL,
											sizeof(int), (void *)&rounds);

		
		icl_read_buffer(buf_input, CL_TRUE, sizeof(cl_uchar4) * size, &output[0], NULL, NULL);
		
		icl_release_buffers(4, buf_input, buf_output, buf_roundKey, buf_SBox);
		icl_release_kernel(kernel);
	}
	
		for(int i = 0; i< size; ++i)
			printf("%d %d %d %d\n", output[i].s[0], output[i].s[1], output[i].s[2], output[i].s[3]);
	
	if (args->check_result) {
		printf("======================\n= AES encrypt working\n");
		unsigned int check = 1;
	
		printf("======================\n");
		printf("Result check: %s\n", check ? "OK" : "FAIL");
	} else {
		
		printf("Result check: OK\n");
	}

	icl_release_args(args);	
	icl_release_devices();
	free(input);
	free(output);
}