#include <iostream>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <windows.networking.sockets.h>
#include <thread>
#pragma comment(lib, "Ws2_32.lib")

struct Flight {
    std::string id;
    std::vector<double> fuelAmounts;
    std::vector<std::chrono::system_clock::time_point> timestamps;

    Flight() = default; // Default constructor
    Flight(const std::string& id) : id(id) {}

    void addData(double fuel, const std::chrono::system_clock::time_point& timestamp) {
        fuelAmounts.push_back(fuel);
        timestamps.push_back(timestamp);
    }

    double calculateAverageConsumption() const {
        if (fuelAmounts.size() < 2) return 0.0; // Need at least two points to calculate consumption

        double totalConsumed = fuelAmounts.front() - fuelAmounts.back();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(timestamps.back() - timestamps.front()).count();
        if (duration == 0) return 0.0; // Avoid division by zero

        return totalConsumed / duration; // Average consumption rate (fuel units per second)
    }
};

std::chrono::system_clock::time_point parseTimestamp(const std::string& timestampStr) {
    // Assuming the timestamp is in milliseconds
    long long milliseconds = std::stoll(timestampStr);
    auto duration = std::chrono::milliseconds(milliseconds);
    return std::chrono::system_clock::time_point(duration);
}

std::unordered_map<std::string, Flight> flights;

void processPacket(const std::string& packet, const sockaddr_in& clientAddr) {
    std::istringstream ss(packet);
    std::string id, timestampStr, fuelStr;
    double fuel = 0.0;
    std::chrono::system_clock::time_point timestamp;

    std::getline(ss, id, ',');
    std::getline(ss, timestampStr, ',');
    std::getline(ss, fuelStr);

    if (!timestampStr.empty() && timestampStr.find_first_not_of("0123456789") == std::string::npos && timestampStr[0] != '-') {
        timestamp = parseTimestamp(timestampStr);

        std::time_t time_readable = std::chrono::system_clock::to_time_t(timestamp);
        if(time_readable < 0){
            std::cerr << "Timestamp before 1970-01-01 00:00:00 UTC, cannot be converted to local time.\n";
            return; // Handle time before epoch or invalid negative time
        }

        std::tm tm_buffer;
        errno_t err = localtime_s(&tm_buffer, &time_readable); // Use localtime_s to convert safely to tm struct
        if(err != 0){
            std::cerr << "Error converting time to local time using localtime_s.\n";
            return; // Handle error in conversion
        }

        std::ostringstream oss;
        oss << std::put_time(&tm_buffer, "%c"); // Convert tm struct to string using put_time
        std::cout << "Human-readable timestamp: " << oss.str() << std::endl;

    } else {
        std::cerr << "Invalid timestamp string: " << timestampStr << std::endl;
        return; // Handle invalid timestamp string
    }

    try {
        fuel = std::stod(fuelStr);
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Invalid fuel value: " << fuelStr << std::endl;
        return; // Handle invalid fuel value
    }

    if (flights.find(id) == flights.end()) {
        flights[id] = Flight(id);
    }

    flights[id].addData(fuel, timestamp);
    
    // Here you can also check if the packet is the last one for the flight and calculate the average consumption
}

void handleClientSession(SOCKET serverSocket, sockaddr_in clientAddr, std::string initialPacket) {
    // Process the initial packet
    processPacket(initialPacket, clientAddr);

    // Continue processing incoming packets
    char RxBuffer[512];
    int len = sizeof(clientAddr);

    while (true) {
        int bytesReceived = recvfrom(serverSocket, RxBuffer, 512, 0, (struct sockaddr*)&clientAddr, &len);
        if (bytesReceived > 0) {
            std::string packet(RxBuffer, bytesReceived);
            // Check for "EOF" message
            if (packet.find("EOF") != std::string::npos) {
                // Handle end of session, calculate average fuel consumption, etc.
                break;
            }
            processPacket(packet, clientAddr);
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Winsock Initialization failed." << std::endl;
        return -1;
    }

    SOCKET ServerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ServerSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return -1;
    }

    sockaddr_in SvrAddr;
    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_addr.s_addr = INADDR_ANY;
    SvrAddr.sin_port = htons(27000);
    if (bind(ServerSocket, (struct sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR) {
        std::cerr << "Binding socket failed." << std::endl;
        closesocket(ServerSocket);
        WSACleanup();
        return -1;
    }

    std::cout << "Server is ready to receive on port 27000" << std::endl;

    while (true) {
        char RxBuffer[512] = {};
        sockaddr_in CltAddr;
        int len = sizeof(CltAddr);

        int bytesReceived = recvfrom(ServerSocket, RxBuffer, 512, 0, (struct sockaddr*)&CltAddr, &len);
        if (bytesReceived > 0) {
            std::string packet(RxBuffer, bytesReceived);
            std::thread clientThread(handleClientSession, ServerSocket, CltAddr, packet);
            clientThread.detach(); // Detach the thread to allow it to run independently
        }
    }

    closesocket(ServerSocket);
    WSACleanup();
    return 0;
}