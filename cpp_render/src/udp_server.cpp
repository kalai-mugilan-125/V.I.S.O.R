#include "udp_server.h"
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#endif

DataPacket DataPacket::deserialize(const std::string& data) {
    DataPacket packet;
    packet.active = false;
    
    try {
        json j = json::parse(data);
        if (j.contains("active")) {
            packet.active = j["active"].get<bool>();
        }
        
        if (packet.active && j.contains("hands") && j["hands"].is_array()) {
            for (const auto& handJson : j["hands"]) {
                HandData hand;
                
                if (handJson.contains("gesture")) {
                    hand.gesture = handJson["gesture"].get<std::string>();
                } else {
                    hand.gesture = "NONE";
                }
                
                if (handJson.contains("pinch_distance")) {
                    hand.pinchDistance = handJson["pinch_distance"].get<float>();
                } else {
                    hand.pinchDistance = 0.0f;
                }
                
                if (handJson.contains("palm_rotation") && handJson["palm_rotation"].is_array() && handJson["palm_rotation"].size() >= 3) {
                    hand.palmRotation.x = handJson["palm_rotation"][0].get<float>();
                    hand.palmRotation.y = handJson["palm_rotation"][1].get<float>();
                    hand.palmRotation.z = handJson["palm_rotation"][2].get<float>();
                } else {
                    hand.palmRotation = glm::vec3(0.0f);
                }
                
                // Compute hand position (centroid of landmarks for smoother tracking)
                if (handJson.contains("landmarks") && handJson["landmarks"].is_array() && !handJson["landmarks"].empty()) {
                    glm::vec3 sum(0.0f);
                    int count = 0;
                    for (const auto& lm : handJson["landmarks"]) {
                        if (lm.is_array() && lm.size() >= 3) {
                            sum += glm::vec3(lm[0].get<float>(), lm[1].get<float>(), lm[2].get<float>());
                            count++;
                        } else if (lm.is_array() && lm.size() >= 2) {
                            sum += glm::vec3(lm[0].get<float>(), lm[1].get<float>(), 0.0f);
                            count++;
                        }
                    }
                    hand.position = (count > 0) ? (sum / static_cast<float>(count)) : glm::vec3(0.0f);
                } else {
                    hand.position = glm::vec3(0.0f);
                }
                
                packet.hands.push_back(hand);
            }
        } else if (packet.active && j.contains("landmarks") && j["landmarks"].is_array()) {
            // Fallback for single-hand root format
            HandData hand;
            hand.gesture = j.value("gesture", "NONE");
            hand.pinchDistance = j.value("pinch_distance", 0.0f);
            
            glm::vec3 sum(0.0f);
            int count = 0;
            for (const auto& lm : j["landmarks"]) {
                if (lm.is_array() && lm.size() >= 3) {
                    sum += glm::vec3(lm[0].get<float>(), lm[1].get<float>(), lm[2].get<float>());
                    count++;
                }
            }
            hand.position = (count > 0) ? (sum / static_cast<float>(count)) : glm::vec3(0.0f);
            packet.hands.push_back(hand);
        }
    } catch (const json::parse_error& e) {
        // Ignored to prevent spamming console if random non-json UDP packets hit the port
    } catch (const json::type_error& e) {
        // Ignored
    }
    
    return packet;
}

UDPReceiver::UDPReceiver(int port) : listenPort(port) {
#ifdef _WIN32
    recvSocket = INVALID_SOCKET;
#else
    recvSocket = -1;
#endif
    m_lastPacket.active = false;
}

UDPReceiver::~UDPReceiver() {
#ifdef _WIN32
    if (recvSocket != INVALID_SOCKET) {
        closesocket(recvSocket);
    }
    WSACleanup();
#else
    if (recvSocket != -1) {
        close(recvSocket);
    }
#endif
}

bool UDPReceiver::initialize() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return false;
    }
    recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recvSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return false;
    }
#else
    recvSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (recvSocket < 0) {
        std::cerr << "Socket creation failed." << std::endl;
        return false;
    }
#endif

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(listenPort);

    if (bind(recvSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed on port " << listenPort << std::endl;
        return false;
    }

    // Set non-blocking mode
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(recvSocket, FIONBIO, &mode);
#else
    int flags = fcntl(recvSocket, F_GETFL, 0);
    fcntl(recvSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    std::cout << "UDPReceiver initialized on port " << listenPort << std::endl;
    return true;
}

DataPacket UDPReceiver::pollData() {
    char buffer[4096];
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    
    bool hasNewData = false;
    std::string latestData;

    // Drain the socket buffer completely to get the most recent packet
    while (true) {
#ifdef _WIN32
        int bytesRead = recvfrom(recvSocket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&clientAddr, &clientAddrLen);
#else
        int bytesRead = recvfrom(recvSocket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&clientAddr, (socklen_t*)&clientAddrLen);
#endif
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            latestData = buffer;
            hasNewData = true;
        } else {
            break; // No more data in buffer
        }
    }

    if (hasNewData) {
        m_lastPacket = DataPacket::deserialize(latestData);
    }

    // Return the last known state (so we don't drop hands if render loop is faster than tracker)
    return m_lastPacket;
}
