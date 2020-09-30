#include <iostream>
#include <vector>
#include <stdlib.h>
#include <assert.h>

#ifndef __ANDROID__

#define A(i,j) a[ (i)*wa + (j) ]
#define B(i,j) b[ (i)*wb + (j) ]
#define C(i,j) c[ (i)*wc + (j) ]

#define min( i, j ) ( (i)<(j) ? (i): (j) )

#define GEMM_N (384)  // GEMM_R
#define GEMM_M (1024)  // GEMM_P
#define GEMM_K (256)  // GEMM_Q
#define GEMM_UNROLL (4)
#define KERNEL_4x4  kernel_4x4_v2

#include <mmintrin.h>
#include <xmmintrin.h>  // SSE
#include <pmmintrin.h>  // SSE2
#include <emmintrin.h>  // SSE3

typedef union {
	__m128	v;		//__m128单精度浮点, __m128i整型, __m128d双精度
	float	f[4];
} v4f_t;

static void kernel_4x4_v2(int m, int n, int k,
	float* sa, float * sb, float* sc, int ldc);
static void packA_4(int m, int k, float* from, int lda, float* to);
static void packB_4(int k, int n, float* from, int ldb, float* to);

static float* fastMalloc(int size) {
#ifdef _WIN32
	void* ptr = _aligned_malloc(size * sizeof(float), 64);
	return static_cast<float*>(ptr);
#else
	void* ptr = 0;
	int iRet = posix_memalign(&ptr, 64, size * sizeof(float));
	assert(0 == iRet);
	return static_cast<float*>(ptr);
#endif
}

static void fastFree(void* ptr) {
#ifdef _WIN32
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}

