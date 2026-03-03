+++
date = '2026-02-25T17:09:56+01:00'
draft = false
title = '0% Loops vs 100% Lambdas & Template Metaprogramming: Maximal Inlining'
+++

TODO 
Pro-Tip for your Post: std::execution
If you really want to flex that "100% Lambda" muscle, you should mention std::execution::par. By switching from a loop to a lambda-based algorithm, you gain the ability to parallelize you

I want to replace every for loop with a lambda.


You can replace avery for loop with a lambda to gain maximal inlining and moving the overhead to the compilation time. 


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

    NetworkPacket(std::string src, std::string dest, int size, bool encrypted = false, Priority priority = Priority::LOW)
        : m_sourceIp(src), m_destinationIp(dest), m_packetSize(size), m_isEncrypted(encrypted), m_priority(priority) 
        {}

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


``` cpp
    // We know the size, let's reserve it to avoid reallocations
    const int N = 100000;
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


``` cpp
    // Filter packets by IP "10.0.0.5" source 
    std::vector<NetworkPacket> filteredPacketsfromSrc;
    for (const auto& packet : buffer.m_packetBuffer) {
        if (packet.m_sourceIp == "10.0.0.5") {
            filteredPacketsfromSrc.push_back(packet);
        }
    }

    // Filter packets that are encrypted with HIGH priority
    std::vector<NetworkPacket> filteredHighPriorEncrypted;
    for (const auto& packet : buffer.m_packetBuffer) {
        if ( (packet.m_isEncrypted) && (packet.m_priority == Priority::HIGH) ) {
            filteredHighPriorEncrypted.push_back(packet);
        }
    }

    // Filter packets by IP "6.8.8.8" destination and size of message > 128 bytes
    std::vector<NetworkPacket> filteredPacketsfromDst_128;
    for (const auto& packet : buffer.m_packetBuffer) {
        if ( (packet.m_destinationIp == "6.8.8.8") && (packet.m_packetSize > 128) ) {
            filteredPacketsfromDst_128.push_back(packet);
        }
    }
    

```

 - With rvalue it creates a temporary and moves it to to the buffer
    // The cost is always one move operation even when we pass rvalues

``` cpp

    std::cout << "[FILTER 1] Source IP is '10.0.0.5'" << std::endl;
    std::cout << "Found: " << filteredPacketsfromSrc.size() << " packets." << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::cout << "[FILTER 2] Encrypted AND High Priority" << std::endl;
    std::cout << "Found: " << filteredHighPriorEncrypted.size() << " packets." << std::endl;
    std::cout << "----------------------------------------" << std::endl;


    std::cout << "[FILTER 3] Destination '192.168.1.1' AND Size > 128 bytes" << std::endl;
    std::cout << "Found: " << filteredPacketsfromDst_128.size() << " packets." << std::endl;
    std::cout << "========================================" << std::endl;

```



More people wil argue this is more or less the same and produce same assembly instructions.


``` cpp
std::vector<NetworkPacket> filteredPacketsfromSrc;
std::for_each(buffer.m_packetBuffer.begin(), buffer.m_packetBuffer.end(), [&](const NetworkPacket& packet) 
                                                        {
                                                            if (packet.m_sourceIp == "10.0.0.5") filteredPacketsfromSrc.push_back(packet); 
                                                        }
                                                    );
```



Note that I use a fixed seed to produce the same packages sso testing is fair. See full code in my github profile.