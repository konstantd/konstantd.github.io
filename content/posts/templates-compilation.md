+++
date = '2025-10-10T11:23:23+01:00'
draft = false
title = 'Advanced Template Metaprogramming: Implementing Hardware Constraints with C++20 Concepts'
tags = ["advanced-level", "templates", "concepts", "undefined-behaviour"]
+++



Templates complilation generate code at compile time, before the program is executed (compile-time vs run-time). Every different instantiation produces a separate function at compile time. This way we avoid writing duplication of the code. Also, we can avoid mistakes even on compilation time when we static assert them. As the name suggests, this checks takes place statically - on compilation. Like this, we avoid mistakes that might appear during runtime, when we should not instantiate a function of a specific type. Static-asserts was already introduced in C++11, though the we have some important imporvements in `C++17` like `CTAD` and `concepts` or `requires` in `C++20`. 


Consider the following code. We have some registers from which we would like to read. We can read and cast the result to the type we need directly, without writing different functions.


```cpp

template<typename TCast>
requires (std::is_arithmetic_v<TCast> || std::is_enum_v<TCast>) && 
         std::is_trivially_copyable_v<TCast> && 
         (sizeof(TCast) <= 4)    
TCast readRegDriver(const drivReg* const reg) const {  // This is a const pointer to a const register
    TCast value{};  
    // Take it from the C API
    const DriverStatus status = driver_get(this->driver, const_cast<drivReg*>(reg), &value);
    if ([[unlikely]] status != DriverStatus::Success) { 
            handleReadError(reg, status);
    }
    // Return the value as the requested type
    return value;
}
```

## When will the above code compile?


Now we can read from different registers directly:


```cpp
// Works good, it is arithmetic, trivially copyable and size of 4 bytes
int val = readRegEdge<int>(regs.bfield);  

// Will not work, because the size is 8 bytes, would overead memory 
double val = readRegEdge<double>(regs.bfield);


// Works, enums do pass our filter
enum class StateMode : uint8_t {
    Active = 0,
    Paused = 1
    Disabled = 2,
};


// we can take the enum directly 
StateMode m = readRegEdge<StateMode>(regs.flag);

// Fails
std::string s = readRegDriver<std::string>(regs.name);
```


## When will this compile?

The compiler now acts as a gatekeeper, validating your logic before a single line of code runs on the target.

### ✅ Success Cases
* **`int val = readRegDriver<int>(regs.bfield);`**
  * **Status:** Compiles. It is arithmetic, trivially copyable, and exactly 4 bytes.
* **`StateMode m = readRegDriver<StateMode>(regs.flag);`**
  * **Status:** Compiles. Enums pass our filter, and as long as the underlying type (like `uint8_t`) is <= 4 bytes, it is safe.

### ❌ Failure Cases
* **`double val = readRegDriver<double>(regs.bfield);`**
  * **Status:** **Fails to compile.** A `double` is typically 8 bytes. Our filter catches this, preventing an "over-read" that would grab memory belonging to adjacent registers and cause Undefined Behavior (UB).
* **`std::string s = readRegDriver<std::string>(regs.name);`**
  * **Status:** **Fails to compile.** Strings are not arithmetic or enums, and they are not trivially copyable. This prevents us from corrupting the internal pointers of a complex object via raw memory writes.


Then we should use float instead. This saves us from reading/writing to memory that belongs to other parts and avoids creating UB..

---

## Our filters

### 1. `std::is_arithmetic_v<TCast> || std::is_enum_v<TCast>`
**The "What is it?" Filter**

* **Purpose:** Restricts the function to numeric types (`int`, `float`, `uint32_t`) and `enum` or `enum class`. Note that all standard signed and unsigned integers, booleans and char types pass this filter.
* **The Reason:** The `driver_get` function writes raw bits into a memory address. If a user tried to read BRAM data into a complex object like a `std::string`, it would overwrite the object's internal state and lead to a crash. Including `is_enum` allows us to maintain strongly-typed states without manual casting. Also pointers, arrays, void and arrays are filtered out.

### 2. `std::is_trivially_copyable_v<TCast>`
**The "Is it safe to bit-copy?" Filter**

* **Purpose:** Ensures the type can be safely handled by raw memory operations (like `memcpy`).
* **The Reason:** C++ objects that are "trivially copyable" do not have virtual tables (vtables) or managed pointers, like a vector or a dequeue container for example. We just want to do a `memcpy` from hardware registers. **Copying like that from a complex container is classic UB**.


### 3. `(sizeof(TCast) <= 4)`
**The "Physical Hardware" Filter**

* **Purpose:** Matches the constraints of a 32-bit BRAM architecture.
* **The Reason:** Since a single BRAM word is 32 bits (4 bytes), trying to read a 64-bit `double` would be invalid. This check forces the developer to use a `float` or `int32_t` instead, ensuring we never read or write to memory that doesn't belong to the target register.



---

## The Takeaway

In critical systems, the sooner you fail, the better. By providing hardware constraints using `C++20 Concepts`, we eliminate entire categories of memory corruption bugs at compile time, long before the code ever starts up.


---

{{< social_icons_extend_with_subscribe >}}