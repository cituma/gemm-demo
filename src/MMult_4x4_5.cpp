#include <iostream>


#define A(i,j) a[ (i)*wa + (j) ]
#define B(i,j) b[ (i)*wb + (j) ]
#define C(i,j) c[ (i)*wc + (j) ]

static void AddDot4x4(int k, float *a, int wa, float *b, int wb, float *c, int wc);

void MMult_4x4_5(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//列乘行, 得到k个矩阵。 K个矩阵相加得到C

	for (int i = 0; i < m; i+=4) {
		for (int j = 0; j < n; j+=4) {
			AddDot4x4(k, &A[i*k], k, &B[j], n, &C[i*n + j], n);
		}
	}
}

static void AddDot4x4(int k, float *a, int wa, float *b, int wb, float *c, int wc)
{
	for (int p = 0; p < k; ++p) {
		//第0列
		C(0, 0) += A(0, p) * B(p, 0);
		C(1, 0) += A(1, p) * B(p, 0);
		C(2, 0) += A(2, p) * B(p, 0);
		C(3, 0) += A(3, p) * B(p, 0);

		//第1列
		C(0, 1) += A(0, p) * B(p, 1);
		C(1, 1) += A(1, p) * B(p, 1);
		C(2, 1) += A(2, p) * B(p, 1);
		C(3, 1) += A(3, p) * B(p, 1);

		//第2列
		C(0, 2) += A(0, p) * B(p, 2);
		C(1, 2) += A(1, p) * B(p, 2);
		C(2, 2) += A(2, p) * B(p, 2);
		C(3, 2) += A(3, p) * B(p, 2);

		//第3列
		C(0, 3) += A(0, p) * B(p, 3);
		C(1, 3) += A(1, p) * B(p, 3);
		C(2, 3) += A(2, p) * B(p, 3);
		C(3, 3) += A(3, p) * B(p, 3);
	}
}
