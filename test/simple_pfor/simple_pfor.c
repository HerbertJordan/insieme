extern void printf(const char*, ...);

#define N 100

int main() {

	int i;
	int a[N];

	#pragma omp parallel
	{
		#pragma omp for
		for (i=0; i<N; i++) {
			a[i] = i;
		}
	}

	printf("After Loop: %d\n", i);

	for (i=0; i<N; i++) {
		printf("a[%d]=%d\n", i, a[i]);
	}
}
