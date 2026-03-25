#include<string>
#include<vector>
#include<iostream>



struct NetworkPacket {
    // Source and Destination
    std::string m_sourceIp;
    std::string m_destinationIp;
    // Let's skip the others member vars
    // ...
public:

    // Ctor
    NetworkPacket(std::string src, std::string dest): m_sourceIp(src), m_destinationIp(dest) {}

    // Move ctor - Make it custom so we can see when it is called 
    // This deletes also the copy ctor because of Rule of 5
    NetworkPacket(NetworkPacket&& other) noexcept : m_sourceIp(std::move(other.m_sourceIp)), m_destinationIp(std::move(other.m_destinationIp)) {
        std::cout << "The packet is moved" << "\n";
    }

    // Move Assignment Operator
    NetworkPacket& operator=(NetworkPacket&& other) noexcept {
        // Check the pointers for self-assignment
        // other is rvalue reference type but inside the function other is an lvalue - holds the type 
        if (this == &other) return *this; 

        this->m_sourceIp = std::move(other->m_sourceIp);
        this->m_destinationIp = std::move(other->destination);
        return *this;
    }
};


/*
    In the NetworkBuffer we have a vector of `NetworkPacket`, we add packets to the buffer in two ways.
*/


struct NetworkBuffer {

    std::vector<NetworkPacket> m_packetBuffer;

    // 1st way of adding a packet 
    // Forward reference - With rvalue it creates a temporary and moves it to to the buffer
    // We pay for one Move operation.
    template <typename T>
    void addPacketForward(T&& packet) {
        m_packetBuffer.emplace_back(std::forward<T>(packet));
    }


    template <typename... Args>
    void addPacketArgs(Args&&... args) {
        // 2nd way of adding a packet - Variadic templates & perfect forwarding
        // No moves and no copies here
        // This constructs the NetworkPacket directly in-place of the vector's mem
        m_packetBuffer.emplace_back(std::forward<Args>(args)...);
    }
    
};


int main() {

    NetworkBuffer buffer;
    buffer.m_packetBuffer.reserve(10); // To avoid reallocation and extra moves


    // Add a packet with the 1st way - Passing the Networkpacket as rvalue - we pay for one move
    buffer.addPacketForward(NetworkPacket("198.0.129.1", "DESTINATION_IP"));

    // Add a packet with the 2nd way - Passing as rvalues the Args - No moves and no copies
    buffer.addPacketArgs("198.0.123.2", "DESTINATION_IP");

    return 0;
}

