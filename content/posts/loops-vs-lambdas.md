+++
date = '2026-03-05T17:09:56+01:00'
draft = false
title = '0% Loops vs 100% Lambdas, TMP and Views: Maximal Inlining'
tags = ["advanced-level", "performance", "lambdas", "views", "parallelization"]
+++


On this article, we can see in a mini real-world example how we can get rid of imperative and manual loops that are not at all descriptive, they are difficult to read and maintain, do not help the compiler to inline them and hence, they lack of performance. We will progress with refactoring one step at a time, starting with refactoring using lambdas, then we can advance a bit and expose the lambdas from a custom template function, which handles the internal iteration - a pattern that mimics the logic of modern libraries (and imititates the `views` implementation) - and finally  we will run the code with `C++20` and use modern `views` directly.


Consider the below example. We have a `NetWorkPacket` and then a `NetworkBuffer` that stores a vector of packets. We would like to filter some of the packets based on - for instance -  the encryption or the sourceIP, gather these filtered packets from the buffer and maybe apply some logic on these.



You can find the full code in the [github repo](https://github.com/konstantd/konstantd.github.io).


``` cpp
struct NetworkPacket {

    // Source and Destination
    std::string m_sourceIp;
    std::string m_destinationIp;

    // Let's skip the payload and use size of payload for simplicity on the ctor 
    size_t m_packetSize;

    // Encryption and Priority
    bool m_isEncrypted;
    Priority m_priority;

    NetworkPacket(std::string src, std::string dest, 
                  int size, bool encrypted = false, 
                  Priority priority = Priority::LOW) 
                  : 
                  m_sourceIp(src), m_destinationIp(dest), 
                  m_packetSize(size), m_isEncrypted(encrypted), 
                  m_priority(priority) {}

    // Move ctor default and noexcept
    NetworkPacket(NetworkPacket&& other) noexcept = default;

    // Above line deleted also the copy ctor
    // We need it for the filtered vectors, let's define it
    NetworkPacket(const NetworkPacket& other) = default;
};


struct NetworkBuffer {

    // Container for the Packets
    std::vector<NetworkPacket> m_packetBuffer;

    // Forward a packet to the container
    template <typename T>
    void addPacketForward(T&& packet) {
        m_packetBuffer.emplace_back(std::forward<T>(packet));
    }

};
```

## Populate the Buffer


So given the above Buffer of packets, now I am populating it randomly, allocating for 2^17 packets. The random generators are not of interest here but you can find the full code, just note that I keep the seed fixed so we have the same random packets generated every time we run it.


``` cpp
    // We know the size, let's reserve it to avoid reallocations
    const int N = 1 << 17;
    buffer.m_packetBuffer.reserve(N);

    // Create N random packets in the buffer
    for (int i = 0; i < N; ++i) {
        // Create them as temporaries rvalues
        buffer.addPacketForward(NetworkPacket(getRandomSrc(), 
                                            getRandomDst(), 
                                            getRandomSize(), 
                                            getRandomEncryptionBool(),
                                            getRandomPriority()
                                        )); 
    }
```


# Code with Manual for loops


And now this is our logic. As we said we are filtering some packets from the buffer and gathering the packets in a new vector. I have 3 filters here, we could also operate on the data - but you get the idea. 


``` cpp
// 1. Filter packets by IP "10.0.0.5" source 
std::vector<NetworkPacket> filteredPacketsfromSrc;
for (const auto& packet : buffer.m_packetBuffer) {
    if (packet.m_sourceIp == "10.0.0.5") {
        filteredPacketsfromSrc.push_back(packet);
    }
}

// 2. Filter packets that are encrypted with HIGH priority
std::vector<NetworkPacket> filteredHighPriorEncrypted;
for (const auto& packet : buffer.m_packetBuffer) {
    if ( (packet.m_isEncrypted) && (packet.m_priority == Priority::HIGH) ) {
        filteredHighPriorEncrypted.push_back(packet);
    }
}

// 3. Filter packets by IP "6.8.8.8" destination and size > 128 bytes
std::vector<NetworkPacket> filteredPacketsfromDst_128;
for (const auto& packet : buffer.m_packetBuffer) {
    if ( (packet.m_destinationIp == "6.8.8.8") && (packet.m_packetSize > 128) ) {
        filteredPacketsfromDst_128.push_back(packet);
    }
}
```


The loops are not really showing intention here, imagine we had some hard-coded extra filtering - or some transformations that the logic is hard to be understood. 


## 1st Improvement - `for_each` is slightly better

As a 1st step, we can replace every for loop with a `std::for_each` and a lambda to gain inlining and moving the overhead to the compilation time. 

``` cpp
// 1. Filter packets by IP "10.0.0.5" source 
std::vector<NetworkPacket> filteredPacketsfromSrc;
std::for_each(buffer.m_packetBuffer.begin(), buffer.m_packetBuffer.end(), [&](const auto& packet) {
    if (packet.m_sourceIp == "10.0.0.5") {
        filteredPacketsfromSrc.push_back(packet);
    }
});

// 2. Filter packets that are encrypted with HIGH priority
std::vector<NetworkPacket> filteredHighPriorEncrypted;
std::for_each(buffer.m_packetBuffer.begin(), buffer.m_packetBuffer.end(), [&](const auto& packet) {
    if (packet.m_isEncrypted && packet.m_priority == Priority::HIGH) {
        filteredHighPriorEncrypted.push_back(packet);
    }
});

// 3. Filter packets by IP "6.8.8.8" destination and size > 128 bytes
std::vector<NetworkPacket> filteredPacketsfromDst_128;
std::for_each(buffer.m_packetBuffer.begin(), buffer.m_packetBuffer.end(), [&](const auto& packet) {
    if (packet.m_destinationIp == "6.8.8.8" && packet.m_packetSize > 128) {
        filteredPacketsfromDst_128.push_back(packet);
    }
});
```


### Lambdas & Parallelization 

Lambdas offer parallelization in hand. By switching from a loop to a lambda-based algorithm, you gain the ability to parallelize super easily just with `std::execution::par` .


Though, if the underlying algorithm is not operating on atomics, we should lock manually, in order to avoid pushing back on the same memory and to protect the vector.


Just as an example the 1st filter above, parallelized would be:


``` cpp
#include <execution>
#include <mutex>


// A mutex to lock
std::mutex mtx;

std::vector<NetworkPacket> filteredPacketsfromSrc;
std::for_each(std::execution::paar, buffer.m_packetBuffer.begin(), buffer.m_packetBuffer.end(), [&](const auto& packet) {
    if (packet.m_sourceIp == "10.0.0.5") {
        std::lock_guard<std::mutex> lock(mtx);   // Lock here, unlock is provided by RAII
        filteredPacketsfromSrc.push_back(packet);
    }
});
```


## 2nd Improvement - Avdanced Predicate and Action template class

Then we can identify the pattern and implement a template function that accepts lambdas to filter and act on the buffer. Like this, we decouple the traversal mechanics (the How) from the business logic (the What). This **internal iteration** pattern allows the compiler to inline the lambdas directly into the loop while significantly improving code reuse. This idea is preffered in modern C++ libraries as well, since it is great for encapsulation. Imagine, even if we change the underlying container that we are iterating over, this would still work without changing code in so many places.





We create a template function for the `struct NetworkBuffer` class that accepts a Predicate and an Action.


``` cpp
template <typename Predicate, typename Action>
inline void filter_and_execute(Predicate&& filter, Action&& work) {
    // Because this is a template, 'filter' and 'work' are NOT function pointers.
    // They are unique types, allowing the compiler to 'paste' their logic here.
    for (const NetworkPacket& packet: m_packetBuffer) {
        if (filter(packet)) {
            work(packet);
        }
    }
}
```


And now we use it like:


```cpp
// 1. Filter packets by IP "10.0.0.5" source 
std::vector<NetworkPacket> filteredPacketsfromSrc;
buffer.filter_and_execute(
    [](const NetworkPacket& packet) {
    return packet.m_sourceIp == "10.0.0.5";
    }, 
    [&](const NetworkPacket& packet) {
        filteredPacketsfromSrc.push_back(packet);
    }
);

// 2. Filter packets that are encrypted with HIGH priority
std::vector<NetworkPacket> filteredHighPriorEncrypted;
buffer.filter_and_execute(
    [](const auto& p) { return p.m_isEncrypted && p.m_priority == Priority::HIGH; },
    [&](const auto& p) { filteredHighPriorEncrypted.push_back(p); }
);

// 3. Filter packets by IP "6.8.8.8" destination and size > 128 bytes
std::vector<NetworkPacket> filteredPacketsfromDst_128;
buffer.filter_and_execute(
    [](const auto& p) { return p.m_destinationIp == "6.8.8.8" && p.m_packetSize > 128; },
    [&](const auto& p) { filteredPacketsfromDst_128.push_back(p); }
);
```




## 3rd Improvement - Views are even more readable and give highest performance

Finally we can see how we can achieve the same with `views` from `C++20`. 



```cpp
// 1. Filter packets by IP "10.0.0.5" source 
auto filter1 = buffer
    | std::views::filter([](const auto& p) { return p.m_sourceIp == "10.0.0.5";} );

// 2. Filter packets that are encrypted with HIGH priority
auto filter2 = buffer 
    | std::views::filter([](const auto& p) { return p.m_isEncrypted && p.m_priority == Priority::HIGH;});

// 3. Filter packets by IP "6.8.8.8" destination and size > 128 bytes
auto filter3 = buffer 
    | std::views::filter([](const auto& p) { return p.m_destinationIp == "6.8.8.8"; })
    | std::views::filter([](const auto& p) { return p.m_packetSize > 128; });
```


The advantages now:

- Obviously way more readable

- Previously we were manually doing a `push_back`, which could trigger mem allocations. (In our case we had reserved memory, so we avoided it). Views do not create a new vector - `std::views::filter` is lazy, meaning that it doesn't move or copy anything. This saves us from allocating 3 separate temporary vectors.

- Also, In our `template filter_and_execute`, we have to run a new loop for every filter. With `views`, we can chain them and the compiler can optimize the logic into a single pass over the data, which is much better for the CPU and cache.



## Concluding



To summarize the evolution from manual loops to modern C++ abstractions:


1. for loops with `if-else` logic make the compiler hard to optimize
2. `lamdbas` are more readable and easier for the compiler to inline
3.  `lamdbas` offer parallelization in hand with `std::execution::par` - way easier than a manual loop
4.   A custom template is used often in modern libraries and is great for splitting the traversal (the **How**) from the applied logic (the **What**), completely independent of the underlying container. It handles the **internal iteration** and is powerful for enapsulation. The forwarded lambdas like `Predicate` and `Action` simply define the criteria and the What.
5. `views` are even more readable and better with performance because
6. `views` are lazy - as developers call them, since they are doing **external iteration** - they do not create temporary vectors for copies
7. `views` are perfect for piping many filters at once. In a loop we should iterate again over the packets and apply the new filter, adding significant overhead.
8. `views` give maximum inlining and performance
9. `views` do not offer parallelization as easy as lambdas

