+++
date = '2026-05-13T21:55:47+03:00'
draft = true
title = 'Get Rid of Runtime Dispatch'
+++


When using OOP it is classic to design our classes using polymorhism and use pointers to the base class. Then objectes will inherit from a base clase, which will have some virtual functions and then depending the object type the corresponding function will be called. This introduces the dynamic dispath. Dynamic dispatch selects a polymorhic operation to be called during runtime.

Note that for dynamic polymorhism., virtual functions are needed to act on a pointer of the Base class. 


As an example:

```cpp
class Vehicle {
    virtual void honk();
    virtual void reverse();
};


class Car : public Vehicle {
    void honk() override;
    void reverse() override;
};


class MotorBike : public Vehicle {
    void honk() override;
    void reverse() override;
}
```


```cpp
int main() {
    // THIS IS A BAD DESIGN - VIRTUAL DISPATCH ON THE POINTER AND INHERITANCE
    using Vehicles = std::vector<std::unique<Vehicle>>;
    Vehicles vehicles;
    // Fill with nice Cars
    vehicles.emplace_back(std::make_unique<Car>("Corvette C8"));
    vehicles.emplace_back(std::make_unique<Car>("Ferrari F40"));
    vehicles.emplace_back(std::make_unique<Car>("Toyota Supra MK4"));
    vehicles.emplace_back(std::make_unique<Car>("Nissan Skyline R34"));
    
    // ALSO MANUAL ALLOCATIONS!!!  --- ???????? 

    // Fill with Bikes 
    vehicles.emplace_back(std::make_unique<MotorBike>("Yamaha YZ"));
    vehicles.emplace_back(std::make_unique<MotorBike>("Kawasaki Hayabusa"));
    vehicles.emplace_back(std::make_unique<MotorBike>("BMW R1200GS"));
    

    for (auto v : vehicles) {
        v->honk();
    }


    return 0;
}
```



## Why The Above is A Bad Design?

The above implementation uses virtual functions which introduces virtual dispatch and is used with a vector of pointers. The pointers are stored consecutively in memory but they point to different places in memory to store the objects, which create memory fragmentation. Also, depending on the machine, on a 64-bit architecture a pointer will be 8 bytes and this on a big vector occupies a lot of memory. The impliciation of addding virtual to a function of a class is 
that now the class contains a vptr and a vtable. The hidden pointer in a 64bit machine is 8 bytes, so the class is also 8 bytes bigger now.



# What About the Runtime Dispatch

Behind the scenes every call in the loop means the exact function (Car::honk or MotorBike::honk) will be chosen at runtime, depending on what kind of object d actually points to.


Internally, each object of a class with virtual functions has a vtable pointer, a hidden pointer to a table of function addresses.

When you call `v->honk();` the CPU must look up the vtable pointer inside `v` object, jump to the correct function address stored in that table and execute it. So, the dispatch (decision of which honk() to call) happens at runtime, not at compile time.



# Why that is slower and less predictable?

It adds one extra memory read and one indirect jump per call. The compiler can’t inline it, because the actual function isn’t known during compilation. This introduces branch misprediction risk and it is bad for hardware control loops or real-time critical systems.





# Value Semantics Solution



We can avoid the extra memory as well as the runtime overhead with `variant`. 
``` cpp
int main() {


    using Vehicles = std::variant<Car,Motorbike>;


    std::vector<Vehicles> vehicles;

    // Creating some shapes

    // NO POINTERS, NO ALLOCATION, ONLY VALUES!!!
    vehicles.emplace_back(Car("Corvette C8"));
    vehicles.emplace_back(Car("Ferrari F40"));
    vehicles.emplace_back(Car("Toyota Supra MK4"));
    vehicles.emplace_back(Car("Nissan Skyline R34"));

    // Fill with Bikes 
    vehicles.emplace_back(MotorBike("Yamaha YZ"));
    vehicles.emplace_back(MotorBike("Kawasaki Hayabusa"));
    vehicles.emplace_back(MotorBike("BMW R1200GS"));

    return 0;

}
```






