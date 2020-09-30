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
	__m128	v;		//__m128�����ȸ���, __m128i����, __m128d˫����
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
����������, �����GEPB(A,B,C)����
    ǰ 3 ��ǰ�᲻���� TLB������ֻ�� �ڴ桢cache �� ALU ��
	1) kc * nc ҪС��С�� " (A��min_mm��) + (B��block) + (C��min_mm��) "�ܹ�һ������ L1 cache
	2) ��� 1 �����㣬CPU ����ʱ�������ڴ��ٶȵ����ƣ����õ���gflopsֵ������ʵ�ļ�������
	3) B ��blockֻ�ᱻ���ؽ� Cache һ�Σ�gemm�����в��ᱻ�����ֻ���
    �� 2 ��Ҫ���� TLB����Ϊ TLB miss �� stall CPU��
	4) kc �� nc ҪС��С�� "(A��min_mm��) + (B��block) + (C��min_mm��) " �ܹ��� TLB ����.
    5) B��blockֻ�����ص� TLB һ��. gemm���̲��ỻ�뻻��.

���������²�������:
	1) kc ~= nc //��һ���
    2) min_mm >= (Rcomp/(2*Rload)) ���� Rcomp ��������Rload �� L2 cache �� register �Ĵ���. Ҳ����˵��B��L2cache��ȡ����ʱ�䲻����ǰһ��Ԫ�ؼ���ʱ�䡣
    3) kc * nc <= K
    4) kc * nc ֻ��ռcacheһ��.
