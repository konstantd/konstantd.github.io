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


// 1. Setup the random number engine (seeded with a random device)
std::mt19937 gen(42);


Priority getRandomPriority() {

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
    const int N = 1 << 20;
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


