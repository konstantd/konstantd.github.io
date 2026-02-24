+++
date = '2026-02-24T11:06:26+01:00'
draft = false
title = 'Avoid the RAM Latency: Keeping the Cache Hot and on Linear Access is the Ultimate C++ Optimization'
summary = 'In this benchmark, we explore the importance of keeping data within the CPU cache to avoid expensive retrieval from RAM. By simply ensuring linear data access and accessing by blocks that fit in L1 and L2, we can achieve massive performance gains without changing the underlying algorithm.'
tags = ["advanced-level", "HPC", "cache-locality", "performance", "blocking", "tiling", "simd", "DOD", "AoS", "perf"]
+++



In this benchmark, we explore the importance of keeping data within the CPU cache to avoid expensive retrieval from RAM. By simply ensuring **linear data access**, we can achieve massive performance gains without changing the underlying algorithm. This principle is used also in **Data Oriented Design (DOD)** with **Array of Structures (AoS)** or **Structures of Arrays (SoA)**, since we lay down all our data to fit linearly. Apart from that there is also the benefit, that when we follow **DOD** designs we avoid also the runtime dynamic dispatch on polymorphism. Though here, in our example we will just focus on the benefit of keeping the cache hot with and we will demonstrate the performance gain in a simple matrix multiplication example using the **perf** tool and **google-benchmark**.

We will have 3 scenarios:

1. one bad multiplication that we do not access the data linearly, 
2. then one that we do access the data linearly. We will notice how massive speed we can gain just from this small change. 
3. Then we will try to improve it even more, accessing in **blocks** of size that fit in L1 cache (**tiling**). 

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

BENCHMARK_TEMPLATE(BM_Multiply_Naive_Template, double)->Arg(2048);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Template, double)->Arg(2048);
BENCHMARK_TEMPLATE(BM_Multiply_Perf_Tilling_Template, double)->Arg(2048);

BENCHMARK_MAIN();
```

---

Compile with:

```zsh
g++ -O3 -march=native cache_locality_matrix.cpp \
    -o cache_perf_test.exe -lpthread -lbenchmark 
