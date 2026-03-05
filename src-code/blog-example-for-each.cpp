#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <iostream>
#include <random>

enum class Priority {
    LOW = 0,
    MEDIUM = 5,
    HIGH = 10
};

Priority getRandomPriority() {
    // 1. Setup the random number engine (seeded with a random device)
    std::random_device rd;
    std::mt19937 gen(rd());

    // 2. Define the possible "indices" (0, 1, or 2)
    std::uniform_int_distribution<> dis(0, 2);
    int choice = dis(gen);

    // 3. Map the random index to your actual enum values
    switch (choice) {
        case 0:  return Priority::LOW;
        case 1:  return Priority::MEDIUM;
        case 2:  default: return Priority::HIGH;
    }
}



bool getRandomEncryptionBool() {
    // Set up the engine (seed it once)
    std::random_device rd;
    std::mt19937 gen(rd());

    // Set up the distribution (default is 0.5)
    std::bernoulli_distribution d(0.5);

    return d(gen);
}



struct NetworkPacket {

    // Source and Destination
    std::string m_sourceIp;
    std::string m_destinationIp;

    // Let's skip the payload and use size of payload for simplicity on the ctor 
    size_t m_packetSize;

    // Encryption and Priority
    bool m_isEncrypted;
    Priority m_priority;


public:
    NetworkPacket(std::string src, std::string dest, int size, bool encrypted = false, Priority priority = Priority::LOW)
        : m_sourceIp(src), m_destinationIp(dest), m_packetSize(size), m_isEncrypted(encrypted), m_priority(priority) 
        {}

    // Move ctor default and noexcept
    NetworkPacket(NetworkPacket&& other) noexcept = default;

    // Above line deleted also the copy ctor, let's define it
    NetworkPacket(const NetworkPacket& other) = default;

};


struct NetworkBuffer {

    std::vector<NetworkPacket> m_packetBuffer;

    // Forward a packet - With rvalue it creates a temporary and moves it to to the buffer
    // The cost is always one move operation even when we pass rvalues
    template <typename T>
    void addPacketForward(T&& packet) {
        m_packetBuffer.emplace_back(std::forward<T>(packet));
    }

};


int main() {

    NetworkBuffer buffer;

    std::vector<std::string> randomIPsrc = {"192.168.1.1", "10.0.0.5", "172.16.0.100", "8.8.8.8"};
    std::vector<std::string> randomIPdst = {"152.128.1.1", "10.1.0.5", "152.145.0.100", "6.8.8.8"};

    // Lambdas to call later
    auto getRandomSrc = [&]() {
        return randomIPsrc[rand() % randomIPsrc.size()];
    };
    auto getRandomDst = [&]() {
        return randomIPdst[rand() % randomIPdst.size()];
    };
    auto getRandomSize = [&]() {
        return (rand() % 1024) + 64;
    };


    // We know the size, let's reserve it
    const int N = 1000000;
    buffer.m_packetBuffer.reserve(N);

    // Create N packets in the buffer
    for (int i = 0; i < N; ++i) {
        // Create them as temporaries rvalues
        buffer.addPacketForward(NetworkPacket(getRandomSrc(), 
                                            getRandomDst(), 
                                            getRandomSize(), 
                                            getRandomEncryptionBool(),
                                            getRandomPriority()
                                        )); 
    }


    
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
                    
    

    std::cout << "[FILTER 1] Source IP is '10.0.0.5'" << std::endl;
    std::cout << "Found: " << filteredPacketsfromSrc.size() << " packets." << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::cout << "[FILTER 2] Encrypted AND High Priority" << std::endl;
    std::cout << "Found: " << filteredHighPriorEncrypted.size() << " packets." << std::endl;
    std::cout << "----------------------------------------" << std::endl;


    std::cout << "[FILTER 3] Destination '192.168.1.1' AND Size > 128 bytes" << std::endl;
    std::cout << "Found: " << filteredPacketsfromDst_128.size() << " packets." << std::endl;
    std::cout << "========================================" << std::endl;


    return 0;
}

