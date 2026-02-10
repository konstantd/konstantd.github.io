+++
date = '2026-02-10T14:31:20+01:00'
draft = false
title = "Move Constructor's promise not to throw improves performance"
tags = ["mid-level", "rule-of-5", "performance"]
+++




Ideally, we should never throw on the move Constructor of an object. If a move constructor throws an exception while a vector is resizing, the vector is in a "broken" state: half of the elements are in the new memory, and half are in the old memory. Move operations often modify the source object (leaving it empty or null), making it dangerous to undo the changes. Therefore, the compiler wants to avoid this risk and uses a copy rather than a movment. We want to avoid this, and the way to do this is to declare the move constructor as noexcept. Like this, we give the promise that we do not throw, and the compiler can safely use it.


Consider the following code.



```cpp
#include <iostream>
#include <utility>
#include <vector>

struct S {
    int x;

    S(int new_x) : x(new_x) {
        std::cout << "Constructor!\n";
    }
    S(const S &s) : x(s.x) {
        std::cout << "Copy Constructor!\n";
    }
    S(S &&s) noexcept : x(s.x) {   // if this is not noexcept then std::vector uses only the copy constructor
        std::cout << "Move Constructor!\n";
    }
    
};

int main() {
    std::vector<S> my_vector;
    my_vector.emplace_back(1);  // capacity now is 1, it should find space for that
    std::cout << "-------- "  << std::endl;
    my_vector.emplace_back(2);  // capacity 2, Constructs in a place that has contiguous mem for 2, moves the other elements
    std::cout << "-------- "  << std::endl;
    my_vector.emplace_back(3);  // capacity 4
    std::cout << "-------- "  << std::endl;
    my_vector.emplace_back(4);  // capacity 4
    std::cout << "-------- "  << std::endl;

    return 0;
}
```


## Under the hood:

When we run this code we get the below output.

``` terminal
    Constructor!
    -------- 
    Constructor!
    Move Constructor!
    -------- 
    Constructor!
    Move Constructor!
    Move Constructor!
    -------- 
    Constructor!
    -------- 
```



Let's check what is going on step by step.

- Okay, the 1st line is easy. We said we want a vector, and then we populate it ones. The capacity of the vector now will become 1. So we construct the vector and add the element 1 there.

- Then we request a 2nd element. The capacity needs to increase from 1 to 2. So the compiler will try to search for a contiguous block of memory for 2 structs S. It finds this spot on memory and it constructs the element "2" directly in place. Then it moves the other element 1.

- Then we request a 3rd element. Capacity is multiplied always times 2. So, from 2 it will become 4. Again, we construct in a new place that has the needed space the element 3, and we move the element 1 and 2.

- Then we want to add the 4th element. Just construct because we have the sufficient capacity.




## Removing the noexcept promise:



Just removing it from the move constructor, we now get:


``` terminal
    Constructor!
    -------- 
    Constructor!
    Copy Constructor!
    -------- 
    Constructor!
    Copy Constructor!
    Copy Constructor!
    -------- 
    Constructor!
    -------- 
``` 






Now it gets expensive, because we are copying every time the whole object. And it's a pity because we already have declared the Move Constructor of the class there, but unfortunately, it is not used.

Imagine this is a really big object that for sure we would not like to copy. Now imagine also, we think we are doing it correctly, but the code behaves otherwise. It is good practice to declare the move constructor as noexcept for the above reason.


## Now what would you do for even more performance?

We can reserve in advance enough memory to avoid all the movements of the previous elements, just by `my_vector.reserve(4);` before we place anything on the vector.


Since our vector has enough capacity for all 4 objects, it will print just the Constructor 4 times.

```
    Constructor!
    -------- 
    Constructor!
    -------- 
    Constructor!
    -------- 
    Constructor!
    -------- 
``` 


Awesome, right?


---

{{< social_icons_extend_with_subscribe >}}