/*
对于行主序, 理想的GEPB(A,B,C)条件
    前 3 个前提不考虑 TLB，假设只有 内存、cache 和 ALU ：
	1) kc * nc 要小，小到 " (A的min_mm行) + (B的block) + (C的min_mm行) "能够一起塞进 L1 cache
	2) 如果 1 被满足，CPU 计算时不再受内存速度的限制，即得到的gflops值就是真实的计算能力
	3) B 的block只会被加载进 Cache 一次，gemm过程中不会被换入又换出
    后 2 个要考虑 TLB，因为 TLB miss 会 stall CPU：
	4) kc 和 nc 要小，小到 "(A的min_mm行) + (B的block) + (C的min_mm行) " 能够被 TLB 索引.
    5) B的block只被加载到 TLB 一次. gemm过程不会换入换出.

所以有以下参数限制:
	1) kc ~= nc //用一半的
    2) min_mm >= (Rcomp/(2*Rload)) 其中 Rcomp 是算力、Rload 是 L2 cache 到 register 的带宽. 也就是说将B从L2cache中取出的时间不超过前一个元素计算时间。
    3) kc * nc <= K
    4) kc * nc 只能占cache一半.
*/
void MMult_4x4_14(float* A, float* B, float* C, int m, int n, int k) {
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
		//min_m 在4到GEMM_M间, 除了最后一个循环，其他值都为GEMM_M
		if (min_m > GEMM_M) {
			min_m = GEMM_M;
		}

		int min_k = 1;
        //论文Fig.4中, GEMM_VAR1操作
		for (int ks = 0; ks < k; ks += min_k) {
			min_k = k - ks;
			//min_k 在4到GEMM_K之间, 除了最后一个循环，其他值都为GEMM_K
			if (min_k >= (GEMM_K << 1)) {
				min_k = GEMM_K;
			}
			else if (min_k > GEMM_K) {
				//min_k = (((min_k / 2) + 3) / 4) * 4;
				min_k = (min_k / 2 + GEMM_UNROLL - 1) & ~(GEMM_UNROLL - 1);	//(min_k / 2)向上与GEMM_UNROLL对齐
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

			// micro kernel, split A Block to smaller Panel
			int min_mm = 0;
            //A中. 高宽为 min_m*min_k 的 panel 中, 分为高宽为 min_mm*min_k 的小块.
            //这些小块分别乘以B中高宽为 min_k*min_n 的block(这是GEMM的最小操作).
            //矩阵乘并拼接后, 得到C中min_m * min_n 的panel.
            //论文Fig.4中, GEPP_VAR2分解后, GEPB操作.
			for (int mms = ms; mms < ms + min_m; mms += min_mm) {
				min_mm = (ms + min_m) - mms;
				if (min_mm >= 3 * GEMM_UNROLL) {
					min_mm = 3 * GEMM_UNROLL;
				}
				else if (min_mm >= 2 * GEMM_UNROLL) {
					min_mm = 2 * GEMM_UNROLL;
				}
				else if (min_mm > GEMM_UNROLL) {
					min_mm = GEMM_UNROLL;
				}

				// coninueous packA
				float* sa_addr = sa + min_k * (mms - ms);
				packA_4(min_mm, min_k, A + mms * lda + ks, lda, sa_addr);

                //Fig.4 GEPB操作的最小操作单元
				KERNEL_4x4(min_mm, min_n, min_k, sa_addr, sb, C + mms * ldc, ldc);
			}

            //论文Fig.4中, GEPP_VAR2后的其它block.
			// the first B Block has been packed, proc the others
			for (int ns = min_n; ns < n; ns += min_n) {
				min_n = n - ns;
				if (min_n >= GEMM_N * 2) {
					min_n = GEMM_N;
				}
				else if (min_n > GEMM_N) {
					min_n = (min_n / 2 + GEMM_UNROLL - 1) & ~(GEMM_UNROLL - 1);
				}

				packB_4(min_k, min_n, B + ns + ks * ldb, ldb, sb);
				KERNEL_4x4(min_m, min_n, min_k, sa, sb, C + ms * ldc + ns, ldc);
			}
		}
	}
	fastFree(sa);
	fastFree(sb);
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
/*
GEPB操作:
mc-kc和kc-nc矩阵乘. 除了最后一块, 其它部分mc==GEMM_M, kc==GEMM_K, nc==GEMM_N
*/
static void kernel_4x4_v2(int mc, int nc, int kc,
	float* sa, float * sb, float* sc, int ldc) {
	//assert(m > 0 && n > 0 && k > 0);
	//assert(m % 4 == 0 && n % 4 == 0 && k % 4 == 0);

	float* a = sa;
	float* b = sb;
	float* c = sc;

	v4f_t
		v0, v1, v2, v3,
		v16_0, v16_1, v16_2, v16_3,
		v17_0, v17_1, v17_2, v17_3,
		v18_0, v18_1, v18_2, v18_3,
		v19_0, v19_1, v19_2, v19_3,
		v24, v25, v26, v27;
    //b应该放在L2cache中. 可以通过限制kc*nc大小,
	for (int i = 0; i < mc; i += 4) {
		//a按每4行分panel
		for (int j = 0; j < nc; j += 4) {
            //在此循环中的a会重复使用，应该放在L1cache中. 可以通过限制kc大小, 使a大小(4*kc)为L1 cache的一半.

			//b按每4列分panel

			v24.v = _mm_setzero_ps();
			v25.v = _mm_setzero_ps();
			v26.v = _mm_setzero_ps();
			v27.v = _mm_setzero_ps();
			//4xk 和 kx4 两个矩阵相乘,累加得到4x4矩阵
            //v24-v27总共16个float, 应该存放在寄存器中
			for (int l = 0; l < kc; l += 4) {
				/*
				A, B矩阵已按pack_A和pack_B的方式重排.
				A(pack前):
				0 1 2 3  4 5 6 7 ...
				0 1 2 3  4 5 6 7 ...
				0 1 2 3  4 5 6 7 ...
				0 1 2 3  4 5 6 7 ...

				B(pack前):
				0 1 2 3
				0 1 2 3
				0 1 2 3
				0 1 2 3
				8 9 a b
				8 9 a b
				8 9 a b
				8 9 a b
				...

				用A的l列乘B的l行, (l+1)列乘(l+1)行,(l+2)列乘(l+2)行,(l+3)列乘(l+3)行
				得到4个4x4矩阵并累加.
				最后把k/4个4x4矩阵累加.
				*/

				//第1列乘第1行
				//v24,v25,v26,v27组成一个4x4的矩阵
				v0.v = _mm_load_ps(b);
				v16_0.v = _mm_load1_ps(a++);		//取*a, 拷贝四次
				v16_1.v = _mm_load1_ps(a++);
				v16_2.v = _mm_load1_ps(a++);
				v16_3.v = _mm_load1_ps(a++);
				v24.v = _mm_add_ps(v24.v, _mm_mul_ps(v0.v, v16_0.v));
				v25.v = _mm_add_ps(v25.v, _mm_mul_ps(v0.v, v16_1.v));
				v26.v = _mm_add_ps(v26.v, _mm_mul_ps(v0.v, v16_2.v));
				v27.v = _mm_add_ps(v27.v, _mm_mul_ps(v0.v, v16_3.v));

				//第2列乘第2行
				v1.v = _mm_load_ps(b + 4);
				v17_0.v = _mm_load1_ps(a++);
				v17_1.v = _mm_load1_ps(a++);
				v17_2.v = _mm_load1_ps(a++);
				v17_3.v = _mm_load1_ps(a++);
				v24.v = _mm_add_ps(v24.v, _mm_mul_ps(v1.v, v17_0.v));
				v25.v = _mm_add_ps(v25.v, _mm_mul_ps(v1.v, v17_1.v));
				v26.v = _mm_add_ps(v26.v, _mm_mul_ps(v1.v, v17_2.v));
				v27.v = _mm_add_ps(v27.v, _mm_mul_ps(v1.v, v17_3.v));

				//第3列乘第3行
				v2.v = _mm_load_ps(b + 8);
				v18_0.v = _mm_load1_ps(a++);
				v18_1.v = _mm_load1_ps(a++);
				v18_2.v = _mm_load1_ps(a++);
				v18_3.v = _mm_load1_ps(a++);
				v24.v = _mm_add_ps(v24.v, _mm_mul_ps(v2.v, v18_0.v));
				v25.v = _mm_add_ps(v25.v, _mm_mul_ps(v2.v, v18_1.v));
				v26.v = _mm_add_ps(v26.v, _mm_mul_ps(v2.v, v18_2.v));
				v27.v = _mm_add_ps(v27.v, _mm_mul_ps(v2.v, v18_3.v));

				//第4列乘第4行
				v3.v = _mm_load_ps(b + 12);
				v19_0.v = _mm_load1_ps(a++);
				v19_1.v = _mm_load1_ps(a++);
				v19_2.v = _mm_load1_ps(a++);
				v19_3.v = _mm_load1_ps(a++);
				v24.v = _mm_add_ps(v24.v, _mm_mul_ps(v3.v, v19_0.v));
				v25.v = _mm_add_ps(v25.v, _mm_mul_ps(v3.v, v19_1.v));
				v26.v = _mm_add_ps(v26.v, _mm_mul_ps(v3.v, v19_2.v));
				v27.v = _mm_add_ps(v27.v, _mm_mul_ps(v3.v, v19_3.v));

				b += 16;
			}
			v24.v = _mm_add_ps(_mm_load_ps(c), v24.v);
			v25.v = _mm_add_ps(_mm_load_ps(c + ldc), v25.v);
			v26.v = _mm_add_ps(_mm_load_ps(c + 2 * ldc), v26.v);
			v27.v = _mm_add_ps(_mm_load_ps(c + 3 * ldc), v27.v);

			//将__m128内的值赋给addr
			_mm_store_ps(c, v24.v);
			_mm_store_ps(c + ldc, v25.v);
			_mm_store_ps(c + 2 * ldc, v26.v);
			_mm_store_ps(c + 3 * ldc, v27.v);

			c += 4;
			a -= 4 * kc;
		}
		sc += ldc * 4;
		c = sc;
		a += 4 * kc;
		b = sb;
	}
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
	} while (--j > 0);
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

			b_offset0 += k * 4;	//处理同一个4行中，其他列的数据
		} while (--i > 0);
	} while (--j > 0);
}

#else  //__ANDROID__

void MMult_4x4_14(float* A, float* B, float* C, int m, int n, int k) {
}

#endif  //__ANDROID__
