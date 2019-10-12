#include <iostream>

static void AddDot1x4(int k, float *a, float *b, int n, float *c);

void MMult_1x4_5(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//列乘行, 得到k个矩阵。 K个矩阵相加得到C

	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < n; j+=4) {
			AddDot1x4(k, &A[i*k], &B[j], n, &C[i*n + j]);
		}
	}
}

static void AddDot1x4(int k, float *a, float *b, int n, float *c)
{
	for (int p = 0; p < k; p++) {
		c[0] += a[p] * b[p*n];
		c[1] += a[p] * b[p*n + 1];
		c[2] += a[p] * b[p*n + 2];
		c[3] += a[p] * b[p*n + 3];
	}
}