```

We use aggresive optimization of `-O3` for `SIMD vectorization` apart from the linear prefetcher, to get even even better results. Given that we run on an old hardware I use also native arch flag to activate SIMD - otherwise the compiler prevented it in my case. 



###  Linear Access vs. Strided Access vs. Blocking

On the **Naive** implementation, we iterate over `i-j-k`. In this pattern, the `b` vector needs to jump `k * N` every time `k` is incremented. This results in "strided" access, which is the enemy of the CPU cache. The prefetcher will load the next elements of the vector in the cache but they are useless in our case since we do not need the next elements but a stride of them. And cache is small so the CPU takes them from RAM, slowing us down.



On the **Performance** version, we iterate over `i-k-j`, so the `b` vector has **linear access**. The compiler and hardware are smart enough to prefetch the data: while we operate on the `j^{th}` element, the CPU loads the `(j+1)^{th}` and `(j+2)^{th}` elements into the cache before they are even requested. Now we saved the extra cycles "walking" to RAM.


On the **Blocking** version we follow the same princliple but we take data in bulk that fit in our cache, gaining massive speed.


### SIMD Vectorization

Because the data is contiguous in our 2 good examples (Perf and Blocking), the compiler (especially with `-O3` optimization) can use **AVX instructions** for **SIMD (Single Instruction, Multiple Data)** vectorization. This allows the CPU to calculate multiple multiplications in a single clock cycle.





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


### 1. The Small Scale (N=32 and N=96): "The Blocking Penalty"

**Winner: Linear Access**

At these sizes, the entire dataset fits within a standard 32 KiB L1 cache. Because the data never has to be evicted to slower memory layers, the specific algorithm choice matters very little.


* **Matrix A (*32 * 32*):** 8 KiB   
* **Matrix B (*32 * 32*):** 8 KiB
* **Result (*32 * 32*):** 8 KiB
* **Total Working Set:** **24 KiB**

 Since 24  KiB < 32  KiB  (L1 size), the CPU loads these matrices once and they never leave the L1 cache.


* **Observation:** The "Perf" (IKJ) version is actually ~20% faster than Tiling here.
* **Reason:** Because the data is already in the fast cache, the extra overhead of tiling (extra loops for block offsets, index calculations) acts as a penalty. IKJ's linear access with the hardware prefetcher is doing well here.


### 2. The Mid-Range (N=320): 

**Winner: Blocking**
* **Observation:** Perf (IKJ) and Blocking are quite close here.
* **Reason:** At N=320, the matrices take up roughly **2.3 MiB**. Remember now the above math we did, now we have
  3 * (320 * 320 * 8) bytes which is almost 2.5 MiB -  exceeds the **L2 cache (256 KiB)** but still fits inside the **L3 cache (3 MiB)**. The benefit of tiling starts to slowly appear here.

### 3. Large Scale (N=1024 to N=2048): Blocking wins by far


Once N=2048, the matrices require **96 MiB** of space. This is 32x larger than the **L3 cache**.

* **The Naive (IJK) Disaster:** At N=2048, Naive takes **~100 seconds**, while Tiling takes **~4.5 seconds**. 
* **The Blocking Advantage:** Blocking is now ~20% faster than IKJ and **22x faster** than Naive. 

When we just move data from RAM to cache and do not perform any calculations on them, and also need to evict them, this is called "Cache Thrashing". In the IKJ version, while the row access is good, it is still streaming through 96 MiB of data, that do not fit even on L3. So it takes data from RAM - data that are carried with the cache-line, stores them in cache and evicts the previous data that do not fit anymore, but **will be needed later** for the next row multiplication. Blocking ensures that once a block is pulled from the high-latency RAM into the L3/L2, they are all used for operations and there is no need to evict anything. So it is a massive gain.



---


### Deeper Dive with perf tool


Let's see what is going on for the scenario with matrixes of the big N=2048 that will not fit in our cache at all. With the below flags we can see all the cache and hits on L1, L2 and L3. We will analyse the 3 implementations:


Below command allows kernel-level profiling and hardware counts.


``` bash
sudo sysctl -w kernel.perf_event_paranoid=-1
```


### First the naive:


``` bash
sudo perf stat -e cycles,instructions,cache-references,cache-misses,L1-dcache-loads,\
L1-dcache-load-misses,LLC-loads,LLC-load-misses \
-v -- ./cache_perf_test.exe --benchmark_filter="Naive.*/2048"
```

``` bash
BM_Multiply_Naive_Template<double>/2048 9.7241e+10 ns   9.5302e+10 ns            1
cycles: 236337972490 96969310198 48491131510
instructions: 43806438921 96969310198 60612349587
cache-references: 9312837146 96969310198 60605207473
cache-misses: 8650212874 96969310198 60600566834
L1-dcache-loads: 13119364058 96969310198 60600980737
L1-dcache-load-misses: 12492970228 96969310198 60601988490
LLC-loads: 8870298075 96969310198 48488107158
LLC-load-misses: 8403483781 96969310198 48489258590

 Performance counter stats for './cache_perf_test.exe --benchmark_filter=Naive.*/2048':

   236,337,972,490      cycles                                                                  (50.01%)
    43,806,438,921      instructions                     #    0.19  insn per cycle              (62.51%)
     9,312,837,146      cache-references                                                        (62.50%)
     8,650,212,874      cache-misses                     #   92.88% of all cache refs           (62.49%)
    13,119,364,058      L1-dcache-loads                                                         (62.50%)
    12,492,970,228      L1-dcache-load-misses            #   95.23% of all L1-dcache accesses   (62.50%)
     8,870,298,075      LLC-loads                                                               (50.00%)
     8,403,483,781      LLC-load-misses                  #   94.74% of all LL-cache accesses    (50.00%)

      97.324655915 seconds time elapsed

      95.322590000 seconds user
       0.060503000 seconds sys