*/
void MMult_4x4_14(float* A, float* B, float* C, int m, int n, int k) {
	int lda = k;
	int ldb = n;
	int ldc = n;
	//A: m*k����
	//B: k*n����
	//C: C=AxB; m*n����
	float* sa = fastMalloc(m * k);
	float* sb = fastMalloc(k * n);

	for (int ms = 0; ms < m; ms += GEMM_M) {
		int min_m = m - ms;
		//min_m ��4��GEMM_M��, �������һ��ѭ��������ֵ��ΪGEMM_M
		if (min_m > GEMM_M) {
			min_m = GEMM_M;
		}

		int min_k = 1;
        //����Fig.4��, GEMM_VAR1����
		for (int ks = 0; ks < k; ks += min_k) {
			min_k = k - ks;
			//min_k ��4��GEMM_K֮��, �������һ��ѭ��������ֵ��ΪGEMM_K
			if (min_k >= (GEMM_K << 1)) {
				min_k = GEMM_K;
			}
			else if (min_k > GEMM_K) {
				//min_k = (((min_k / 2) + 3) / 4) * 4;
				min_k = (min_k / 2 + GEMM_UNROLL - 1) & ~(GEMM_UNROLL - 1);	//(min_k / 2)������GEMM_UNROLL����
			}

			//���� packB
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
            //A��. �߿�Ϊ min_m*min_k �� panel ��, ��Ϊ�߿�Ϊ min_mm*min_k ��С��.
            //��ЩС��ֱ����B�и߿�Ϊ min_k*min_n ��block(����GEMM����С����).
            //����˲�ƴ�Ӻ�, �õ�C��min_m * min_n ��panel.
            //����Fig.4��, GEPP_VAR2�ֽ��, GEPB����.
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

                //Fig.4 GEPB��������С������Ԫ
				KERNEL_4x4(min_mm, min_n, min_k, sa_addr, sb, C + mms * ldc, ldc);
			}

            //����Fig.4��, GEPP_VAR2�������block.
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
GEPB����:
mc-kc��kc-nc�����. �������һ��, ��������mc==GEMM_M, kc==GEMM_K, nc==GEMM_N
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
    //bӦ�÷���L2cache��. ����ͨ������kc*nc��С,
	for (int i = 0; i < mc; i += 4) {
		//a��ÿ4�з�panel
		for (int j = 0; j < nc; j += 4) {
            //�ڴ�ѭ���е�a���ظ�ʹ�ã�Ӧ�÷���L1cache��. ����ͨ������kc��С, ʹa��С(4*kc)ΪL1 cache��һ��.

			//b��ÿ4�з�panel

			v24.v = _mm_setzero_ps();
			v25.v = _mm_setzero_ps();
			v26.v = _mm_setzero_ps();
			v27.v = _mm_setzero_ps();
			//4xk �� kx4 �����������,�ۼӵõ�4x4����
            //v24-v27�ܹ�16��float, Ӧ�ô���ڼĴ�����
			for (int l = 0; l < kc; l += 4) {
				/*
				A, B�����Ѱ�pack_A��pack_B�ķ�ʽ����.
				A(packǰ):
				0 1 2 3  4 5 6 7 ...
				0 1 2 3  4 5 6 7 ...
				0 1 2 3  4 5 6 7 ...
				0 1 2 3  4 5 6 7 ...

				B(packǰ):
				0 1 2 3
				0 1 2 3
				0 1 2 3
				0 1 2 3
				8 9 a b
				8 9 a b
				8 9 a b
				8 9 a b
				...

				��A��l�г�B��l��, (l+1)�г�(l+1)��,(l+2)�г�(l+2)��,(l+3)�г�(l+3)��
				�õ�4��4x4�����ۼ�.
				����k/4��4x4�����ۼ�.
				*/

				//��1�г˵�1��
				//v24,v25,v26,v27���һ��4x4�ľ���
				v0.v = _mm_load_ps(b);
				v16_0.v = _mm_load1_ps(a++);		//ȡ*a, �����Ĵ�
				v16_1.v = _mm_load1_ps(a++);
				v16_2.v = _mm_load1_ps(a++);
				v16_3.v = _mm_load1_ps(a++);
				v24.v = _mm_add_ps(v24.v, _mm_mul_ps(v0.v, v16_0.v));
				v25.v = _mm_add_ps(v25.v, _mm_mul_ps(v0.v, v16_1.v));
				v26.v = _mm_add_ps(v26.v, _mm_mul_ps(v0.v, v16_2.v));
				v27.v = _mm_add_ps(v27.v, _mm_mul_ps(v0.v, v16_3.v));

				//��2�г˵�2��
				v1.v = _mm_load_ps(b + 4);
				v17_0.v = _mm_load1_ps(a++);
				v17_1.v = _mm_load1_ps(a++);
				v17_2.v = _mm_load1_ps(a++);
				v17_3.v = _mm_load1_ps(a++);
				v24.v = _mm_add_ps(v24.v, _mm_mul_ps(v1.v, v17_0.v));
				v25.v = _mm_add_ps(v25.v, _mm_mul_ps(v1.v, v17_1.v));
				v26.v = _mm_add_ps(v26.v, _mm_mul_ps(v1.v, v17_2.v));
				v27.v = _mm_add_ps(v27.v, _mm_mul_ps(v1.v, v17_3.v));

				//��3�г˵�3��
				v2.v = _mm_load_ps(b + 8);
				v18_0.v = _mm_load1_ps(a++);
				v18_1.v = _mm_load1_ps(a++);
				v18_2.v = _mm_load1_ps(a++);
				v18_3.v = _mm_load1_ps(a++);
				v24.v = _mm_add_ps(v24.v, _mm_mul_ps(v2.v, v18_0.v));
				v25.v = _mm_add_ps(v25.v, _mm_mul_ps(v2.v, v18_1.v));
				v26.v = _mm_add_ps(v26.v, _mm_mul_ps(v2.v, v18_2.v));
				v27.v = _mm_add_ps(v27.v, _mm_mul_ps(v2.v, v18_3.v));

				//��4�г˵�4��
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

			//��__m128�ڵ�ֵ����addr
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
�ڿ�Ϊlda��A�����m*k��������, ���ڴ水���·�ʽ����(pack)
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
�ڿ�Ϊldb��B�����k*n��������, ���ڴ水���·�ʽ����(pack)

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

			b_offset0 += k * 4;	//����ͬһ��4���У������е�����
		} while (--i > 0);
	} while (--j > 0);
}

#else  //__ANDROID__

void MMult_4x4_14(float* A, float* B, float* C, int m, int n, int k) {
}

#endif  //__ANDROID__
