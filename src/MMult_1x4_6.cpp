#include <iostream>

static void AddDot1x4(int k, float *a, float *b, int n, float *c);

void MMult_1x4_6(float* A, float* B, float* C, int m, int n, int k) {
	//A: m*k; B: k*n; C: m*n
	//列乘行, 得到k个矩阵。 K个矩阵相加得到C

	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < n; j+=4) {
			AddDot1x4(k, &A[i*k], &B[j], n, &C[i*n + j]);
		}
	}
}

static void AddDot1x4(int k, float *a, float *b, int n, float *c)
{
	//register关键字请求编译器尽可能的将变量存在CPU内部寄存器中
	register float
		/* hold contributions to
		   C( 0, 0 ), C( 0, 1 ), C( 0, 2 ), C( 0, 3 ) */
		c_00_reg = 0.f, c_01_reg = 0.f, c_02_reg = 0.f, c_03_reg = 0.f,
		/* holds A( p ) */
		a_0p_reg;

	for (int p = 0; p < k; p++) {
		a_0p_reg = a[p];
		c_00_reg += a_0p_reg * b[p*n];
		c_01_reg += a_0p_reg * b[p*n + 1];
		c_02_reg += a_0p_reg * b[p*n + 2];
		c_03_reg += a_0p_reg * b[p*n + 3];
	}
	c[0] += c_00_reg;
	c[1] += c_01_reg;
	c[2] += c_02_reg;
	c[3] += c_02_reg;
}
