#include <stdio.h>
#include <omp.h>
#include <unistd.h>

int flag;

#define IMAX 1000000000ll

int main() {
	long long i = 0;
	#pragma omp parallel
	{
		if(omp_get_thread_num() == 0) {
			usleep(10);
			flag = 1;
		}
		while(flag == 0 && i < IMAX) {
			#pragma omp flush(flag)
			++i;
		}
	}
	// successful if we reach this point at all
	if(i < IMAX) {
		printf("Success!\n");
	} else {
		printf("Fail!\n");
	}		
}
