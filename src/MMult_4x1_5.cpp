#include <iostream>

static void AddDot4x1(int k, float *a, int m, float *b, int n, float *c);

void MMult_4x1_5(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//列乘行, 得到k个矩阵。 K个矩阵相加得到C

	for (int i = 0; i < m; i+=4) {
		for (int j = 0; j < n; ++j) {
			AddDot4x1(k, &A[i*k], m, &B[j], n, &C[i*n + j]);
		}
	}
}

static void AddDot4x1(int k, float *a, int m, float *b, int n, float *c)
{
	for (int p = 0; p < k; p++) {
		c[0] += a[p] * b[p * n];
		c[n] += a[k + p] * b[p * n];
		c[2 * n] += a[2*k + p] * b[p * n];
		c[3 * n] += a[3*k + p] * b[p * n];
	}
}
