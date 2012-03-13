
#include <stdio.h>

#define N 1000

#define MIN(X,Y) ((X)<(Y)?(X):(Y))
#define MAX(X,Y) ((X)>(Y)?(X):(Y))

#define VALUE double

// create the matices
VALUE A[N][N];

VALUE eq(double x, double y) {
	VALUE diff = x - y;
	diff = (diff<0)?-diff:diff;
	return diff < 0.000001;
}

int main() {

	// initialize the matices

	#pragma omp parallel
	{

		// fill A with data
               
		#pragma omp for
		for (int i=0; i<N; i++) {
			for (int j=0; j<N; j++) {
				A[i][j] = (i==j)?1:0;
				A[i][j] = 1.0/((i+j+1));
			}
		}

		// compute LU decomposition
		#pragma insieme tile(4,4,4)
		for(int i=0; i<N; i++) {
			#pragma omp for
			for(int j=i+1; j<N; j++) {
				// compute scaling factor of current row (and save it in L part of matrix)
				A[j][i] = A[j][i] / A[i][i];
				for(int k=i+1; k<N; k++) {
					// compute R part of matrix
					A[j][k] -= A[j][i] * A[i][k];
				}
			}
		}

	}

	// verify result (samples)
	int success = 1;
	for(int i=0; i<MIN(10,N); i++) {
		for(int j=0; j<MIN(10,N); j++) {
			VALUE sum = 0;
			for(int k=0; k<N; k++) {
				VALUE a = (i==k)?1:((i>k)?A[i][k]:0);
				VALUE b = (k<=j)?A[k][j]:0;
				sum += a * b;
			}
			if (!eq(sum, 1.0/(i+j+1))) {
				printf("%f - %f\n", sum, 1.0/(i+j+1));
				success = 0;
			}
		}
	}

	// print verification result
	printf("Verification: %s\n", (success)?"OK":"ERR");
	//return success;
}
