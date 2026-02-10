+++
date = '2026-02-10T11:24:55+01:00'
draft = false
title = "Don't Break NRVO on return"
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

### 🚩 The Common Misconception
It looks intuitive: *“I’m returning a local variable that is expensive to copy, so I should use `std::move` to ensure it's efficient!”* **The Reality:** You are likely making your code **slower**.

---

### 🚀 How Modern Compilers Work: NRVO
Modern C++ compilers use **NRVO** (Named Return Value Optimization) or **Copy Elision**. Instead of a "Create -> Copy -> Destroy" cycle, the compiler optimizes the memory management:

* **Standard Logic:**
    1.  Create `result` in the function’s stack frame.
    2.  Copy or move `result` to the caller’s context.
    3.  Destroy `result` in the function’s stack frame.
* **NRVO Logic:**
    1.  **Construct `result` directly in the caller’s memory.**

This effectively reduces the cost to **zero operations**.



---

### ⚠️ The Catch: "Pessimization"
NRVO has strict rules. For it to work, the `return` statement must return the variable **by name**. 

When you write `return std::move(result);`:
1.  The compiler sees an expression, not a name.
2.  **NRVO is disabled.**
3.  You force a **Move Operation**.

You have traded a **zero-cost operation** (NRVO) for a **move operation**. This is called *pessimization*. While moving is usually cheaper than copying, it is still more expensive than doing nothing.

---

### ✅ The Fix
Keep it simple. Just return the variable by name:

```cpp
// ❌ BAD: Forces a move, kills NRVO
return std::move(result);

// ✅ GOOD: Allows the compiler to use NRVO (Zero cost)
return result;


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