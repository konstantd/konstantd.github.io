
#include<vector>
#include <benchmark/benchmark.h>

// Compile with: g++ -03 cache_locality_matrix.cpp -o cache_perf_test.exe -lpthread -lbenchmark 
// SIMD vectorization with aggresive -O3 and linear prefetcher give better results


template<typename T> 
void multiply_naive(const std::vector<T>& a, const std::vector<T>& b, std::vector<T>& result, int const N) {
    std::fill(result.begin(), result.end(), 0);

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            for (int k = 0; k < N; ++k) {
                // b[k*N + j] is accessed with a stride of N - vector "a" has linear access 
                result[i * N + j] += a[i * N + k] * b[k * N + j];  // Prefetcher cannot really help here with the b vector, since it is prefetching linearly
            }
        }
    }
}



template<typename T> 
void multiply_performance(const std::vector<T>& a, const std::vector<T>& b, std::vector<T>& result, int const N) {
    std::fill(result.begin(), result.end(), 0);

    for (int i = 0; i < N; ++i) {
        for (int k = 0; k < N; ++k) {
            // Cache A[i][k] in a register since it's constant for the j-loop
            auto temp = a[i * N + k]; 
            for (int j = 0; j < N; ++j) {
                // Now accessing result[i][j] and b[k][j] linearly
                result[i * N + j] += temp * b[k * N + j];   // Prefetcher will load already the next block in memory
            }
        }
    }

}



template <typename T>
static void BM_Multiply_Naive_Template(benchmark::State& state) {
    int N = state.range(0);
    std::vector<T> m1(N * N, 10.41);
    std::vector<T> m2(N * N, 20.09);
    std::vector<T> m3(N * N);

    for (auto _ : state) {
        multiply_naive(m1, m2, m3, N);
        benchmark::DoNotOptimize(m3.data()); 
    }
}





template <typename T>
static void BM_Multiply_Perf_Template(benchmark::State& state) {
    int N = state.range(0);

    std::vector<T> m1(N * N, 10.41);
    std::vector<T> m2(N * N, 20.09);
    std::vector<T> m3(N * N);

    for (auto _ : state) {
        multiply_performance(m1, m2, m3, N);
        benchmark::DoNotOptimize(m3.data());
    }
}




// Tests with N = 1024 
// 
// One array of 1024 * 1024 * 8 will fit in L3 (in my machine 12 MB)
BENCHMARK_TEMPLATE(BM_Multiply_Naive_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Template, double)->Arg(1024);
BENCHMARK_MAIN();
