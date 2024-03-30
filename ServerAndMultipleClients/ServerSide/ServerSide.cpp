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
#include <mutex>
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

std::mutex flightsMutex; // Global mutex for synchronizing access to the flights map
std::unordered_map<std::string, Flight> flights; // Global map to track flight data

void processPacket(const std::string& packet, const sockaddr_in& clientAddr) {
    std::istringstream ss(packet);
    std::string id, timestampStr, fuelStr, eofFlag;
    double fuel = 0.0;
    std::chrono::system_clock::time_point timestamp;

    // Parse the packet
    std::getline(ss, id, ',');
    if (ss.peek() == 'E') {  // Check if the next character is the beginning of "EOF"
        ss >> eofFlag;
    }
    else {
        std::getline(ss, timestampStr, ',');
        std::getline(ss, fuelStr, ',');
    }

    // Convert and validate the timestamp
    if (!timestampStr.empty() && timestampStr.find_first_not_of("0123456789") == std::string::npos && timestampStr[0] != '-') {
        timestamp = parseTimestamp(timestampStr);

        // Convert to time_t for human-readable format
        std::time_t time_readable = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm_buffer = {};
        localtime_s(&tm_buffer, &time_readable); // Use secure localtime_s in Windows

        // Format and print the timestamp with the flight ID
        std::cout << "Flight ID: " << id << " - Processed timestamp: " << std::put_time(&tm_buffer, "%c") << std::endl;
    }
    else {
        std::cerr << "Invalid timestamp string: " << timestampStr << std::endl;
        return; // Handle invalid timestamp string
    }

    // Convert and validate the fuel amount
    try {
        fuel = std::stod(fuelStr);
        // Print the fuel quantity with the flight ID
        std::cout << "Flight ID: " << id << " - Processed fuel quantity: " << fuel << " units" << std::endl;
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Invalid fuel value: " << fuelStr << std::endl;
        return; // Handle invalid fuel value
    }

    // Lock the flights map for safe access and handle the data
    std::lock_guard<std::mutex> lock(flightsMutex);
    if (flights.find(id) == flights.end()) {
        flights[id] = Flight(id); // Create a new entry if it's a new flight
    }
    flights[id].addData(fuel, timestamp); // Add the data to the flight

    // Calculate the average fuel consumption if EOF flag is set
    if (eofFlag == "EOF") {
        // Print message when EOF flag is received
        std::cout << "EOF received for Flight ID: " << id << std::endl;
        // Handle EOF: Calculate average consumption, clean up, etc.
        // ...
    }
    // Optionally, respond to the client if needed
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