+++
date = '2026-02-12T11:06:26+01:00'
draft = false
title = 'Avoid the RAM Latency: Keeping the Cache Hot and on Linear Access is the Ultimate C++ Optimization'
summary = 'In this benchmark, we explore the importance of keeping data within the CPU cache to avoid expensive retrieval from RAM. By simply ensuring linear data access and accessing by blocks that fit in L1 and L2, we can achieve massive performance gains without changing the underlying algorithm.'
tags = ["advanced-level", "HPC", "cache-locality", "performance", "blocking", "tiling", "simd", "DOD", "AoS", "perf"]
+++

TODO:
// std::span

In this benchmark, we explore the importance of keeping data within the CPU cache to avoid expensive retrieval from RAM. By simply ensuring **linear data access**, we can achieve massive performance gains without changing the underlying algorithm. This principle is used also in **Data Oriented Design (DOD)** with **Array of Structures (AoS)** or **Structures of Arrays (SoA)**, since we lay down all our data to fit linearly. Apart from that there is also the benefit, that when we follow **DOD** designs we avoid also the runtime dynamic dispatch on polymorphism. Though here, in our example we will just focus on the benefit of keeping the cache hot with and we will demonstrate the performance gain in a simple matrix multiplication example using the **perf** tool and **google-benchmark**.

We will have 3 scenarios:

1. one bad multiplication that we do not access the data linearly, 
2. then one that we do access the data linearly. We will notice how massive speed we can gain just from this small change. 
3. Then we will try to improve it even more, accessing in **blocks** of size that fit in cache L1/L2 (**tiling**). 

(Note that similar techniques are implemented to fit data in the cache line when reading or modifying data, aligning with 64 bytes which is the cache line. In C++17 we also have `std::hardware_constructive_interference_size`, but in most machines this is the same as 64bytes anyways.)


## The Core Concept: CPU Cache vs. RAM

Data access speed is largely determined by physical distance and the hierarchy of memory. Below table gives an idea of the time and cycles the CPU needs to access data from the corresponding memory. Going down to the hierarchy, the memory grows and the speed also decreases.


| Memory Level - |   Time to reach  |   CPU Cycles (Approx.)
| :--- | :--- | :--- |
| L1 Cache	   |   ~1 ns | 4–5 cycles |  
| L2 Cache	  |    ~4 ns |  12–15 cycles|  
| L3 Cache	  |   ~10-40 ns | 40–60 cycles |  
| Main RAM	 |    ~100 ns+  | 200–300+ cycles |  
 


Notice that accessing RAM can be 100 times slower thatn L1. It would be nice to try to keep our data in cache, so we avoid this cost and this is exactly what we are going to do.



## The Code: Naive vs Optimized vs Blocking

We are comparing 3 versions of a matrix multiplication. We lay our matrixes down linearly. For the first 2, the only difference is the order of the nested loops, which defines how we traverse memory.

``` cpp
#include<vector>
#include <benchmark/benchmark.h>


// Simple and naive
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
```



Now the improved version **Optimized** just changing the stride:


```cpp
// Performance vesrion 
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
```


Now, we can use **Tiling** or **Blokcing** in order to split in blocks that fit in L1 and save some extra CPU cycles. The `BLOCK_SIZE` is an important parameter here, since it will define if the data fit in L1.

Doing the math for a double of 8 bytes, we have matrix A, B and one of the same dimenstions to hold the results - so 3 in total.

3* (*Block* * *Block* * 8 bytes) <= 32768 bytes (32 KiB, L1 size)


Then *Block* should be *Block* <= 37, so we choose 32 safely, which is often a sweet spot for modern CPUs.

```cpp
template<typename T> 
void multiply_performance_tilling(const std::vector<T>& a, const std::vector<T>& b, std::vector<T>& result, int const N) {
    std::fill(result.begin(), result.end(), 0);

    // Choose a block size of 32 given our math above
    const int BLOCK_SIZE = 32; 

    // Outer loops: Iterate over tiles
    for (int i_tile = 0; i_tile < N; i_tile += BLOCK_SIZE) {
        for (int k_tile = 0; k_tile < N; k_tile += BLOCK_SIZE) {
            for (int j_tile = 0; j_tile < N; j_tile += BLOCK_SIZE) {

                // Inner loops: Perform multiplication within the tiles
                // Note: std::min handles cases where N is not perfectly divisible by BLOCK_SIZE
                for (int i = i_tile; i < std::min(i_tile + BLOCK_SIZE, N); ++i) {
                    for (int k = k_tile; k < std::min(k_tile + BLOCK_SIZE, N); ++k) {
                        
                        auto temp = a[i * N + k];
                        int row_i = i * N;
                        int row_k = k * N;

                        for (int j = j_tile; j < std::min(j_tile + BLOCK_SIZE, N); ++j) {
                            result[row_i + j] += temp * b[row_k + j];
                        }
                    }
                }

            }
        }
    }
}
```


