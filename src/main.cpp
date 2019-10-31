#include <iostream>
#include <random>
#include <time.h>
#include "timer.h"

static void random_vector(std::vector<float>& matrix) {
	int size = static_cast<int>(matrix.size());
	unsigned int min = 0;
	unsigned int max = 255;
	clock_t start = clock();
	for (int i = 0; i < size; ++i) {
		unsigned int seed = static_cast<unsigned int>(i + start);
		static std::default_random_engine e(seed);
		static std::uniform_real_distribution<double> u(min, max);
		matrix[i] = static_cast<float>(static_cast<int>(u(e)));
	}
}

static void clear_vector(std::vector<float>& v, float num = 0.f) {
	for (auto& f : v) {
		f = num;
	}
}

static void check_result(std::vector<float>& l, std::vector<float>& r) {
	for (int i = 0; i < l.size(); ++i) {
		if (l[i] != r[i]) {
			std::cout << "compare error in idx:" << i << "." << l[i] << ":" << r[i] << std::endl;
			if (i > 100) {
				return;
			}
		}
	}
}

// https://github.com/flame/how-to-optimize-gemm
// 上述github中是列主序, 这里改为行主序。
// 参考 https://zhuanlan.zhihu.com/p/65436463
// 对应git https://github.com/tpoisonooo/how-to-optimize-gemm

extern void MMultBase(float* A, float* B, float* C, int m, int n, int k);
extern void MMult1(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x1_5(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x1_6(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x1_7(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x1_8(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x4_5(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x4_6(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x4_7(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x4_8(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x4_9(float* A, float* B, float* C, int m, int n, int k);
//从这里开始会用到汇编
extern void MMult_4x4_10(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x4_11(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x4_12(float* A, float* B, float* C, int m, int n, int k);

//GEMM
extern void MMult_4x4_13(float* A, float* B, float* C, int m, int n, int k);
extern void MMult_4x4_14(float* A, float* B, float* C, int m, int n, int k);

int main(int argc, char* argv[]) {
	int size = 600; // 248; // 600;
	size = (size / 4) * 4;
	int m = size;
	int n = size;
	int k = size;
	std::vector<float> A(m*k);	//m*k
	std::vector<float> B(k*n);	//k*n
	std::vector<float> C(m*n);	//C = A*B
	std::vector<float> tmp_cmp(m*n);
	random_vector(A);
	random_vector(B);

	unsigned long long cal_num = 2 * (unsigned long long)m * (unsigned long long)n * (unsigned long long)k;
	double gflops = (double)cal_num / (double)(1024 * 1024 * 1024);

	HighClock clk;
	double cal_time = 0.;
#if 1
	clear_vector(C);
	// matrix multipl1 test
	clk.Start();
	MMultBase(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000.; //s
	std::cout << "MMultBase time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif
	tmp_cmp = C;

#if 0
	// 相比Base没有性能提升
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult1(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult1 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度有提升, 原因 https://github.com/flame/how-to-optimize-gemm/wiki/Optimization_1x4_5
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x1_5(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_1x4_5 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度有提升, 原因 https://github.com/flame/how-to-optimize-gemm/wiki/Optimization_1x4_5
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x1_6(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_1x4_6 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度有提升, 原因 https://github.com/flame/how-to-optimize-gemm/wiki/Optimization_1x4_5
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x1_7(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_1x4_7 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif


#if 0
	// 速度有提升, 原因 https://github.com/flame/how-to-optimize-gemm/wiki/Optimization_1x4_5
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x1_8(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x1_8 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度有提升, 原因 https://github.com/flame/how-to-optimize-gemm/wiki/Optimization_1x4_5
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_5(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_5 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度有提升, 原因 https://github.com/flame/how-to-optimize-gemm/wiki/Optimization_1x4_5
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_6(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_6 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度有提升, 原因 https://github.com/flame/how-to-optimize-gemm/wiki/Optimization_1x4_5
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_7(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_7 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度未提升, 原因 https://github.com/flame/how-to-optimize-gemm/wiki/Optimization_1x4_5
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_8(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_8 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度未提升, 4x4中一次计算2列
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_9(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_9 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_10(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_10 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	// 速度
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_11(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_11 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 1
	// 速度
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_12(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_12 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 0
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_13(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_13 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 1
	clear_vector(C);
	// matrix multipl pack test
	clk.Start();
	MMult_4x4_14(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult_4x4_14 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

	check_result(tmp_cmp, C);

	return 0;
}