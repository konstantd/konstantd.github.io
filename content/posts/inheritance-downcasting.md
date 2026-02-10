+++
date = '2025-02-10T10:38:41+01:00'
draft = false
title = 'Inheritance downcasting is dangerous'
summary = 'Converting a base-class pointer to a derived-class pointer is downcasting. We should be careful when we do that in order to avoid UBs.'
tags = ["mid", "inheritance"]
disableShare = false
+++


## Inheritance downcasting is dangerous!


Converting a base-class pointer to a derived-class pointer is downcasting. We should be careful when we do that in order to avoid UBs.

Consider the following code.

```cpp
#include<iostream>

class Base
{
public:

    Base() {
        std::cout << "Base constructor" << std::endl;
    }
    void foo()
    {
        std::cout << "Base::foo" << std::endl;
    }

    virtual ~Base()
    {
        std::cout << "Base destructor" << std::endl;
    }
};


class Derived : public Base
{
public:
    void foo()
    {
        std::cout << "Derived::foo" << std::endl;
    }

    Derived() {
        std::cout << "Derived constructor" << std::endl;
    }


    ~Derived()
    {
        std::cout << "Derived destructor" << std::endl;
    }
};


int main()
{


    Base* base_ptr = new Base();   // OBJECT IS BASE - NOT DERIVED
    Derived* derived_ptr = static_cast<Derived*>(base_ptr); // DANGER!
    // The cast would succeed because static_cast performs no runtime check.
    //  However, derived_ptr would point to an object 
    // that is only a Base but is being treated as a Derived. 



    // Now accessing Derived-specific members (like foo() if it touches Derived data) 
    // leads to undefined behavior and likely memory corruption or a crash.
    derived_ptr->foo();


    // ---------------------------------------------------

    // Its is better to use dynamic_cast and perform a runtime check Like below:
    Derived* derived_ptr = dynamic_cast<Derived*>(base_ptr);
    
    if (derived_ptr) {
        // This code block is NOT executed because the cast failed
        derived_ptr->foo(); 
    } else {
        std::cout << "ERROR: Cast failed! Object is not a Derived. " << "\n">>;
    }


    return 0;
}
```

## The TakeAway
*Never downcast a base pointer to a derived type unless you are 100% certain of the object it points to.*

### Static vs. Dynamic Casting
* **static_cast**: Will compile as the name suggests—**statically**. It does not perform runtime checks, making it faster - but dangerous if you are wrong about the object type.
* **dynamic_cast**: The preferred method for downcasting. It performs a **runtime check**. If the cast is invalid, it returns a `nullptr` (for pointers), allowing you to handle the error gracefully.

---

### Directional Safety

#### ⬆️ Upcasting (`Derived` --> `Base`)
* **Safe** and Implicit.
* Moving up the hierarchy is always allowed because a derived object *is* an instance of its base class.

#### ⬇️ Downcasting (`Base` --> `Derived`)
* Always **Dangerous**.
* Requires explicit casting. Always prefer `dynamic_cast` and perform a runtime check for the returned pointer to avoid crashes or undefined behavior.

---