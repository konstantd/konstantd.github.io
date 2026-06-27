

+++
date = '2026-06-27T18:05:10+03:00'
draft = false
title = 'Avoid Locks And Use Memory Orderding to Eliminate Context Swicthes - How to Track Them'
+++


While it's running find the PID

```Shell
pidof exe
```

Now watch it:

```Shell
watch -n 0.5 grep ctxt /proc/21787/status
```

This only includes the main thread so 2 context swictes

```Shell

pidstat -w -t -p 21787 1

pidstat -w -t -p 11947 1

Linux 6.18.33.1-microsoft-standard-WSL2 (Kostas)        06/27/26     _x86_64_        (16 CPU)



16:39:08      UID      TGID       TID   cswch/s nvcswch/s  Command

16:39:09     1000     11947         -      0.00      0.00  exe

16:39:09     1000         -     11947      0.00      0.00  |__exe

16:39:09     1000         -     11948  19020.00      0.00  |__exe

16:39:09     1000         -     11949   3082.00      0.00  |__exe

16:39:10     1000     11947         -      0.00      0.00  exe

16:39:10     1000         -     11947      0.00      0.00  |__exe

16:39:10     1000         -     11948  18390.00      0.00  |__exe

16:39:10     1000         -     11949   2964.00      0.00  |__exe

16:39:11     1000     11947         -      0.00      0.00  exe

16:39:11     1000         -     11947      0.00      0.00  |__exe

16:39:11     1000         -     11948  18424.00      0.00  |__exe

16:39:11     1000         -     11949   3049.00      2.00  |__exe

16:39:12     1000     11947         -      0.00      0.00  exe

16:39:12     1000         -     11947      0.00      0.00  |__exe

16:39:12     1000         -     11948  18168.00      0.00  |__exe

16:39:12     1000         -     11949   2953.00      0.00  |__exe

16:39:13     1000     11947         -      0.00      0.00  exe
```

* **cswch/s (Voluntary Context Switches per Second):** This is the massive number. This happens every time your thread hits `cv.wait()` and willingly goes to sleep because the buffer is either full or empty.
* **nvcswch/s (Involuntary Context Switches per Second):** This happens when the OS forcefully kicks your thread off the CPU because its time is up. Notice this is basically `0`. Your threads are yielding the CPU *voluntarily* long before the OS has to force them off.

Without sudo you only get the user space and woiuld give 0


### How this applies to your Lock-Free Code

When you used `std::mutex`, you had massive **Voluntary** switches and 0 involuntary, because the threads yielded long before their time slice ran out.

If you were to run `pidstat` on your new  **Lock-Free Spin Loop** , you would see the exact opposite!
Because your `std::atomic` `while()` loops never yield, they never trigger voluntary switches. Instead, they will aggressively burn CPU cycles until the OS timer eventually runs out, forcing an **Involuntary** context switch just to keep the rest of your Linux system from freezing up.




sudo perf stat -e context-switches,cpu-migrations ./exe

 Performance counter stats for './exe':

    673,897      context-switches
    73      cpu-migrations

    10.082698722 seconds time elapsed

    0.657872000 seconds user
       2.428806000 seconds sys


WITH LOCKS:

```C++
#include <iostream>
#include <thread>
#include <atomic>
#include <array>


// real    0m11.175s
// user    0m11.908s
// sys     0m9.448s
const int MAX_SIZE = 10;        // Keep the buffer tiny to force contention
const int TOTAL_ITEMS = 1000000; // 1 Million items

// 1. Replace std::queue with a fixed-size array (Ring Buffer)

// this can be used also with more complex objects than int?
std::array<int, MAX_SIZE> buffer;

// 2. Use atomics for our indices instead of a mutex
std::atomic<size_t> head{0}; // Producer writes here
std::atomic<size_t> tail{0}; // Consumer reads here

void producer() {
    for (int i{0}; i < TOTAL_ITEMS; ++i) {
        size_t current_head = head.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % MAX_SIZE;

        // THE SPIN: If buffer is full, do NOT sleep. 
        // Just spin in an endless loop until the consumer changes 'tail'.
        while (next_head == tail.load(std::memory_order_acquire)) {
            // Actively doing nothing. No OS context switch!
        }

        // Produce
        buffer[current_head] = i;
      
        // Update the head pointer so the consumer can see it
        head.store(next_head, std::memory_order_release);
    }
}

void consumer() {
    while(true) {
        size_t current_tail = tail.load(std::memory_order_relaxed);

        // THE SPIN: If buffer is empty, do NOT sleep.
        // Spin in an endless loop until the producer changes 'head'.
        while (current_tail == head.load(std::memory_order_acquire)) {
             // Actively doing nothing. No OS context switch!
        }

        // Consume
        int val = buffer[current_tail];
        std::cout << val << "\n";

        // Update the tail pointer so the producer can see space opened up
        tail.store((current_tail + 1) % MAX_SIZE, std::memory_order_release);

        if (val == TOTAL_ITEMS - 1) break;
    }
}

int main() {
    std::cout << "Starting Lock-Free SPSC Queue..." << std::endl;
  
    std::jthread prod(producer);
    std::jthread cons(consumer);
  
    return 0;
}
```




MEM ORDERING:

```C++
#include <iostream>
#include <thread>
#include <atomic>
#include <array>


// real    0m11.175s
// user    0m11.908s
// sys     0m9.448s
const int MAX_SIZE = 10;        // Keep the buffer tiny to force contention
const int TOTAL_ITEMS = 1000000; // 1 Million items

// 1. Replace std::queue with a fixed-size array (Ring Buffer)

// this can be used also with more complex objects than int?
std::array<int, MAX_SIZE> buffer;

// 2. Use atomics for our indices instead of a mutex
std::atomic<size_t> head{0}; // Producer writes here
std::atomic<size_t> tail{0}; // Consumer reads here

void producer() {
    for (int i{0}; i < TOTAL_ITEMS; ++i) {
        size_t current_head = head.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % MAX_SIZE;

        // THE SPIN: If buffer is full, do NOT sleep. 
        // Just spin in an endless loop until the consumer changes 'tail'.
        while (next_head == tail.load(std::memory_order_acquire)) {
            // Actively doing nothing. No OS context switch!
        }

        // Produce
        buffer[current_head] = i;
      
        // Update the head pointer so the consumer can see it
        head.store(next_head, std::memory_order_release);
    }
}

void consumer() {
    while(true) {
        size_t current_tail = tail.load(std::memory_order_relaxed);

        // THE SPIN: If buffer is empty, do NOT sleep.
        // Spin in an endless loop until the producer changes 'head'.
        while (current_tail == head.load(std::memory_order_acquire)) {
             // Actively doing nothing. No OS context switch!
        }

        // Consume
        int val = buffer[current_tail];
        std::cout << val << "\n";

        // Update the tail pointer so the producer can see space opened up
        tail.store((current_tail + 1) % MAX_SIZE, std::memory_order_release);

        if (val == TOTAL_ITEMS - 1) break;
    }
}

int main() {
    std::cout << "Starting Lock-Free SPSC Queue..." << std::endl;
  
    std::jthread prod(producer);
    std::jthread cons(consumer);
  
    return 0;
}
```
