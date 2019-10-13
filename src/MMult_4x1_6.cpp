#include <iostream>

static void AddDot4x1(int k, float *a, int m, float *b, int n, float *c);

void MMult_4x1_6(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//列乘行, 得到k个矩阵。 K个矩阵相加得到C

	for (int i = 0; i < m; i+=4) {
		for (int j = 0; j < n; ++j) {
			AddDot4x1(k, &A[i*k], m, &B[j], n, &C[i*n + j]);
		}
	}
}

static void AddDot4x1(int k, float *a, int m, float *b, int n, float *c)
{
	//register关键字请求编译器尽可能的将变量存在CPU内部寄存器中
	register float
		/* hold contributions to
		   C( 0, 0 ), C( 0, 1 ), C( 0, 2 ), C( 0, 3 ) */
		c_00_reg = 0.f, c_01_reg = 0.f, c_02_reg = 0.f, c_03_reg = 0.f,
		/* holds A( p ) */
		b_0p_reg;

	for (int p = 0; p < k; p++) {
		b_0p_reg = b[p*n];
		c_00_reg += a[p] * b_0p_reg;
		c_01_reg += a[k + p] * b_0p_reg;
		c_02_reg += a[2 * k + p] * b_0p_reg;
		c_03_reg += a[3 * k + p] * b_0p_reg;
	}
	c[0] += c_00_reg;
	c[n] += c_01_reg;
	c[2*n] += c_02_reg;
	c[3*n] += c_03_reg;
}
