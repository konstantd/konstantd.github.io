+++
date = '2026-06-16T14:37:09+03:00'
draft = false
title = 'Rule of 5: It Is Not Just About the Destructor'
tags = ["intermediate-level", "rule-of-5", "move-semantics", "polymorhism", "performance"]
+++


For resource managing in classes, **do not think that move will always move**. It will falllback to copy if the object is not moveable. Or if the operation throws an exception it will fall back to copy as well, so **always mark** the move constructor as `noexcept` as we saw on the [previous blog about move semantics](https://konstantd.github.io/posts/move-ctor-no-except/).


``` cpp
struct MyClass{
    std::vector<int> m_data;
    ~MyClass() = default;
};
```


Given the above class, consider the below lines:

``` cpp
MyClass a;
MyClass b = std::move(a);
```


The above code looks like class `a` will be moved to class `b` and class `a` will be left in a valid but unspecified state, right? This is exactly what `std::move` is supposed to do, **but NO**. Class `a` **will be deep-copied** here. Why?



# Rule of 5 

When we operate on classes `Rule of 5` means that if you implement one special member function either default or then you need to implement all of them.

1. destructor
2. copy constructor
3. copy assignment (deep copy)
4. move constructor
5. move assignment operator

The compiler is trying to protect the resources of the class, especially the move operations that are more prone to error/data loss. It sees you have implemented one of the special member functions and says ok, then something more complicated is needed for the move operations (move ctor and move assignment operator), so it deactivates the `deafult` operations, since it assumes it does not know how to handle it, in order to protect the data of the class and uses the corresponding copy special member function just to ensure no data are lost.


# Code Review 

Now that we have spotted this, let's delete the last line.


``` cpp
struct MyClass{
    std::vector<int> m_data;
    // No destructor, no copy, no move defined.
    // The compiler automatically generates perfectly move and copy semantics.
};
```

Oddly enough it is **EXACTLY** the same as writing:


``` cpp
struct MyClass {
    std::vector<int> m_data;
    MyClass() = default;
    ~MyClass() = default;
    MyClass(const MyClass& other) = default;
    MyClass& operator=(const MyClass& other) = default;
    MyClass(MyClass&& other) noexcept = default;
    MyClass& operator=(MyClass&& other) noexcept = default;
};
```

And that saves some typing, right?

# Virtual Destructor and Resource Management    


Given what we said above, if we have a virtual destructor it breaks the move semantics of the class. When we read about `Rule of 5`, most people just mention the `virtual destructor` and omit that the same happens with all special member functions that we mentioned above, not just the destructor. It is important to remember the rule for all definitions, but why more focus is given on destructors? 


The answer is that C++11 had the `Rule of 3`, which is the rule of 5 without the 2 extra move operations. Because managing a resource always required a custom destructor, the destructor became the universal signal that a class was doing something complex with memory.

If you need a virtual destructor (which you almost always do for base classes), you must explicitly tell the compiler to bring the move functions back. Polymorhism is used a lot, so many people might lose performance just by omitting this. That is the reason why more attention is given to the `virtual dtor` with the `Rule of 5`.

For Base classes it is good habit to always define all of them (even marking them as default) like below:


```cpp
class MyBase {
public:
    // 1. Virtual Destructor (This 'breaks' automatic move generation)
    virtual ~MyBase() = default;

    // 2 & 3. You must explicitly bring back Move semantics!
    MyBase(MyBase&&) noexcept = default;            // Move Constructor
    MyBase& operator=(MyBase&&) noexcept = default; // Move Assignment

    // 4 & 5. Usually you'd bring back Copy too
    MyBase(const MyBase&) = default;
    MyBase& operator=(const MyBase&) = default;
};
```


# Conclusion

Because a custom destructor silently kills your move semantics while leaving your copy semantics alive, it creates a performance trap. Your code still compiles perfectly, but your program secretly slows down because it is copying data when it should be moving it, and sometimes programmers falsely assume data are moved. 

So either avoid completely declaring special member functions for clarity (especially if default) OR declare all of them manually (even as default).


---

{{< social_icons_extend_with_subscribe >}}