```


LLC-load-misses are the L3 misses. Which is 95%. This is HUGE. Almost every single time the CPU looks for data in the L1, L2 or L3 cache, it isn't there. The CPU has to stop everything and wait for the RAM, and this also gives a really-really bad instructions per cycle of 0.19. 



### The Perf - Linear Access:

```bash
perf stat -e cycles,instructions,cache-references,cache-misses,L1-dcache-loads,\
L1-dcache-load-misses,LLC-loads,LLC-load-misses \
-v ./cache_perf_test.exe \
--benchmark_filter="Perf_Template.*/2048"
```

```bash
---------------------------------------------------------------------------------
Benchmark                                       Time             CPU   Iterations
---------------------------------------------------------------------------------
BM_Multiply_Perf_Template<double>/2048 5706398240 ns   5520526706 ns            1
cycles: 14883020444 5791414256 2892144199
instructions: 8810721969 5791414256 3616863209
cache-references: 1864671979 5791414256 3621198456
cache-misses: 1095803386 5791414256 3622645754
L1-dcache-loads: 5436462959 5791414256 3622965093
L1-dcache-load-misses: 1454919790 5791414256 3621470331
LLC-loads: 682781128 5791414256 2892857066
LLC-load-misses: 147854096 5791414256 2892376125

 Performance counter stats for './cache_perf_test.exe --benchmark_filter=Perf_Template.*/2048':

    14,883,020,444      cycles                                                                  (49.94%)
     8,810,721,969      instructions                     #    0.59  insn per cycle              (62.45%)
     1,864,671,979      cache-references                                                        (62.53%)
     1,095,803,386      cache-misses                     #   58.77% of all cache refs           (62.55%)
     5,436,462,959      L1-dcache-loads                                                         (62.56%)
     1,454,919,790      L1-dcache-load-misses            #   26.76% of all L1-dcache accesses   (62.53%)
       682,781,128      LLC-loads                                                               (49.95%)
       147,854,096      LLC-load-misses                  #   21.65% of all LL-cache accesses    (49.94%)

       5.803357483 seconds time elapsed

       5.540186000 seconds user
       0.071809000 seconds sys

```

- L1 Miss Rate: Dropped from 95% to 26%.

- LLC Miss Rate: Dropped from 94% (Naive) to 21%.

Now we are accessing memory in a straight line, the Prefetcher can guess what you need next. It starts pulling data from RAM before you even ask for it.

The instructions per cycle - IPC jumped to 0.59. It is 3x faster, but the CPU is still stalling - we have described the reason above - once we need data for the next row multiplication we need the data with start of this row in cache - previously they were already there but we were out of L3 space, so they got evicted from L3 in LRU order. This means we need AGAIN the same data from RAM. And this gives also a not that good IPC.




### The Blocking/Tilling:

``` bash
perf stat -e cycles,instructions,cache-references,cache-misses,L1-dcache-loads,\
L1-dcache-load-misses,LLC-loads,LLC-load-misses \
-v ./cache_perf_test.exe \
--benchmark_filter="Tilling.*/2048"
```


``` bash
-----------------------------------------------------------------------------------------
Benchmark                                               Time             CPU   Iterations
-----------------------------------------------------------------------------------------
BM_Multiply_Perf_Tilling_Template<double>/2048 4544805539 ns   4428673382 ns            1
cycles: 11913764889 4600547279 2299384402
instructions: 13949810100 4600547279 2874449269
cache-references: 287582812 4600547279 2875806198
cache-misses: 49065998 4600547279 2875890019
L1-dcache-loads: 7695992754 4600547279 2876809499
L1-dcache-load-misses: 1459673513 4600547279 2875351381
LLC-loads: 85428531 4600547279 2300052381
LLC-load-misses: 3111522 4600547279 2298895236

 Performance counter stats for './cache_perf_test.exe --benchmark_filter=Tilling.*/2048':

    11,913,764,889      cycles                                                                  (49.98%)
    13,949,810,100      instructions                     #    1.17  insn per cycle              (62.48%)
       287,582,812      cache-references                                                        (62.51%)
        49,065,998      cache-misses                     #   17.06% of all cache refs           (62.51%)
     7,695,992,754      L1-dcache-loads                                                         (62.53%)
     1,459,673,513      L1-dcache-load-misses            #   18.97% of all L1-dcache accesses   (62.50%)
        85,428,531      LLC-loads                                                               (50.00%)
         3,111,522      LLC-load-misses                  #    3.64% of all LL-cache accesses    (49.97%)

       4.629830187 seconds time elapsed

       4.448874000 seconds user
       0.061816000 seconds sys
```


- LLC-load-misses is now down to 3.5%. The CPU rarely needs to go to RAM to get data, what it needs is almost always in cache. 
- Now for every clock cycle, we get more than 1 instruction, we got IPC from 0.6 to IPC = 1.17. **Excellent!**




---



## Conclusion:

When writing High Performance code, how you traverse your data is often more important than the algorithm itself. Benchmark always, imagive we have a huge computational system and this iterations should run multiple times on different data. Summing all this extra waiting time up makes a big difference. Also, **DO NOT guess, MEAUSRE    directly**. `perf` is an excellent tool to see the cache miss or hits and **understand the hardware**. Keep the data linear, and in cache - as much as possible. 




### Notes


- `std::mdspan` introduced in C++23 and does the Tiling - Blocking part, avoiding the loop we implemented manually
- **Data Oriented Design (DOD)** aims to benefit exactly from this. We might see such an example in a future article.




