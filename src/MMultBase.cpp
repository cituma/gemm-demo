#include <iostream>

static void AddDot(int k, float *x, float *y, int n, float *gamma);

void MMultBase(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//普通做法, 行乘列
	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < n; ++j) {
			C[i*n + j] = 0.f;
			AddDot(k, &A[i*k], &B[j], n, &C[i*n + j]);
		}
	}
}

static void AddDot(int k, float *x, float *y, int n, float *gamma)
{
	for (int p = 0; p < k; p++) {
		*gamma += x[p] * y[p * n];
	}
}
