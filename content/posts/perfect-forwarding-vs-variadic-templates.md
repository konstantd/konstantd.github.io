+++
date = '2026-03-25T13:03:31+01:00'
draft = false
title = 'Perfect Forwarding vs Variadic Templates'
+++



In a small code snippet we will extend the post from the NetworkBuffer that we used in our previous article [](). We will demonstrate the difference between variadic templates and perfect forwarding.

``` cpp

struct NetworkPacket {

    // Source and Destination
    std::string m_sourceIp;
    std::string m_destinationIp;

    // Let's skip the others member vars
    // ...


public:
    NetworkPacket(std::string src, std::string dest): m_sourceIp(src), m_destinationIp(dest) {}

    // Move ctor default - this deletes also the copy ctor
    NetworkPacket(NetworkPacket&& other) noexcept {
        std::cout << "The packet is moved" << "\n";
        //.. continue here
    }


};

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