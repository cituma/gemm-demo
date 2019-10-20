#include <iostream>
#include <vector>

#define A(i,j) a[ (i)*wa + (j) ]
#define B(i,j) b[ (i)*wb + (j) ]
#define C(i,j) c[ (i)*wc + (j) ]

/* Block sizes */
#define nc 128
#define kc 256

#define min( i, j ) ( (i)<(j) ? (i): (j) )

static void AddDot4x4(int k, float *a, int wa, float *b, int wb, float *c, int wc);
static void InnerKernel(int m, int n, int k, float* a, int lda,
	float* b, int ldb,
	float* c, int ldc);

void MMult_4x4_12(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//列乘行, 得到k个矩阵。 K个矩阵相加得到C

	//nc: 256
	//kc: 128
	//GEPB: 行主序下性能最优
	for (int p = 0; p < k; p += kc) {		//Panel
		int pa = min(k - p, kc);		//A按列分为panel, B按行分为panel
		for (int j = 0; j < n; j += nc) {	//Block
			int ja = min(m-j, nc);		//B的panel按列分为block
			//得到一个m*ja(最大为nc)的矩阵, 加法次数为pa(最大为kc)
			//panel*block
			//p为不同值对C的地址没有影响, 在p轴累加
			//A和B全部可以放到L2 cache中, 性能会有提升。
			InnerKernel(m, ja, pa, 
				&A[p], k,
				&B[p*n+j], n,
				&C[j], n
				);
		}
	}
}

//让4xk的A矩阵按列内存连续. 因为 AddDot4x4 每次把列保存到sse vec中. 
static void PackMatrixA(int k, float *a, int wa, float *a_to)
{
	float
		*a_0i_pntr = a,
		*a_1i_pntr = a + wa,
		*a_2i_pntr = a + (wa << 1),
		*a_3i_pntr = a + (3 * wa);

	for (int i = 0; i < k; ++i) {
		*a_to++ = *a_0i_pntr++;
		*a_to++ = *a_1i_pntr++;
		*a_to++ = *a_2i_pntr++;
		*a_to++ = *a_3i_pntr++;
	}
}

//让kx4的B矩阵按行内存连续.因为 AddDot4x4 
static void PackMatrixB(int k, float *b, int wb, float *b_to)
{
	for (int j = 0; j < k; j++) {  /* loop over columns of A */
		float* b_ij_pntr = &B(j, 0);

		*b_to++ = *b_ij_pntr;
		*b_to++ = *(b_ij_pntr + 1);
		*b_to++ = *(b_ij_pntr + 2);
		*b_to++ = *(b_ij_pntr + 3);
	}
}

static void InnerKernel(int m, int n, int k, float* a, int wa,
	float* b, int wb,
	float* c, int wc) {
	//A: m*k; B: k*n; C: m*n
	//列乘行, 得到k个矩阵。 K个矩阵相加得到C

	std::vector<float> packedA(m * k);
	std::vector<float> packedB(k * n);

	for (int j = 0; j < n; j += 4) {
		//pcakedB为(n/4) * (k * 4) 的矩阵
		PackMatrixB(k, &B(0, j), wb, &packedB[j*k]);
		for (int i = 0; i < m; i += 4) {
			if (j == 0) {
				PackMatrixA(k, &A(i, 0), wa, &packedA[i*k]);
			}
			//AddDot4x4(k, &A(i, 0), wa, &B(0, j), wb, &C(i, j), wc);
			//4行 与 4列 相乘, 得到C[4x4]矩阵
			AddDot4x4(k, &packedA[i*k], k, &packedB[j * k], wb, &C(i, j), wc);
		}
	}
}

#include <mmintrin.h>
#include <xmmintrin.h>  // SSE
#include <pmmintrin.h>  // SSE2
#include <emmintrin.h>  // SSE3

typedef union {
	__m128	v;		//__m128单精度浮点, __m128i整型, __m128d双精度
	float	f[4];
} v2f_t;

/*
	4xk矩阵A和一个kx4矩阵B相乘, 得到4x4的结果C。
	使用K个循环, 每次循环时，将A的列放到sse vec中, 将B的行放到sse vec中。两个vec相乘，一次性得到C的一行。
	其中，经过pack操作, B的行内存连续. 所以B每个循环地址加4就行。 
*/
static void AddDot4x4(int k, float *a, int wa, float *b, int wb, float *c, int wc)
{
	v2f_t
		c_00_01_02_03_vreg, c_10_11_12_13_vreg,
		c_20_21_22_23_vreg, c_30_31_32_33_vreg,
		b_p0_p1_p2_p3_vreg,
		a_0p_reg, a_1p_reg, a_2p_reg, a_3p_reg;

	c_00_01_02_03_vreg.v = _mm_setzero_ps();
	c_10_11_12_13_vreg.v = _mm_setzero_ps();
	c_20_21_22_23_vreg.v = _mm_setzero_ps();
	c_30_31_32_33_vreg.v = _mm_setzero_ps();

	for (int p = 0; p < k; ++p) {
		//b_p0_p1_p2_p3_vreg.v = _mm_load_ps((float*)&B(p, 0));	//读内存中的内容到vector中
		b_p0_p1_p2_p3_vreg.v = _mm_load_ps(b);
		b += 4;

		a_0p_reg.v = _mm_load1_ps(a++);		//取*a, 拷贝四次
		a_1p_reg.v = _mm_load1_ps(a++);
		a_2p_reg.v = _mm_load1_ps(a++);
		a_3p_reg.v = _mm_load1_ps(a++);

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