// When marking a fun as virtual 

We have on RAM another pointer to the function table. This is the vpointer. This is usually the 1st 64-bits on the address of the object.

Also the static array - vtable is stored in ROM and stores all the addresses of the className::VirtualFunction1, className::VirtualFunction2 etc.

vtable size ≈ N × 8 bytes

Where:

N = number of virtual function entries


It goes to ROM because it is constant, shared, and known at link time.


So a dynamic dispatch, goes to the vpointer and then searches the vtable for the function to jump to.

This adds some overhead extra CPU instructions and RAM. ROM is perfect for shared constants.




/*
The implciation of addding virtual to  a function of a class is 
that now the class contains a vptr and a vtable. The hidden pointer in a 64bit machine is 8 bytes, so the class
is 8 bytes bigger now.

Usually a class is 1 byte. 


In C++, an empty class is 1 byte because the language requires every object to have a unique memory address.

More on that, why? Later.

If you have a static variable, this does not add to the class size.This goes to ROM. The vptr is in RAM.

*/








/*

1️⃣ Calling a virtual function from a constructor or destructor
class Base {
public:
    Base() { foo(); }        // virtual call in constructor
    virtual ~Base() { foo(); }
    virtual void foo() { std::cout << "Base\n"; }
};

class Derived : public Base {
public:
    void foo() override { std::cout << "Derived\n"; }
};

int main() {
    Derived d;
}


Problem:

When the Base constructor runs, the object is not yet a Derived, so virtual calls resolve to Base::foo(), not Derived::foo().

Same in destructors: once the destructor of Base runs, the Derived part is gone.

✅ Rule: Avoid calling virtual functions in constructors or destructors.

2️⃣ Deleting a derived object through a base pointer without a virtual destructor
class Base { };
class Derived : public Base {
    int* data = new int[10];
public:
    ~Derived() { delete[] data; }
};

int main() {
    Base* b = new Derived();
    delete b;  // UB! Base::~Base() is called, Derived::~Derived() is skipped
}


Problem:

If the base class doesn’t have a virtual destructor, deleting via a base pointer won’t call the derived destructor, causing resource leaks or UB.

✅ Rule: Always make base classes with virtual functions have a virtual destructor.

3️⃣ Object slicing
class Base {
public:
    int x;
    virtual void foo() { std::cout << x; }
};

class Derived : public Base {
public:
    int y;
    void foo() override { std::cout << x + y; }
};

int main() {
    Derived d;
    Base b = d;  // slicing: only Base part is copied
    b.foo();     // calls Base::foo(), Derived part lost
}


Problem:

Copying a derived object into a base object by value slices off the derived parts.

Virtual function calls on the sliced object do not behave as expected.

✅ Rule: Pass polymorphic objects by pointer or reference, not by value.



*/



// in almost all embedded C++ compilers, the vtable is placed in ROM (specifically the .rodata or .text section) because the table itself is constant and shared by every instance of that class.

// However, each object instance in RAM carries a hidden pointer (the vptr) that points to that table in ROM.

// 1. Where it lives (ROM vs. RAM)
// vtable (The Table): Lives in ROM. It is a static array of addresses created at compile-time.

// vptr (The Pointer): Lives in RAM (inside the object). This is what "connects" your specific object to the logic in ROM.


// Structure,      Location,           Created,        Cost
// vptr,           RAM,                Per Instance,   4 or 8 bytes per object created.
// vtable,         Flash (ROM),        Per Class,      4 bytes per virtual function in the class.
// Function Code,  Flash (ROM),        Per Function,   The actual machine instructions.


// What happens during a function call?
// When you write mySensor->read() the CPU performs these three steps which is why it's slower than a normal call

// Dereference the Object: Load the address of the vtable from the object’s vptr (1 memory access).

// Lookup the Address: Go to the specific index in the vtable (e.g., index 1 for read) to get the function address (1 memory access).

