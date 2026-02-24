+++
date = '2026-01-10T11:24:55+01:00'
draft = false
title = "De-Optimizing C++: How Manual Moves Kills NRVO"
summary = 'Returning a moved big object will actually create extra overhead for the compiler, as a zero-cost operation is traded for a cheap move operation.'
tags = ["mid-level", "performance"]
+++



Consider the following code.


```cpp
#include <utility>
#include<vector>

std::vector<double> createSuperExpensiveData() {

  // result is constructed locally
  std::vector<double> result(10000, 10.0);
  // ....
  return std::move(result);  // breaks RVO 
}

```

# C++ Optimization: Don't `std::move` Your Returns

### The Common Misconception

It looks intuitive: *“I’m returning a local variable that is expensive to copy, so I should use `std::move` to ensure it's efficient!”* **The Reality:** You are likely making your code **slower**.

---

### Modern Compilers & NRVO

Modern C++ compilers use **NRVO** (Named Return Value Optimization) or **Copy Elision**. Instead of a "Create -> Copy -> Destroy" cycle, the compiler optimizes the memory management:

* **Standard Logic:**
    1.  Create `result` in the function’s stack frame.
    2.  Copy or move `result` to the caller’s context.
    3.  Destroy `result` in the function’s stack frame.
* **NRVO Logic:**
    1.  **Construct `result` directly in the caller’s memory.**

This effectively reduces the cost to **zero operations**.



---

###  Pessimization

NRVO has strict rules. For it to work, the `return` statement must return the variable **by name**. 

When you write `return std::move(result);`:
1.  The compiler sees an expression, not a name.
2.  **NRVO is disabled.**
3.  You force a **Move Operation**.

You have traded a **zero-cost operation** (NRVO) for a **move operation**. This is called *pessimization*. While moving is usually cheaper than copying, it is still more expensive than doing nothing.

---



### Let's have a look on the assembly


See the below generated code when we compile with -02 flag. 


``` assembly

    ; Without RVO - std::move
    createSuperExpensiveData():
            push    rbx
            mov     rbx, rdi
            mov     edi, 80000
            call    operator new(unsigned long)
            movsd   xmm0, QWORD PTR .LC0[rip]
            mov     rcx, rax
            lea     rdx, [rax+80000]
    .L2:
            movsd   QWORD PTR [rax], xmm0
            add     rax, 16
            movsd   QWORD PTR [rax-8], xmm0
            cmp     rax, rdx
            jne     .L2
            mov     QWORD PTR [rbx+8], rax
            mov     QWORD PTR [rbx+16], rax
            mov     rax, rbx
            mov     QWORD PTR [rbx], rcx
            pop     rbx
            ret
    .LC0:
            .long   0
            .long   1076101120
    
```



And this is the generated code when I use RVO, so just `return result;`.


``` assembly

    ; With RVO - just return result
    createSuperExpensiveData():
        push    rbx
        mov     rbx, rdi
        mov     edi, 80000
        call    operator new(unsigned long)
        movsd   xmm0, QWORD PTR .LC0[rip]
        lea     rdx, [rax+80000]
        mov     QWORD PTR [rbx], rax
        mov     QWORD PTR [rbx+16], rdx
    .L2:
        movsd   QWORD PTR [rax], xmm0
        add     rax, 16
        movsd   QWORD PTR [rax-8], xmm0
        cmp     rax, rdx
        jne     .L2
        mov     QWORD PTR [rbx+8], rax
        mov     rax, rbx
        pop     rbx
        ret
    .LC0:
        .long   0
        .long   1076101120
```


Let's not bother much about the assembly instructions and let us just check on the `.L2 label`, which runs the initialiazation loop of the vector. The CPU spends many `mov` instructions before the final `mov` of the caller's address when we don't use RVO.
The compiler is trying to do RVO directly, but it fails, so it forces the move. There is still no copy but as you can see above there is some extra overhead. 

**In the RVO version, the pointers are set before the loop even starts.**


### The Simple Fix


Keep it simple. Just return the variable by name:


```cpp
#include <utility>
#include<vector>

std::vector<double> createSuperExpensiveData() {

  // result is constructed locally
  std::vector<double> result(10000, 10.0);
  // ....
  return result;  // This is simple and more efficient
}

```





---

{{< social_icons_extend_with_subscribe >}}


