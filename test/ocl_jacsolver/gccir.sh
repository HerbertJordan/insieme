gcc out.c -D_POSIX_C_SOURCE=199309 ../../code/backend/test/ocl_kernel/lib_icl.c -I$OPENCL_ROOT/include -I../../code/backend/test/ocl_kernel  -I../../code/frontend/test/inputs -I../../code/runtime/include --std=c99 -I. -D_XOPEN_SOURCE=700 -DUSE_OPENCL=ON -D_GNU_SOURCE -L$OPENCL_ROOT/lib/x86_64 -lOpenCL -lm -lpthread -ldl -lrt -o ijs -g