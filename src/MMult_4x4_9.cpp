#include <iostream>


#define A(i,j) a[ (i)*wa + (j) ]
#define B(i,j) b[ (i)*wb + (j) ]
#define C(i,j) c[ (i)*wc + (j) ]

static void AddDot4x4(int k, float *a, int wa, float *b, int wb, float *c, int wc);

void MMult_4x4_9(float* A, float* B, float* C, int m, int n, int k) {
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
	register float
		/* hold contributions to
		   C( 0, 0 ), C( 0, 1 ), C( 0, 2 ), C( 0, 3 )
		   C( 1, 0 ), C( 1, 1 ), C( 1, 2 ), C( 1, 3 )
		   C( 2, 0 ), C( 2, 1 ), C( 2, 2 ), C( 2, 3 )
		   C( 3, 0 ), C( 3, 1 ), C( 3, 2 ), C( 3, 3 )   */
		c_00_reg = 0.f, c_01_reg = 0.f, c_02_reg = 0.f, c_03_reg = 0.f,
		c_10_reg = 0.f, c_11_reg = 0.f, c_12_reg = 0.f, c_13_reg = 0.f,
		c_20_reg = 0.f, c_21_reg = 0.f, c_22_reg = 0.f, c_23_reg = 0.f,
		c_30_reg = 0.f, c_31_reg = 0.f, c_32_reg = 0.f, c_33_reg = 0.f,
		/* hold
		   A( 0, p )
		   A( 1, p )
		   A( 2, p )
		   A( 3, p ) */
		b_0p_reg,
		b_1p_reg,
		b_2p_reg,
		b_3p_reg,
		a_0p_reg,
		a_1p_reg,
		a_2p_reg,
		a_3p_reg;
	float
		*a_p0_pntr, *a_p1_pntr, *a_p2_pntr, *a_p3_pntr;

	a_p0_pntr = &A(0, 0);
	a_p1_pntr = &A(1, 0);
	a_p2_pntr = &A(2, 0);
	a_p3_pntr = &A(3, 0);
	for (int p = 0; p < k; ++p) {
		b_0p_reg = B(p, 0);
		b_1p_reg = B(p, 1);
		b_2p_reg = B(p, 2);
		b_3p_reg = B(p, 3);

		a_0p_reg = *a_p0_pntr++;
		a_1p_reg = *a_p1_pntr++;
		a_2p_reg = *a_p2_pntr++;
		a_3p_reg = *a_p3_pntr++;

		//第0,1列
		c_00_reg += a_0p_reg * b_0p_reg;
		c_01_reg += a_0p_reg * b_1p_reg;

		c_10_reg += a_1p_reg * b_0p_reg;
		c_11_reg += a_1p_reg * b_1p_reg;

		c_20_reg += a_2p_reg * b_0p_reg;
		c_21_reg += a_2p_reg * b_1p_reg;

		c_30_reg += a_3p_reg * b_0p_reg;
		c_31_reg += a_3p_reg * b_1p_reg;
		

		//第2, 3列
		c_02_reg += a_0p_reg * b_2p_reg;
		c_03_reg += a_0p_reg * b_3p_reg;

		c_12_reg += a_1p_reg * b_2p_reg;
		c_13_reg += a_1p_reg * b_3p_reg;

		c_22_reg += a_2p_reg * b_2p_reg;
		c_23_reg += a_2p_reg * b_3p_reg;

		c_32_reg += a_3p_reg * b_2p_reg;
		c_33_reg += a_3p_reg * b_3p_reg;
	}

	C(0, 0) += c_00_reg;   C(0, 1) += c_01_reg;   C(0, 2) += c_02_reg;   C(0, 3) += c_03_reg;
	C(1, 0) += c_10_reg;   C(1, 1) += c_11_reg;   C(1, 2) += c_12_reg;   C(1, 3) += c_13_reg;
	C(2, 0) += c_20_reg;   C(2, 1) += c_21_reg;   C(2, 2) += c_22_reg;   C(2, 3) += c_23_reg;
	C(3, 0) += c_30_reg;   C(3, 1) += c_31_reg;   C(3, 2) += c_32_reg;   C(3, 3) += c_33_reg;
}
