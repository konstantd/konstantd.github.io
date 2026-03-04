+++
date = '2026-02-25T17:09:56+01:00'
draft = false
title = '0% Loops vs 100% Lambdas, TMP and Views: Maximal Inlining'
tags = ["advanced-level", "performance", "lambdas", "views", "parallelization"]
+++

TODO 
 std::execution
 mention std::execution::par. By switching from a loop to a lambda-based algorithm, you gain the ability to parallelize you

On this article, we can see in a mini real-world example how we can get rid of loops that are not at all descriptive, they are difficult to read and maintain, do not help the compiler to inline them and hence, they lack of performance. We will start with simple refactoring using lambdas, then we can advance a bit and expose the lambdas from a template function - imititating the `views` implementation - and finally  we will run the code with `C++20` and use modern `views` directly.


Consider the below example. We have a `NetWorkPacket` and then a `NetworkBuffer` that stores a vector of packets. We would like to filter some of the packets based on - for instance -  the encryption or the sourceIP, gather these filtered packets from the buffer and maybe apply some logic on these. This screams `C++20` and `views` but we will extend the blog a step at a time reaching to the modernest and most performant and readable way.





You can find the full code here: 



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

## Populate 


So given the above Buffer of packets, now I am populating it randomly, allocating for 2^17 packets. The random generators are not of interest here but you can find the full code in my github, just note I keep the seed fixed so we have every time we run it the same random packets generated.



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


And now this is our logic. As we said we are filtering some packets from the buffer and gathering the packets in a new vector. I have 3 filters here, but you get the idea. 

``` cpp
    // Filter 1 - packets by IP "10.0.0.5" source 
    std::vector<NetworkPacket> filteredPacketsfromSrc;
    for (const auto& packet : buffer.m_packetBuffer) {
        if (packet.m_sourceIp == "10.0.0.5") {
            filteredPacketsfromSrc.push_back(packet);
        }
    }

    // Filter 2 - packets that are encrypted with HIGH priority
    std::vector<NetworkPacket> filteredHighPriorEncrypted;
    for (const auto& packet : buffer.m_packetBuffer) {
        if ( (packet.m_isEncrypted) && (packet.m_priority == Priority::HIGH) ) {
            filteredHighPriorEncrypted.push_back(packet);
        }
    }

    // Filter 3 - packets by IP "6.8.8.8" destination and size of message > 128 bytes
    std::vector<NetworkPacket> filteredPacketsfromDst_128;
    for (const auto& packet : buffer.m_packetBuffer) {
        if ( (packet.m_destinationIp == "6.8.8.8") && (packet.m_packetSize > 128) ) {
            filteredPacketsfromDst_128.push_back(packet);
        }
    }
```


The loops are not really showing intention here, imagine we had some hard-coded extra filtering - the logic is hard to be understood. As a 1st step, we can replace every for loop with a lambda to gain inlining and moving the overhead to the compilation time. 



 - With rvalue it creates a temporary and moves it to to the buffer
    // The cost is always one move operation even when we pass rvalues



More people wil argue this is more or less the same and produce same assembly instructions.


## For_each is slightly better


``` cpp
std::vector<NetworkPacket> filteredPacketsfromSrc;
std::for_each(buffer.m_packetBuffer.begin(), buffer.m_packetBuffer.end(), [&](const NetworkPacket& packet) 
                {
                    if (packet.m_sourceIp == "10.0.0.5") filteredPacketsfromSrc.push_back(packet); 
                }
            );
```



Note that I use a fixed seed to produce the same packages sso testing is fair. See full code in my github profile. 

Lambdas offer parallelization in hand. We just do:

`std::execution::par`


## Avdanced Predicate and Action template class

Then we can identify a pattern and implement a template function that accepts lambdas to filter and act. Finally we can see how we can achieve the same with `views` from `C++20`. 

We can create a template function for the `struct NetworkBuffer` class that accepts a Predicate and an Action.


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


And now 


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




## Views are even more readable and give highest performance

And we can do the same with views:


```cpp
auto filter1 = buffer
    | std::views::filter([](const auto& p) { return p.m_sourceIp == "10.0.0.5";} );

auto filter2 = buffer 
    | std::views::filter([](const auto& p) { return p.m_isEncrypted && p.m_priority == Priority::HIGH;});

auto filter3 = buffer 
    | std::views::filter([](const auto& p) { return p.m_destinationIp == "6.8.8.8"; })
    | std::views::filter([](const auto& p) { return p.m_packetSize > 128; });
```


The advantages now:

- Obviously way more readable

- Previously we were manually doing a `push_back`, which could trigger mem allocations. (In our case we had reserved memory, so we avoided it). Views do not create a new vector - `std::views::filter` is lazy, meaning that it doesn't move anything until you actually iterate over it. This saves us from allocating 3 separate temporary vectors.

- Also, In our `template filter_and_execute`, we have to run a new loop for every filter. With `views`, we can chain them and the compiler can optimize the logic into a single pass over the data, which is much better for the CPU cache.



## Concluding


- Lambdas are more readable and easier for the compiler to inline
- If you act on a container often and manipulate data, like `std::for_each` and `std::transform` consider using a template function and forward lambdas like our `Predicate` and `Action` 
- lamdbas offer parallelization in hand with `std::execution::par` - way easier than your manual loop
- views are even more readable 
- views are lazy - they do not create temporary vectors for copies
- views give maximum inlining and performance
- views do not offer parallelization, at least yet. 