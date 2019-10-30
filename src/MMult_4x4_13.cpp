#include <iostream>
#include <vector>
#include <stdlib.h>

#define A(i,j) a[ (i)*wa + (j) ]
#define B(i,j) b[ (i)*wb + (j) ]
#define C(i,j) c[ (i)*wc + (j) ]

/* Block sizes */
#define nc 128
#define kc 128

#define min( i, j ) ( (i)<(j) ? (i): (j) )

#define GEMM_N (240)  // GEMM_R
#define GEMM_M (240)  // GEMM_P
#define GEMM_K (240)  // GEMM_Q
#define GEMM_UNROLL (4)
#define KERNEL_4x4  kernel_4x4_v2

static void AddDot4x4(int k, float *a, int wa, float *b, int wb, float *c, int wc);
static void InnerKernel(int m, int n, int k, float* a, int lda,
	float* b, int ldb,
	float* c, int ldc);

static void packA_4(int m, int k, float* from, int lda, float* to);
static void packB_4(int k, int n, float* from, int ldb, float* to);

float* fastMalloc(int size) {
#ifdef _WIN32
	void* ptr = _aligned_malloc(size * sizeof(float), 64);
	return static_cast<float*>(ptr);
#else
	void* ptr = 0;
	int iRet = posix_memalign(&ptr, 64, size * sizeof(float));
	assert(0 == iRet);
	return ptr;
#endif
}

void MMult_4x4_13(float* A, float* B, float* C, int m, int n, int k) {
	int lda = k;
	int ldb = n;
	int ldc = n;
	//A: m*k矩阵
	//B: k*n矩阵
	//C: C=AxB; m*n矩阵
	float* sa = fastMalloc(m * k);
	float* sb = fastMalloc(k * n);
	
	for (int ms = 0; ms < m; ms += GEMM_M) {
		int min_m = m - ms;
		if (min_m > GEMM_M) {
			min_m = GEMM_M;
		}

		int min_k = 1;
		for (int ks = 0; ks < k; ks += min_k) {
			min_k = k - ks;
			if (min_k >= (GEMM_K << 1)) {
				min_k = GEMM_K;
			}
			else if (min_k > GEMM_K) {
				min_k = (min_k / 2 + GEMM_UNROLL - 1) & ~(GEMM_UNROLL - 1);
			}

			//首先 packB
			int min_n = n;
			if (n >= GEMM_N * 2) {
				min_n = GEMM_N;
			}
			else if (n > GEMM_N) {
				min_n = (min_n / 2 + GEMM_UNROLL - 1) & ~(GEMM_UNROLL - 1);
			}
			packB_4(min_k, min_n, B + ks * ldb, ldb, sb);

		}
	}
}

/*
将A内存按以下方式重排(pack)
Input:
0 1 2 3  4 5 6 7
0 1 2 3  4 5 6 7
0 1 2 3  4 5 6 7
0 1 2 3  4 5 6 7

8 9 a b  c d e f
8 9 a b  c d e f
8 9 a b  c d e f
8 9 a b  c d e f

Pack it zigzag

Output:
0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3
4 4 4 4 5 5 5 5 6 6 6 6 7 7 7 7
8 8 8 8 9 9 9 9 a a a a b b b b
c c c c d d d d e e e e f f f f

*/
static void packA_4(int m, int k, float* from, int lda, float* to) {

}

/*
将B内存按以下方式重排(pack)

Input:
0 1 2 3  4 5 6 7
0 1 2 3  4 5 6 7
0 1 2 3  4 5 6 7
0 1 2 3  4 5 6 7

8 9 a b  c d e f
8 9 a b  c d e f
8 9 a b  c d e f
8 9 a b  c d e f

Pack it zigzag, not like pack A

Output:
0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3
8 9 a b 8 9 a b 8 9 a b 8 9 a b
4 5 6 7 4 5 6 7 4 5 6 7 4 5 6 7
c d e f c d e f c d e f c d e f

*/
static void packB_4(int k, int n, float* from, int ldb, float* to) {

}