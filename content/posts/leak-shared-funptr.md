+++
date = '2026-11-08T18:04:57+02:00'
draft = false
title = 'Spot the Leak of Shared Pointer in Lambdas'
summary = 'Be careful of circular ownerships especially in lambdas that accept shared pointers.'
tags = ["mid-level", "lambdas", "memory"]
+++


Let's figure out why we need to be careful with lambdas when we want to capture pointers.


## Will this compile? If yes, does this leak?

``` cpp
#include <iostream>
#include <functional>
#include <memory>

class Node {
public:
    std::function<void()> callback;

    Node() { std::cout << "Node created\n"; }
    ~Node() { std::cout << "Node destroyed\n"; }
};

void createNode() {
    auto node = std::make_unique<Node>();

    node->callback = [node]() {
        std::cout << "Callback executed!\n";
    };
}


int main() {
    createNode();
    return 0;
}
```


Give it a bit of time, before you scroll down to have a look at it.


---

So, here, we create a unique_ptr. Then we pass it in the capture as copy. But... we know that a unique_ptr has its `copy ctor` and `copy assignment` operator **deleted**. This **will not compile**!


## Will this compile? If yes, does this leak?


Now consider this one.

``` cpp
#include <iostream>
#include <functional>
#include <memory>

class Node {
public:
    std::function<void()> callback;

    Node() { std::cout << "Node created\n"; }
    ~Node() { std::cout << "Node destroyed\n"; }
};

void createNode() {
    auto node = std::make_shared<Node>();

    node->callback = [node]() {
        std::cout << "Callback executed!\n";
    };

}

int main() {
    createNode();
    return 0;
}
```

All we did was change the unique to a shared pointer. Now of course we can copy this pointer inside the lambda. This will compile successfully.


``` bash
g++ leak_std_function.cpp -Wall -Werror -o exe
./exe
```

And what we get is:

``` 
Node created
```

# Where is the leak?



So it's not deleted? Then where is the leak? 


Note that, the shared pointer has a ref counter under the hood - which is also thread-safe by the way. So, the ref counter increments 1 when we allocate the object. On the lambda it also increments because we pass by copy the ptr. So ref counter is equal to 2. 

When `createNode()` finishes executing, the local variable node goes out of scope and is destroyed. This decrements the reference count by 1.  Since the ref counter is 1 (not 0), the Node destructor is not called when it goes out of scope.



# So who owns what?

- The `Node` owns the lambda.
- The lambda owns a pointer pointing to that Node.


Now, this should also explain the closed loop of the problem and the solution. 


# Solution - weak_ptr


Making this a weak pointer breaks the trap.


A `std::weak_ptr` is an observer, not an owner, it can check if the object is still alive (by checking the ref counter itself) but it will not increment the ref counter at all. 

That means that we need the object initially to be existent in the heap. So we use a shared_ptr that will manage the allocation and free of the memory for that object in the heap. Then we break the circular ownership with a weak pointer.

Consider this:


``` cpp
#include <iostream>
#include <functional>
#include <memory>

class Node {
public:
    std::function<void()> callback;

    Node() { std::cout << "Node created\n"; }
    ~Node() { std::cout << "Node destroyed\n"; }
};

void createNode() {

    auto node = std::make_shared<Node>();

    std::weak_ptr<Node> weakNode = node;

    node->callback = [weakNode]() {
        std::cout << "Callback executed!\n";
    };
    
}

int main() {
    createNode();
    return 0;
}
```

And now we get 

``` bash
./exe
Node created
Node destroyed
```


# Summary


Be careful of circular ownerships especially in lambdas that accept shared pointers!

In that case, consider a `std::weak_ptr`.



---

{{< social_icons_extend_with_subscribe >}}