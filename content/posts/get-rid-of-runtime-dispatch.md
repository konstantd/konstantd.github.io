+++
date = '2026-05-13T21:55:47+03:00'
draft = true
title = 'Literally Everything You Need to Know About the True Cost of Runtime Dispatch And How to Avoid It'
+++


When writing classic Object-Oriented Programming (OOP) in C++, we are taught to design our systems using polymorphism. We define a base class with virtual functions, inherit from it, and manipulate those objects using base class pointers. 

At runtime, the program dynamically selects the correct function execution based on the actual object type. This is known as **dynamic (or runtime) dispatch**.

While flexible, this design introduces significant hidden costs in terms of memory layout, allocation overhead, and CPU performance. In modern C++, especially for performance-critical or embedded systems, we can entirely eliminate this overhead by shifting to **value semantics** using `std::variant`.

---

## The Classic OOP Approach (and Why It is BAD)

Let’s look at a classic setup. We have a `Vehicle` base class and a couple of derived classes: `Car` and `MotorBike`.

```cpp
#include <iostream>
#include <vector>
#include <memory>
#include <string>

class Vehicle {
public:
    virtual ~Vehicle() = default; // Crucial for OOP, but adds to the overhead!
    virtual void honk() const = 0;
    virtual void reverse() const = 0;
};

class Car : public Vehicle {
    std::string name;
public:
    Car(std::string n) : name(std::move(n)) {}
    void honk() const override { std::cout << name << " goes Honk!\n"; }
    void reverse() const override { std::cout << name << " reverses.\n"; }
};

class MotorBike : public Vehicle {
    std::string name;
public:
    MotorBike(std::string n) : name(std::move(n)) {}
    void honk() const override { std::cout << name << " goes Beep!\n"; }
    void reverse() const override { std::cout << name << " cannot easily reverse.\n"; }
};


If we want to store these mixed types together in a container, classic OOP forces us to use a collection of pointers:

```cpp
int main() {
    // ANTI-PATTERN: Heap allocations, memory fragmentation, and pointer indirection
    using Vehicles = std::vector<std::unique_ptr<Vehicle>>;
    Vehicles vehicles;

    // Manual heap allocations via make_unique
    vehicles.emplace_back(std::make_unique<Car>("Corvette C8"));
    vehicles.emplace_back(std::make_unique<Car>("Ferrari F40"));
    vehicles.emplace_back(std::make_unique<MotorBike>("Yamaha YZ"));
    vehicles.emplace_back(std::make_unique<MotorBike>("BMW R1200GS"));

    // Runtime dispatch in a loop
    for (const auto& v : vehicles) {
        v->honk(); 
    }

    return 0;
}
```




## The Architectural Pitfalls of this Design

Memory Fragmentation & Cache Misses: While the std::vector stores the std::unique_ptr elements contiguously in memory, the actual objects themselves are scattered randomly across the heap. Iterating through the vector forces the CPU to constantly chase pointers to different memory addresses, completely destroying CPU cache efficiency.

Pointer Overhead: On a 64-bit architecture, each pointer takes up 8 bytes of memory. If you have a massive array, you are wasting considerable memory just storing the "maps" to your data. In the embedded world, this can get really bad. 

The OOP Core Traps: By sticking to value inheritance, you open the door to classic C++ bugs:

Object Slicing: Accidentally passing or copying a derived object into a base object by value, which completely truncates the derived data.

Undefined Behavior: Forgetting to declare a virtual ~Vehicle() destructor results in resource leaks when deleting via a base pointer.

Constructor Hazards: Calling virtual functions inside constructors or destructors doesn't work as you'd intuitively expect, because the derived class layer hasn't been built (or has already been destroyed) yet.

## Under the Hood: What is Runtime Dispatch Actually Costing You?

When you mark a function as virtual, the compiler shifts the responsibility of choosing the function from compile-time to runtime using two hidden mechanisms:

The vtable (Virtual Table): A static array of function addresses generated per class. It is constant, shared across all instances, and typically placed by the compiler into ROM/Flash memory (the .rodata or .text sections).

The vptr (Virtual Pointer): A hidden pointer injected into every instance of your object in RAM, pointing to its class vtable. On a 64-bit system, this adds a fixed 8-byte overhead to every single object.

## The RAM Trap

Consider a small embedded class like an LED control:

``` cpp
// Non-virtual version
class Led {
    uint8_t pin; // Size: 1 byte
}; // sizeof(Led) = 1 byte

// Virtual version
class LedVirtual {
    uint8_t pin;
    virtual void toggle(); 
}; // sizeof(LedVirtual) = 8 bytes (4/8 bytes for vptr + 1 byte for pin + padding)
```

## The CPU Step Penalty

When your code executes v->honk();, the CPU can no longer make a direct jump. Instead, it must complete three distinct operations:

Dereference the Object: Load the address of the vtable from the object’s vptr (1st memory access).

Lookup the Address: Go to the specific index in the vtable to fetch the target function's address (2nd memory access).

Indirect Jump: Perform an indirect branch instruction to that address.

Because the actual function target is unknown at compile time, the compiler cannot inline the function. This limits optimization, risks branch misprediction, and makes code execution latency less predictable—a massive problem for real-time or low-latency systems.


The Modern Alternative: Value Semantics via std::variant
If your polymorphic types are known at compile-time (which is true for the vast majority of cases), you can completely eliminate pointers, heap allocations, and vptrs by combining std::variant with std::visit.

Here is the clean, high-performance alternative:

```cpp
#include <iostream>
#include <vector>
#include <variant>
#include <string>

// No base class, no virtual keywords, no inheritance required!
class Car {
    std::string name;
public:
    Car(std::string n) : name(std::move(n)) {}
    void honk() const { std::cout << name << " goes Honk!\n"; }
    void reverse() const { std::cout << name << " reverses.\n"; }
};

class MotorBike {
    std::string name;
public:
    MotorBike(std::string n) : name(std::move(n)) {}
    void honk() const { std::cout << name << " goes Beep!\n"; }
    void reverse() const { std::cout << name << " cannot easily reverse.\n"; }
};

int main() {
    // Define a variant that can hold either a Car OR a MotorBike
    using VehicleVariant = std::variant<Car, MotorBike>;
    std::vector<VehicleVariant> vehicles;

    // NO POINTERS, NO HEAP ALLOCATIONS, ONLY FLAT VALUES!
    vehicles.emplace_back(Car("Corvette C8"));
    vehicles.emplace_back(Car("Ferrari F40"));
    vehicles.emplace_back(MotorBike("Yamaha YZ"));
    vehicles.emplace_back(MotorBike("BMW R1200GS"));

    // Use std::visit to handle compile-time or optimized branch dispatching
    for (const auto& v : vehicles) {
        std::visit([](const auto& vehicle) {
            vehicle.honk();
        }, v);
    }

    return 0;
}
```
### Why This is Drastically Better

Value semantics radically outperform classic OOP by keeping data tightly packed in contiguous memory, completely eliminating heap allocations and the hidden 8-byte `vptr` overhead. While OOP scatters pointers—destroying cache locality and blocking compiler inlining due to runtime evaluation—`std::variant` guarantees strictly safe, pointer-free code that the compiler can fully optimize.

## Summary

By embracing value semantics, you treat your classes like regular data values. You get rid of the messy management of heap pointers, avoid errors and gain performance.

Next time you reach for a virtual keyword, ask yourself: Could this be a `std::variant` instead?