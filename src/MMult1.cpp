#include <iostream>

static void AddDot(int k, float *x, float *y, int n, float *gamma);

void MMult1(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//列乘行, 得到k个矩阵。 K个矩阵相加得到C

	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < n; j += 4) {
			AddDot(k, &A[i*k], &B[j], n, &C[i*n + j]);
			AddDot(k, &A[i*k], &B[j+1], n, &C[i*n + j + 1]);
			AddDot(k, &A[i*k], &B[j+2], n, &C[i*n + j + 2]);
			AddDot(k, &A[i*k], &B[j+3], n, &C[i*n + j + 3]);
		}
	}
}

static void AddDot(int k, float *x, float *y, int n, float *gamma)
{
	for (int p = 0; p < k; p++) {
		*gamma += x[p] * y[p * n];
	}
}
