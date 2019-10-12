#include <iostream>
#include <random>
#include <time.h>
#include "timer.h"

void random_vector(std::vector<float>& matrix) {
	int size = static_cast<int>(matrix.size());
	unsigned int min = 0;
	unsigned int max = 255;
	clock_t start = clock();
	for (int i = 0; i < size; ++i) {
		unsigned int seed = static_cast<unsigned int>(i + start);
		static std::default_random_engine e(seed);
		static std::uniform_real_distribution<double> u(min, max);
		matrix[i] = static_cast<float>(u(e));
	}
}

// https://github.com/flame/how-to-optimize-gemm
// 上述github中是列主序, 这里改为行主序。
// 参考 https://github.com/tpoisonooo/how-to-optimize-gemm

extern void MMultBase(float* A, float* B, float* C, int m, int n, int k);
extern void MMult1(float* A, float* B, float* C, int m, int n, int k);

int main(int argc, char* argv[]) {
	int size = 1000;
	size = (size / 4) * 4;
	int m = size;
	int n = size;
	int k = size;
	std::vector<float> A(m*k);	//m*k
	std::vector<float> B(k*n);	//k*n
	std::vector<float> C(m*n);	//C = A*B
	random_vector(A);
	random_vector(B);

	unsigned long long cal_num = 2 * (unsigned long long)m * (unsigned long long)n * (unsigned long long)k;
	double gflops = (double)cal_num / (double)(1024 * 1024 * 1024);

	HighClock clk;
	double cal_time = 0.;
#if 1
	// matrix multipl1 test
	clk.Start();
	MMultBase(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000.; //s
	std::cout << "MMultBase time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

#if 1
	// matrix multipl pack test
	clk.Start();
	MMult1(&A[0], &B[0], &C[0], m, n, k);
	clk.Stop();
	cal_time = clk.GetTime() / 1000000; //s
	std::cout << "MMult1 time: " << cal_time * 1000. << "ms. GFLOPS/sec: " << gflops / cal_time << std::endl;
#endif

	return 0;
}