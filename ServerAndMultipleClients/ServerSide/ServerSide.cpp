// ServerSide.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

int main()
{
    std::cout << "Hello World!\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

        double totalConsumed = fuelAmounts.front() - fuelAmounts.back();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(timestamps.back() - timestamps.front()).count();
        if (duration == 0) return 0.0; // Avoid division by zero

        return totalConsumed / duration; // Average consumption rate (fuel units per second)
    }
};

// Parsing example (note this is a simplification and may not handle all your date-time formats)
std::chrono::system_clock::time_point parseTimestamp(const std::string& timestampStr) {
    std::tm tm = {};
    std::stringstream ss(timestampStr);
    ss >> std::get_time(&tm, "%d_%m_%Y %H:%M:%S");
    if (ss.fail()) {
        throw std::runtime_error("Failed to parse timestamp");
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}


std::unordered_map<std::string, Flight> flights;

void processPacket(const std::string& packet) {
    // Example packet format: ID,timestamp,fuel
    std::istringstream ss(packet);
    std::string id, timestampStr, fuelStr;
    double fuel = 0.0;
    std::chrono::system_clock::time_point timestamp;

    std::getline(ss, id, ',');
    std::getline(ss, timestampStr, ',');
    std::getline(ss, fuelStr);

    // Parse and convert timestamp and fuel from the packet
    timestamp = parseTimestamp(timestampStr);  // Function to parse timestamp
    std::getline(ss, fuelStr);
    fuel = std::stod(fuelStr);   // Convert fuel string to double

    if (flights.find(id) == flights.end()) {
        flights[id] = Flight(id);
    }

    flights[id].addData(fuel, timestamp);
}


int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;

    SOCKET ServerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ServerSocket == INVALID_SOCKET) {
        WSACleanup();
        return -1;
    }

    sockaddr_in SvrAddr;
    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_addr.s_addr = INADDR_ANY;
    SvrAddr.sin_port = htons(27000);
    if (bind(ServerSocket, (struct sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR) {
        closesocket(ServerSocket);
        WSACleanup();
        return -1;
    }

    std::cout << "Server is ready to receive on port 27000\n";

    while (true) {
        char RxBuffer[512] = {};
        sockaddr_in CltAddr;
        int len = sizeof(CltAddr);
        int bytesReceived = recvfrom(ServerSocket, RxBuffer, 512, 0, (struct sockaddr*)&CltAddr, &len);
        if (bytesReceived > 0) {
            processPacket(std::string(RxBuffer, bytesReceived));
        }
    }

    closesocket(ServerSocket);
    WSACleanup();
    return 0;
}
