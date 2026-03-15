+++
date = '2026-03-15T12:23:42+02:00'
draft = false   
title = "The 'T&&' Trap: Forwarding References in Templated Classes"
tags = ["advanced-level", "templates", "concepts"]
+++


In this article we will see shortly the trap of forwarding references for a member function of a class that is already templated. Let's start with a practical and simple example of designing our own stack and get to the topic.

Below we design a simple stack using a vector. We just need functions of `empty` ,`top` a `pop` and `push`. 

Where to be careful here for the `API` design:
- `top` should not copy and return, but should return as a reference.
In our case we make it also const reference for `read-only `access.


```cpp
template <typename T>
class MyStack{
    std::vector<T> data;

public:
    const T& top() const {
        if (data.empty()) throw std::out_of_range("Stack<>::top(): is empty");
        return data.back(); 
    }

    bool empty() const {
        return data.empty();
    }

    void pop() {
        if (data.empty()) throw std::out_of_range("Stack<>::pop(): is empty");
        data.pop_back();
    }

    void push(T elem) {
        data.push_back(elem);
    }
};
```

But, now we always copy the element when we need to push it inside our stack. The `push` function should be able to distinguish if the element that is being pushed should be copied or not, in order to save copies. Why to copy a temporary anyways? 

``` cpp
MyStack st;
// We dont want to copy a temporary
st.push("Temporary");
```


## Let's Avoid some Unnecessary Copies - The Manual Way

Now we can distinguish these 2 by overloading the push, 1 for rvalues and one for lvalues. For rvalues we remember that we need to move the element. `std::move` is a `static_cast` to an rvalue reference. When the element is already inside the function, it now has a name and so, it behaves as an lvalue. So we need to move/cast it in that case.

`T&&` only becomes a *forwarding reference* if the compiler has to deduce what T is at the exact moment the function is called. In our `MyStack` example, that deduction has **already happened** at the class level, **when the class already instantiated** and not the function level.

``` cpp
void push(T&& elem) {
    data.emplace_back(std::move(elem));
}
void push(const T& elem) {
    data.emplace_back(elem);
}
```

## Forwarding References Now Come Into Play


Now what about having one function for both cases? We will need universal forwarding. In order to do this we should have another typename that the compiler will deduce when the function is called, and figure out if this is a temporary or an lvalue. 


```cpp
template <typename U>
void push(U&& elem) {
    data.emplace_back(std::forward<U>(elem));
}
```


Perfect! Now, the question is, what if the new typename U **is NOT** the type that the class was initially instantiated and we now mix types for our stack? We can easily protect it with a concept that requires T to be constructible from U. This is `requires std::constructible_from<T, U>` from `C++20`. And this gets to the below **final version** of `MyStack`.



```cpp
template <typename T>
class MyStack{
    std::vector<T> data;

public:
    const T& top() const {
        if (data.empty()) throw std::out_of_range("Stack<>::top(): is empty");
        return data.back(); 
    }

    bool empty() const {
        return data.empty();
    }

    void pop() {
        if (data.empty()) throw std::out_of_range("Stack<>::pop(): is empty");
        data.pop_back();
    }

    template <typename U>
    requires std::constructible_from<T, U>
    void push(U&& elem) {
        data.emplace_back(std::forward<U>(elem));
    }
};
```

Clean and nicely done, right?

---

{{< social_icons_extend_with_subscribe >}}