Below I have the functions I used to benchmark them, I just populate the data and I say  `benchmark::DoNotOptimize(data);` which actually fakes the compiler. It is like saying: I am doing something here, better leave it as is. If we skip it, the compiler would just wipe out the data because it sees we do not perform anything on these tables.



```cpp
// Now the Benchmarks for all the above:
template <typename T>
static void BM_Multiply_Perf_Tilling_Template(benchmark::State& state) {
    int N = state.range(0);

    std::vector<T> m1(N * N, 10.41);
    std::vector<T> m2(N * N, 20.09);
    std::vector<T> m3(N * N);

    for (auto _ : state) {
        multiply_performance_tilling(m1, m2, m3, N);
        benchmark::DoNotOptimize(m3.data());
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


// Tests with different N to have a general picture
BENCHMARK_TEMPLATE(BM_Multiply_Naive_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Tilling_Template, double)->Arg(1024);

BENCHMARK_TEMPLATE(BM_Multiply_Naive_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Tilling_Template, double)->Arg(1024);

BENCHMARK_TEMPLATE(BM_Multiply_Naive_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Tilling_Template, double)->Arg(1024);

BENCHMARK_TEMPLATE(BM_Multiply_Naive_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Tilling_Template, double)->Arg(1024);
BENCHMARK_MAIN();
```

---

Compile with:

```zsh
g++ -O3 -march=native cache_locality_matrix.cpp -o cache_perf_test.exe -lpthread -lbenchmark 
```

We use aggresive optimization of `-O3` for `SIMD vectorization` apart from the linear prefetcher, to get even even better results. Given that we run on an old hardware I use also native arch flag to activate SIMD - otherwise the compiler prevented it in my case. 




## Benchmark Results

Note that you can run the code to test it directly in your machine. Of course, results will vary per hardware. I have optimized the Blocking version above for my given hardware **based on my L1 cache**.

``` bash
./cache_perf_test.exe 
2026-02-24T16:49:02+01:00
Running ./cache_perf_test.exe
Run on (4 X 2494.28 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 256 KiB (x2)
  L3 Unified 3072 KiB (x1)
Load Average: 0.88, 0.88, 0.63
-----------------------------------------------------------------------------------------
Benchmark                                               Time             CPU   Iterations
-----------------------------------------------------------------------------------------
BM_Multiply_Naive_Template<double>/32               30977 ns        30783 ns        21123
BM_Multiply_Perf_Template<double>/32                 8646 ns         8603 ns        82986
BM_Multiply_Perf_Tilling_Template<double>/32        10786 ns        10722 ns        60000
BM_Multiply_Naive_Template<double>/96              844889 ns       841980 ns          793
BM_Multiply_Perf_Template<double>/96               228622 ns       227727 ns         3122
BM_Multiply_Perf_Tilling_Template<double>/96       255608 ns       254843 ns         2651
BM_Multiply_Naive_Template<double>/320           40599337 ns     40463478 ns           13
BM_Multiply_Perf_Template<double>/320            10196626 ns     10159836 ns           61
BM_Multiply_Perf_Tilling_Template<double>/320    10222128 ns     10187554 ns           64
BM_Multiply_Naive_Template<double>/1024        9695255635 ns   9637606553 ns            1
BM_Multiply_Perf_Template<double>/1024          626099493 ns    617875934 ns            1
BM_Multiply_Perf_Tilling_Template<double>/1024  549344328 ns    545949753 ns            1
BM_Multiply_Naive_Template<double>/2048        1.0114e+11 ns   1.0054e+11 ns            1
BM_Multiply_Perf_Template<double>/2048         5701418543 ns   5626022840 ns            1
BM_Multiply_Perf_Tilling_Template<double>/2048 4568252067 ns   4546480151 ns            1
```


