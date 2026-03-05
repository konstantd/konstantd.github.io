# cache-locality-test
Cache locality benchmarking on simple matrix multiplication 


Compile with: 

``` bash
g++ -03 cache_locality_matrix.cpp -o cache_perf_test.exe -lpthread -lbenchmark 
```

| Benchmark                                   | Time (ns)    | CPU (ns)     | Iterations |
|--------------------------------------------|--------------|--------------|------------|
| BM_Multiply_Naive_Template<double>/1024    | 1,946,863,116 | 1,946,797,038 | 1          |
| BM_Multiply_Perf_Template<double>/1024     |   379,033,700 |   379,018,676 | 2          |



In the Performance version, both the result vector and matrix B are accessed linearly, so the prefetcher pulls the next blocks of memory into the cache before the loop even reaches them. Because the data is contiguous, the compiler's -O3 optimization can use AVX instructions (SIMD vectorization) to calculate 8 multiplications in a single clock cycle.


We achive ~5.1x times faster calculations just by changing the for loop index. 
