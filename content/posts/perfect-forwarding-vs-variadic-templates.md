+++
date = '2026-03-25T13:03:31+01:00'
draft = false
title = 'No Moves, In-Place Construction: Perfect Forwarding vs. Variadic Templates'
tags = ["advanced-level", "templates", "performance", "Move-Semantics", "variadic-templates"]

+++



In a small code snippet we will extend the post from the `NetworkBuffer` that we used in our previous article with [lambdas and views](https://konstantd.github.io/posts/loops-vs-lambdas/).

We will demonstrate the difference between **variadic templates and perfect forwarding**  vs **perfect forwarding**. 


You can find the cpp file in the [src-code](https://github.com/konstantd/konstantd.github.io/tree/main/src-code) of the blog repo.

## The Packet & Buffer - Code

In the `NetworkPacket` we have kept only two member variables for simplicity. Then we make the move ctor and move assignment operator custom so we can see when they are called.


``` cpp

struct NetworkPacket {

    // Source and Destination
    std::string m_sourceIp;
    std::string m_destinationIp;

    // Let's skip the others member vars
    // ...


public:
    // Ctor
    NetworkPacket(std::string src, std::string dest): m_sourceIp(src), m_destinationIp(dest) {}

    // Move ctor default - this deletes also the copy ctor
    NetworkPacket(NetworkPacket&& other) noexcept {
        std::cout << "The packet is moved" << "\n";
    }


};
```

In the `NetworkBuffer` we have a vector of `NetworkPacket`, we add packets to the buffer in two ways.

So we have `NetworkPacket` and then `NetworkBuffer` that has a vector of `NetworkPacket`

We can add a packet to the buffer in two ways:
1. `Perfect forwarding` - With rvalue it creates a temporary and moves it to to the buffer
2. `Variadic templates` & `perfect forwarding` - No moves and no copies here, this constructs the NetworkPacket directly in-place of the vector's memory


When we forward we pass an object as an argument to a fuction and we want to preserve its value category (lvalue or rvalue) to avoid unnecessary copies. 

With variadic templates we do not pass an object at all. We pass the arguements needed to construct this object in the vector's memory. So, we construct it directly in-place.


``` cpp
struct NetworkBuffer {

    std::vector<NetworkPacket> m_packetBuffer;

    // Forward reference - With rvalue it creates a temporary and moves it to to the buffer
    // You pay for one Move operation.
    template <typename T>
    void addPacketForward(T&& packet) {
        m_packetBuffer.emplace_back(std::forward<T>(packet));
    }


    template <typename... Args>
    void addPacketArgs(Args&&... args) {
        // Zero Moves. Zero Copies.
        // This constructs the NetworkPacket directly in the vector's memory
        // It calls: NetworkPacket(args...)
        m_packetBuffer.emplace_back(std::forward<Args>(args)...);
    }
    
};
```



## Execution


 Let's execute adding 2 packets in the buffer to confirm the execution.


``` cpp
int main() {

    NetworkBuffer buffer;
    buffer.m_packetBuffer.reserve(10); // To avoid reallocation and extra moves


    // Add a packet with the 1st way - Passing the Networkpacket as rvalue - we pay for one move
    buffer.addPacketForward(NetworkPacket("198.0.129.1", "DESTINATION_IP"));

    // Add a packet with the 2nd way - Passing as rvalues the Args - No moves and no copies
    buffer.addPacketArgs("198.0.123.2", "DESTINATION_IP");

    return 0;
}
```


The output of the above code will be:

```
The packet is moved
```

This is because of the `addPacketForward` function, as described above.

## Conclusion


1. The **cost** of forwarding a packet is **one move operation** even when we pass **rvalues**.

2. The **cost** of adding a packet with **variadic templates and perfect forwarding** is **zero moves and zero copies**. This is because we construct the NetworkPacket directly **in-place** of the vector's memory, without creating any temporary objects that need to be moved or copied.


---

{{< social_icons_extend_with_subscribe >}}