``` bash
./cache_perf_test.exe 
2026-02-24T12:21:10+01:00
Running ./cache_perf_test.exe
Run on (4 X 2270.21 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 256 KiB (x2)
  L3 Unified 3072 KiB (x1)
Load Average: 0.83, 0.81, 0.73


-----------------------------------------------------------------------------------------
Benchmark                                               Time             CPU   Iterations
-----------------------------------------------------------------------------------------
BM_Multiply_Naive_Template<double>/32               29684 ns        29529 ns        23667
BM_Multiply_Perf_Template<double>/32                14096 ns        14060 ns        44138
BM_Multiply_Perf_Tilling_Template<double>/32        11775 ns        11735 ns        56519
BM_Multiply_Naive_Template<double>/96              840709 ns       837230 ns          752
BM_Multiply_Perf_Template<double>/96               436760 ns       433365 ns         1458
BM_Multiply_Perf_Tilling_Template<double>/96       338696 ns       335899 ns         2070
BM_Multiply_Naive_Template<double>/320           48389745 ns     47677130 ns           15
BM_Multiply_Perf_Template<double>/320            23002329 ns     22579461 ns           31
BM_Multiply_Perf_Tilling_Template<double>/320    13449012 ns     13321432 ns           42
BM_Multiply_Naive_Template<double>/1024        1.2313e+10 ns   1.2205e+10 ns            1
BM_Multiply_Perf_Template<double>/1024          999436845 ns    978583268 ns            1
BM_Multiply_Perf_Tilling_Template<double>/1024  623033424 ns    617335362 ns            1
```

## N = 32 

At N=32, the entire working set fits within a standard 32 KiB L1 cache. Because the data never has to be evicted to slower memory layers, the specific algorithm choice matters very little.

* **Matrix A (*32 * 32*):** 8 KiB   
* **Matrix B (*32 * 32*):** 8 KiB
* **Result (*32 * 32*):** 8 KiB
* **Total Working Set:** **24 KiB**

> **Analysis:** Since 24  KiB < 32  KiB  (L1 size), the CPU loads these matrices once and they never leave the L1 cache. So we don't see any effect.


## N = 96



---

### Performance Comparison by Scale

| Matrix Size (*N*) | Performance Winner | Reasoning |
| :--- | :--- | :--- |
| ***N=32*** | **Tie** (Tiling ≈ IKJ) | Overhead is negligible; everything stays in L1. |
| ***N=96*** | **IKJ Order** | Tiling disrupts the L2 prefetcher and adds loop overhead. |
| ***N=1024*** | **Tiling** | Prevents **"Cache Thrashing"** in the L3 and RAM. |

---

### Key Takeaways
1. ***N=32*:** Tiling and IKJ are roughly equal because the hardware isn't being pushed to its capacity.
2. ***N=96*:** IKJ wins because it maintains a linear access pattern that the prefetcher loves, whereas tiling breaks that flow.
3. ***N=1024*:** Tiling wins big. At this scale, the cost of a cache miss (going to RAM) is so high that the extra loop overhead of tiling is a small price to pay for keeping data local.

### Deeper Dive with perf


Let's see what is going on for the scenarios where data 


We compared a **Naive** matrix multiplication (`i-j-k` loops) **vs** an **Optimized** version (`i-k-j` loops) **vs** **Blocking** version.


---

## Analysis: Why the 5.1x Speedup?

We managed to get **~5.1x faster** calculations just by changing the for-loop index.

### 1. Linear Access vs. Strided Access
On the **Naive** implementation, we iterate over `i-j-k`. In this pattern, the `b` vector needs to jump `k * N` every time `k` is incremented. This results in "strided" access, which is the enemy of the CPU cache. The prefetcher will load the next elements of the vector in the cache but they are useless in our case since we do not need the next elements but a stride of them. And cache is small so the CPU takes them from RAM, slowing us down.



On the **Performance** version, we iterate over `i-k-j`, so the `b` vector has **linear access**. The compiler and hardware are smart enough to prefetch the data: while we operate on the `j^{th}` element, the CPU loads the `(j+1)^{th}` and `(j+2)^{th}` elements into the cache before they are even requested. Now we saved the extra cycles "walking" to RAM.

### 2. SIMD Vectorization
Because the data is contiguous, the compiler (especially with `-O3` optimization) can use **AVX instructions** for **SIMD (Single Instruction, Multiple Data)** vectorization. This allows the CPU to calculate multiple multiplications in a single clock cycle.

## Conclusion:

When writing HPC code, how you traverse your data is often more important than the algorithm itself. Keep them linear, and in cache.


We do in order to..


``` bash
sudo sysctl -w kernel.perf_event_paranoid=-1
```

``` bash
perf stat -e cycles,instructions,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses -v ./cache_perf_test.exe --benchmark_filter=Tilling
```



``` bash
perf stat -e cycles,instructions,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses -v ./cache_perf_test.exe --benchmark_filter=Tilling
```

``` bash
perf stat -e \
L1-dcache-load-misses,\
l2_rqsts.references,\
l2_rqsts.miss,\
LLC-load-misses \
./cache_perf_test.exe --benchmark_filter=Tilling
``` 
