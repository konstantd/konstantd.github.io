+++
date = '2026-02-12T11:06:26+01:00'
draft = false
title = 'Avoid the RAM Latency: Keeping the Cache Hot and on Linear Access is the Ultimate C++ Optimization'
summary = 'In this benchmark, we explore the importance of keeping data within the CPU cache to avoid expensive retrieval from RAM. By simply ensuring linear data access and accessing by blocks that fit in L1 and L2, we can achieve massive performance gains without changing the underlying algorithm.'
tags = ["advanced-level", "HPC", "cache-locality", "performance", "blocking", "tiling", "simd", "DOD", "AoS", "perf"]
+++

TODO:
// std::span

// AoS design, avoid runtime dispatch but here we just demo on a simple matrix multip

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


Now, we can use **Tiling** or **Blokcing** in order to split in blocks that fit in L1 and L2 and save some extra CPU cycles:

```cpp
template<typename T> 
void multiply_performance_tilling(const std::vector<T>& a, const std::vector<T>& b, std::vector<T>& result, int const N) {
    std::fill(result.begin(), result.end(), 0);

    // Choose a block size. 32 or 64 is often a sweet spot for modern CPUs.
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


// Tests with N = 1024 
// 
// One array of 1024 * 1024 * 8 will fit in L3 (in my machine 12 MB)
BENCHMARK_TEMPLATE(BM_Multiply_Naive_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Template, double)->Arg(1024);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Tilling_Template, double)->Arg(1024);
BENCHMARK_MAIN();
```

---

Compile with:

```zsh
g++ -03 cache_locality_matrix.cpp -o cache_perf_test.exe -lpthread -lbenchmark 
```

We use aggresive optimization of `-O3` for `SIMD vectorization` apart from the linear prefetcher, to get even even better results




## Benchmark Results



The following hardware specs were used to run these benchmarks. Note that you can find the code in my github repo and run it to test it in your machine. Of course, results will vary per hardware.

| Component | Specification |
| :--- | :--- |
| **CPU** | 12 X 2712 MHz |
| **L1 Data Cache** | 48 KiB (x6) |
| **L1 Instruction Cache** | 32 KiB (x6) |
| **L2 Unified Cache** | 512 KiB (x6) |
| **L3 Unified Cache** | 12288 KiB (x1) |




| Benchmark | Time (Wall) | CPU Time | Iterations |
| :--- | :--- | :--- | :--- |
| `BM_Multiply_Naive<double>/1024` | 1,946,863,116 ns | 1,946,797,038 ns | 1 |
| `BM_Multiply_Perf<double>/1024` | 379,033,700 ns | 379,018,676 ns | 2 |



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



```
perf stat -e cycles,instructions,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses -v ./cache_perf_test.exe --benchmark_filter=Tilling
Control descriptor is not initialized
Warning:
kernel.perf_event_paranoid=2, trying to fall back to excluding kernel and hypervisor  samples
Warning:
kernel.perf_event_paranoid=2, trying to fall back to excluding kernel and hypervisor  samples
Warning:
kernel.perf_event_paranoid=2, trying to fall back to excluding kernel and hypervisor  samples
Warning:
kernel.perf_event_paranoid=2, trying to fall back to excluding kernel and hypervisor  samples
Warning:
kernel.perf_event_paranoid=2, trying to fall back to excluding kernel and hypervisor  samples
Warning:
kernel.perf_event_paranoid=2, trying to fall back to excluding kernel and hypervisor  samples
Warning:
kernel.perf_event_paranoid=2, trying to fall back to excluding kernel and hypervisor  samples
Warning:
kernel.perf_event_paranoid=2, trying to fall back to excluding kernel and hypervisor  samples
2026-02-23T22:15:48+01:00
Running ./cache_perf_test.exe
Run on (4 X 2700 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 256 KiB (x2)
  L3 Unified 3072 KiB (x1)
Load Average: 1.13, 0.39, 0.24
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
-----------------------------------------------------------------------------------------
Benchmark                                               Time             CPU   Iterations
-----------------------------------------------------------------------------------------
BM_Multiply_Perf_Tilling_Template<double>/1024  580543215 ns    569520099 ns            1
cycles:u: 1512989646 606248302 301883163
instructions:u: 3680757891 606248302 378010534
cache-references:u: 161727837 606248302 378602689
cache-misses:u: 5945403 606248302 379596198
L1-dcache-loads:u: 1252137815 606248302 380573631
L1-dcache-load-misses:u: 214937003 606248302 379914987
LLC-loads:u: 51702197 606248302 302625598
LLC-load-misses:u: 265743 606248302 301796942

 Performance counter stats for './cache_perf_test.exe --benchmark_filter=Tilling':

     1,512,989,646      cycles:u                                                                (49.80%)
     3,680,757,891      instructions:u                   #    2.43  insn per cycle              (62.35%)
       161,727,837      cache-references:u                                                      (62.45%)
         5,945,403      cache-misses:u                   #    3.68% of all cache refs           (62.61%)
     1,252,137,815      L1-dcache-loads:u                                                       (62.78%)
       214,937,003      L1-dcache-load-misses:u          #   17.17% of all L1-dcache accesses   (62.67%)
        51,702,197      LLC-loads:u                                                             (49.92%)
           265,743      LLC-load-misses:u                #    0.51% of all LL-cache accesses    (49.78%)

       0.612500850 seconds time elapsed

       0.578505000 seconds user
       0.016582000 seconds sys

```




``` bash
perf stat -e \
L1-dcache-load-misses,\
l2_rqsts.references,\
l2_rqsts.miss,\
LLC-load-misses \
./cache_perf_test.exe --benchmark_filter=Tilling
``` 
