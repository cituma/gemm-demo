#include <iostream>

static void AddDot(int k, float *x, float *y, int n, float *gamma);

void MMult1(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//�г���, �õ�k������ K��������ӵõ�C

	for (int i = 0; i < m; i+=4) {
		for (int j = 0; j < n; ++j) {
			//ÿ�μ���C��4x1��ֵ
			AddDot(k, &A[i*k], &B[j], n, &C[i*n + j]);
			AddDot(k, &A[(i+1)*k], &B[j], n, &C[(i+1)*n + j]);
			AddDot(k, &A[(i+2)*k], &B[j], n, &C[(i+2)*n + j]);
			AddDot(k, &A[(i+3)*k], &B[j], n, &C[(i+3)*n + j]);
		}
	}
}

static void AddDot(int k, float *x, float *y, int n, float *gamma)
{
	for (int p = 0; p < k; p++) {
		*gamma += x[p] * y[p * n];
	}
}