// Jump: Perform a "Branch with Link" (BLX on ARM) to that address.





// sizeof(Led) = 8 bytes (4 for vptr + 1 for pin + 3 for padding)


// Component,      Size (32-bit),                  Notes
// vptr,               4 Bytes,                    Added to the start or end of the object.
// Alignment Padding,  1-3 Bytes,                  Added by the compiler to keep data aligned.
// vtable,             ~4 Bytes(32-bit system) per function,      Stored in Flash (ROM).



// The "Hidden" RAM Trap
// While the vtable in ROM is small, the vptr in RAM is what usually surprises embedded developers. If you have a Point class and you make its methods virtual,
//  every single Point object in your 2KB RAM buffer suddenly gets 4 bytes larger.




// in almost all embedded C++ compilers, the vtable is placed in ROM (specifically the .rodata or .text section) because the table itself is constant and shared by every instance of that class.

// However, each object instance in RAM carries a hidden pointer (the vptr) that points to that table in ROM.

// 1. Where it lives (ROM vs. RAM)
// vtable (The Table): Lives in ROM. It is a static array of addresses created at compile-time.

// vptr (The Pointer): Lives in RAM (inside the object). This is what "connects" your specific object to the logic in ROM.


// Structure,      Location,           Created,        Cost
// vptr,           RAM,                Per Instance,   4 or 8 bytes per object created.
// vtable,         Flash (ROM),        Per Class,      4 bytes per virtual function in the class.
// Function Code,  Flash (ROM),        Per Function,   The actual machine instructions.


// What happens during a function call?
// When you write mySensor->read() the CPU performs these three steps which is why it's slower than a normal call

// Dereference the Object: Load the address of the vtable from the object’s vptr (1 memory access).

// Lookup the Address: Go to the specific index in the vtable (e.g., index 1 for read) to get the function address (1 memory access).

// Jump: Perform a "Branch with Link" (BLX on ARM) to that address.





// Non-virtual version
class Led {
    uint8_t pin; // Size: 1 byte
}; 
// sizeof(Led) = 1 byte

// Virtual version
class Led {
    uint8_t pin;
    virtual void toggle(); // Adds vptr
}; 

// sizeof(Led) = 8 bytes (4 for vptr + 1 for pin + 3 for padding)



// Component,      Size (32-bit),                  Notes
// vptr,               4 Bytes,                    Added to the start or end of the object.
// Alignment Padding,  1-3 Bytes,                  Added by the compiler to keep data aligned.
// vtable,             ~4 Bytes(32-bit system) per function,      Stored in Flash (ROM).



// The "Hidden" RAM Trap
// While the vtable in ROM is small, the vptr in RAM is what usually surprises embedded developers. If you have a Point class and you make its methods virtual,
//  every single Point object in your 2KB RAM buffer suddenly gets 4 bytes larger.




```cpp
#include<vector>
#include<memory>
#include<variant>

// https://www.youtube.com/watch?v=G9MxNwUoSt0&list=PLHTh1InhhwT47Xpx7Cn-bPw9Qygjr98rs




class Shape{
    virtual ~Shape() = default;
};



class Circle : public Shape {

    ~Circle() = default;
};


class Square : public Shape {

    ~Square() = default;
};



int main() {
    // THIS IS A BAD DESIGN - VIRTUAL DISPATCH ON THE POINTER AND INHERITANCE

    // MANY POINTERS - MEMORY IS FRAGMENTED 
    using Shapes = std::vector<std::unique_ptr<Shape>>;

    Shapes shapes;

    // Creating some shapes

    // ALSO MANUAL ALLOCATIONS!!!
    shapes.emplace_back(std::make_unique<Circle>(2.0));
    shapes.emplace_back(std::make_unique<Square>(2.0));
    shapes.emplace_back(std::make_unique<Circle>(2.0));
    shapes.emplace_back(std::make_unique<Square>(2.0));

    return 0;
}



```


