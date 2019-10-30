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

static void kernel_4x4_v2(int m, int n, int k,
	float* sa, float * sb, float* sc, int ldc);
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


/**

float* a: A
float* b: (B)T
float* c: C

C = A * (B)T

A1 A2 A3    B1 B4 B7
A4 A5 A6  x B2 B5 B8 => C1 C4 C7 C2 C5 C8 C3 C6 C9 (packed)
A7 A8 A9    B3 B4 B9

Calculation sequence:
1st. calculate C1
2st. calculate C4
3st. calculate C7
...
9st. calculate C9

A1-A9/B1-B9 is packed block, not single number.
C1-C9 is 4x4 block, not single number.

Output
C1 C2 C3
C4 C5 C6
C7 C8 C9

 */
static void kernel_4x4_v2(int m, int n, int k,
	float* sa, float * sb, float* sc, int ldc) {
}

/*
在宽为lda的A矩阵的m*k的区域内, 将内存按以下方式重排(pack)
Input:
... 0 1 2 3  4 5 6 7 ...
... 0 1 2 3  4 5 6 7 ...
... 0 1 2 3  4 5 6 7 ...
... 0 1 2 3  4 5 6 7 ...

... 8 9 a b  c d e f ...
... 8 9 a b  c d e f ...
... 8 9 a b  c d e f ...
... 8 9 a b  c d e f ...

Pack it zigzag

Output:
0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3
4 4 4 4 5 5 5 5 6 6 6 6 7 7 7 7
8 8 8 8 9 9 9 9 a a a a b b b b
c c c c d d d d e e e e f f f f

*/
static void packA_4(int m, int k, float* from, int lda, float* to) {
	//assert( k != 0 && m != 0 && k % 4 == 0 && m % 4 == 0);

	float* a_offset = from;
	float* b_offset = to;

	int j = (m >> 2);
	do {
		float* a_offset0 = a_offset;
		float* a_offset1 = a_offset0 + lda;
		float* a_offset2 = a_offset1 + lda;
		float* a_offset3 = a_offset2 + lda;
		a_offset += 4 * lda;

		int i = (k >> 2);
		do {
			float c_temp00 = *(a_offset0 + 0);
			float c_temp01 = *(a_offset0 + 1);
			float c_temp02 = *(a_offset0 + 2);
			float c_temp03 = *(a_offset0 + 3);

			float c_temp10 = *(a_offset1 + 0);
			float c_temp11 = *(a_offset1 + 1);
			float c_temp12 = *(a_offset1 + 2);
			float c_temp13 = *(a_offset1 + 3);

			float c_temp20 = *(a_offset2 + 0);
			float c_temp21 = *(a_offset2 + 1);
			float c_temp22 = *(a_offset2 + 2);
			float c_temp23 = *(a_offset2 + 3);

			float c_temp30 = *(a_offset3 + 0);
			float c_temp31 = *(a_offset3 + 1);
			float c_temp32 = *(a_offset3 + 2);
			float c_temp33 = *(a_offset3 + 3);

			*(b_offset + 0) = c_temp00;
			*(b_offset + 1) = c_temp10;
			*(b_offset + 2) = c_temp20;
			*(b_offset + 3) = c_temp30;

			*(b_offset + 4) = c_temp01;
			*(b_offset + 5) = c_temp11;
			*(b_offset + 6) = c_temp21;
			*(b_offset + 7) = c_temp31;

			*(b_offset + 8) = c_temp02;
			*(b_offset + 9) = c_temp12;
			*(b_offset + 10) = c_temp22;
			*(b_offset + 11) = c_temp32;

			*(b_offset + 12) = c_temp03;
			*(b_offset + 13) = c_temp13;
			*(b_offset + 14) = c_temp23;
			*(b_offset + 15) = c_temp33;

			a_offset0 += 4;
			a_offset1 += 4;
			a_offset2 += 4;
			a_offset3 += 4;

            b_offset += 16;
		} while (--i > 0);
	}while(--j>0);
}

/*
在宽为ldb的B矩阵的k*n的区域内, 将内存按以下方式重排(pack)

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

	float* a_offset = from;
	float* b_offset = to;

	int j = (k >> 2);
	do {
		float* a_offset0 = a_offset;
		float* a_offset1 = a_offset0 + ldb;
		float* a_offset2 = a_offset1 + ldb;
		float* a_offset3 = a_offset2 + ldb;
		a_offset += 4 * ldb;

		float* b_offset0 = b_offset;
		b_offset += 16;

		int i = (n >> 2);
		do {
			float c_temp00 = *(a_offset0 + 0);
			float c_temp01 = *(a_offset0 + 1);
			float c_temp02 = *(a_offset0 + 2);
			float c_temp03 = *(a_offset0 + 3);

			float c_temp10 = *(a_offset1 + 0);
			float c_temp11 = *(a_offset1 + 1);
			float c_temp12 = *(a_offset1 + 2);
			float c_temp13 = *(a_offset1 + 3);

			float c_temp20 = *(a_offset2 + 0);
			float c_temp21 = *(a_offset2 + 1);
			float c_temp22 = *(a_offset2 + 2);
			float c_temp23 = *(a_offset2 + 3);

			float c_temp30 = *(a_offset3 + 0);
			float c_temp31 = *(a_offset3 + 1);
			float c_temp32 = *(a_offset3 + 2);
			float c_temp33 = *(a_offset3 + 3);

			a_offset0 += 4;
			a_offset1 += 4;
			a_offset2 += 4;
			a_offset3 += 4;

			*(b_offset0 + 0) = c_temp00;
			*(b_offset0 + 1) = c_temp01;
			*(b_offset0 + 2) = c_temp02;
			*(b_offset0 + 3) = c_temp03;

			*(b_offset0 + 4) = c_temp10;
			*(b_offset0 + 5) = c_temp11;
			*(b_offset0 + 6) = c_temp12;
			*(b_offset0 + 7) = c_temp13;

			*(b_offset0 + 8) = c_temp20;
			*(b_offset0 + 9) = c_temp21;
			*(b_offset0 + 10) = c_temp22;
			*(b_offset0 + 11) = c_temp23;

			*(b_offset0 + 12) = c_temp30;
			*(b_offset0 + 13) = c_temp31;
			*(b_offset0 + 14) = c_temp32;
			*(b_offset0 + 15) = c_temp33;

			b_offset += k*4;	//处理同一个4行中，其他列的数据
		} while (--i > 0);
	} while (--j > 0);
}
