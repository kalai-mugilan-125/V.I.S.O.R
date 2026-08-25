#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

// Define glm vectors for simple structures if GLM is not included here
// Using basic structs to decouple, or we can just include glm
#include <glm/glm.hpp>

struct HandData {
    std::string gesture;
    glm::vec3 position;      // Derived from landmarks[0] (wrist)
    float pinchDistance;
    glm::vec3 palmRotation;
};

struct DataPacket {
    bool active;
    std::vector<HandData> hands;
    
    // For Python json parsing
    static DataPacket deserialize(const std::string& data);
};

class UDPReceiver {
public:
    UDPReceiver(int port = 5005);
    ~UDPReceiver();

    bool initialize();
    DataPacket pollData();

private:
    int listenPort;
#ifdef _WIN32
    SOCKET recvSocket;
#else
    int recvSocket;
#endif
    DataPacket m_lastPacket;
};
