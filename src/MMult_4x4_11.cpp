#include <iostream>

#ifndef __ANDROID__

#define A(i,j) a[ (i)*wa + (j) ]
#define B(i,j) b[ (i)*wb + (j) ]
#define C(i,j) c[ (i)*wc + (j) ]

/* Block sizes */
#define nc 128
#define kc 256

#define min( i, j ) ( (i)<(j) ? (i): (j) )

static void AddDot4x4(int k, float *a, int wa, float *b, int wb, float *c, int wc);
static void InnerKernel(int m, int n, int k, float* A, int lda,
	float* B, int ldb,
	float* C, int ldc);

void MMult_4x4_11(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//�г���, �õ�k������ K��������ӵõ�C

	//nc: 256
	//kc: 128
	//GEPB: ����������������
	for (int p = 0; p < k; p += kc) {		//Panel
		int pa = min(k - p, kc);		//A���з�Ϊpanel, B���з�Ϊpanel
		for (int j = 0; j < n; j += nc) {	//Block
			int ja = min(m-j, nc);		//B��panel���з�Ϊblock
			//�õ�һ��m*ja(���Ϊnc)�ľ���, �ӷ�����Ϊpa(���Ϊkc)
			//panel*block
			//pΪ��ֵͬ��C�ĵ�ַû��Ӱ��, ��p���ۼ�
			//A��Bȫ�����Էŵ�L2 cache��, ���ܻ���������
			InnerKernel(m, ja, pa, 
				&A[p], k,
				&B[p*n+j], n,
				&C[j], n
				);
		}
	}
}

static void InnerKernel(int m, int n, int k, float* a, int wa,
	float* b, int wb,
	float* c, int wc) {
	//A: m*k; B: k*n; C: m*n
	//�г���, �õ�k������ K��������ӵõ�C

	for (int i = 0; i < m; i+=4) {
		for (int j = 0; j < n; j+=4) {
			AddDot4x4(k, &A(i, 0), wa, &B(0, j), wb, &C(i, j), wc);
		}
	}
}

#include <mmintrin.h>
#include <xmmintrin.h>  // SSE
#include <pmmintrin.h>  // SSE2
#include <emmintrin.h>  // SSE3

typedef union {
	__m128	v;		//__m128�����ȸ���, __m128i����, __m128d˫����
	float	f[4];
} v4f_t;

/*
	4xk����A��һ��kx4����B���, �õ�4x4�Ľ��C��
	ʹ��K��ѭ��, ÿ��ѭ��ʱ����A���зŵ�sse vec��, ��B���зŵ�sse vec�С�����vec��ˣ�һ���Եõ�C��һ�С�
	���У�B�����ڴ治��������Ϊ�ǴӴ�����н�ȡ�ġ�
*/
static void AddDot4x4(int k, float *a, int wa, float *b, int wb, float *c, int wc)
{
	v4f_t
		c_00_01_02_03_vreg, c_10_11_12_13_vreg,
		c_20_21_22_23_vreg, c_30_31_32_33_vreg,
		b_p0_p1_p2_p3_vreg,
		a_0p_reg, a_1p_reg, a_2p_reg, a_3p_reg;

	float
		*a_p0_pntr, *a_p1_pntr, *a_p2_pntr, *a_p3_pntr;

	a_p0_pntr = &A(0, 0);
	a_p1_pntr = &A(1, 0);
	a_p2_pntr = &A(2, 0);
	a_p3_pntr = &A(3, 0);

	c_00_01_02_03_vreg.v = _mm_setzero_ps();
	c_10_11_12_13_vreg.v = _mm_setzero_ps();
	c_20_21_22_23_vreg.v = _mm_setzero_ps();
	c_30_31_32_33_vreg.v = _mm_setzero_ps();

	for (int p = 0; p < k; ++p) {
		b_p0_p1_p2_p3_vreg.v = _mm_load_ps((float*)&B(p, 0));	//���ڴ��е����ݵ�vector��

		a_0p_reg.v = _mm_load1_ps(a_p0_pntr++);		//ȡ*a_p0_pntr, �����Ĵ�
		a_1p_reg.v = _mm_load1_ps(a_p1_pntr++);
		a_2p_reg.v = _mm_load1_ps(a_p2_pntr++);
		a_3p_reg.v = _mm_load1_ps(a_p3_pntr++);

		c_00_01_02_03_vreg.v = _mm_add_ps(c_00_01_02_03_vreg.v, _mm_mul_ps(a_0p_reg.v, b_p0_p1_p2_p3_vreg.v));
		c_10_11_12_13_vreg.v = _mm_add_ps(c_10_11_12_13_vreg.v, _mm_mul_ps(a_1p_reg.v, b_p0_p1_p2_p3_vreg.v));
		c_20_21_22_23_vreg.v = _mm_add_ps(c_20_21_22_23_vreg.v, _mm_mul_ps(a_2p_reg.v, b_p0_p1_p2_p3_vreg.v));
		c_30_31_32_33_vreg.v = _mm_add_ps(c_30_31_32_33_vreg.v, _mm_mul_ps(a_3p_reg.v, b_p0_p1_p2_p3_vreg.v));
	}

	C(0, 0) += c_00_01_02_03_vreg.f[0];   C(0, 1) += c_00_01_02_03_vreg.f[1];   C(0, 2) += c_00_01_02_03_vreg.f[2];   C(0, 3) += c_00_01_02_03_vreg.f[3];
	C(1, 0) += c_10_11_12_13_vreg.f[0];   C(1, 1) += c_10_11_12_13_vreg.f[1];   C(1, 2) += c_10_11_12_13_vreg.f[2];   C(1, 3) += c_10_11_12_13_vreg.f[3];
	C(2, 0) += c_20_21_22_23_vreg.f[0];   C(2, 1) += c_20_21_22_23_vreg.f[1];   C(2, 2) += c_20_21_22_23_vreg.f[2];   C(2, 3) += c_20_21_22_23_vreg.f[3];
	C(3, 0) += c_30_31_32_33_vreg.f[0];   C(3, 1) += c_30_31_32_33_vreg.f[1];   C(3, 2) += c_30_31_32_33_vreg.f[2];   C(3, 3) += c_30_31_32_33_vreg.f[3];
}

#else  //__ANDROID__

void MMult_4x4_11(float* A, float* B, float* C, int m, int n, int k) {
}

#endif  //__ANDROID__
