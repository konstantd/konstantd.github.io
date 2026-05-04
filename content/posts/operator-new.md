+++
date = '2026-05-04T17:07:03+02:00'
draft = false
title = "An Intro to Pooling & Custom Allocators: `operator new`"
tags = ["beginner", "memory"]
+++



In this beginner friendly article we dive into the memory internals. For the matter of this blog I will use the `new` and `delete` keywords below to showcase what is going on under the hood. Though, always rememeber to use `RAII` principles (unique ptr in that case). The manual management of the underlying memory is not the safe way to do it but could be used in more advanced topics.


Let's say we have a minor struct.

## Code

```cpp
struct Bar {
    Bar() { std::cout << "Bar()" << std::endl; }
    ~Bar() { std::cout << "~Bar()" << std::endl; }
};
```

## Two ways to handle the memory under the hood

The typical way to allocate + construct an object of `Bar` in place is with:

```cpp
Bar* ptr = new Bar; // new: allocates memory + constructs a Bar object in place
delete ptr;         // delete: calls the destructor + deallocates memory
```


Now the manual way:


```cpp
    void* raw = operator new(sizeof(Bar));  // Just allocates memory of the size of our Bar
    Bar* ptr = new(raw) Bar;                // Construct IN-PLACE: constructs a Bar object in the pre-allocated memory

    ptr->~Bar();            // Manually call the destructor
    operator delete(raw);   // Manually deallocate the raw memory
```



## Why this is useful?



With `new operator` you need to manually deallocate the memory using `operator delete`, and you also need to manually call the destructor of the object if you used placement new. This can lead to memory leaks or undefined behavior if not done correctly. So what is the point of it? 

The advantage is that you can have **more control** over what happens under the hood when you manually handle the memory. This could be used in custom allocators, or memory pooling, where you need to delete the object but not to deallocate the memory, so it can be used for future use. This can be **more efficient** than allocating and deallocating memory repeatedly, since it **avoids the system calls overhead**, **especially if the objects are frequently created and destroyed**.


## Summary


You can think of it like this:

* `operator new` is the `C-style malloc` equivalent. It allocates the requested memory, does not call ctor
* `operator delete` mimics the `C-style free`; It does not call the destructor of the object, it only deallocates the memory

Note that there are some more C++ internals underneath though, the C++ equivalent can throw exceptions, allocate for SIMD vectorization and **most importantly can be overloaded** for a pool allocator use for instance


* `new` allocates and constructs
* `delete` calls destructor and deallocates



In a future more advanced article we can extend the **in-place construction** for a custom pool allocator.