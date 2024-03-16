#include <iostream>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <winsock2.h>
#include <windows.networking.sockets.h>
#include <Ws2tcpip.h>
#include <random>

#pragma comment(lib,"ws2_32.lib")

// Parses the date-time string into a time_point
std::chrono::system_clock::time_point parseTimestamp(const std::string& dateTime) {
    std::tm t = {};
    std::istringstream ss(dateTime);
    ss >> std::get_time(&t, "%d_%m_%Y %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&t));
}

// Function to generate a unique flight ID based on the current time
std::string generateFlightID() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();

    // Generate a unique flight ID based on the current time
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

std::string pickFlight() {
    std::vector<std::string> telemetryFiles = {
        "katl-kefd-B737-700.txt",
        "Telem_2023_3_12 14_56_40.txt",
        "Telem_2023_3_12 16_26_4.txt",
        "Telem_czba-cykf-pa28-w2_2023_3_1 12_31_27.txt"
    };

    // Generate a random index
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, telemetryFiles.size() - 1);
    int randomIndex = dis(gen);

    // Return the randomly selected telemetry file name
    return telemetryFiles[randomIndex];
}

int main() {
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server;
    char buffer[512];
    std::string serverAddress = "127.0.0.1"; // Server IP
    int port = 27000; // Server port

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Winsock Initialization failed." << std::endl;
        return 1;
    }

    // Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        return 1;
    }

    // Set up the server address structure
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    //server.sin_addr.S_un.S_addr = inet_addr(serverAddress.c_str());
    inet_pton(AF_INET, serverAddress.c_str(), &server.sin_addr);

    // Open the telemetry data file
    std::ifstream telemetryFile(pickFlight());
    std::string line;
    std::string id = generateFlightID();

    if (!telemetryFile.is_open()) {
        std::cerr << "Failed to open telemetry file." << std::endl;
        return 1;
    }

    // Read and send each line of the file
    while (std::getline(telemetryFile, line)) {
        std::stringstream linestream(line);
        std::string timestamp, fuelString;
        std::getline(linestream, timestamp, ',');
        std::getline(linestream, fuelString, ',');

        // Remove any leading whitespace
        fuelString.erase(0, fuelString.find_first_not_of(" "));

        // Create the packet data
        std::stringstream packet;
        packet << id << ',' << parseTimestamp(timestamp).time_since_epoch().count() << ',' << fuelString;

        // Send the packet to the server
        int sendOk = sendto(sock, packet.str().c_str(), packet.str().size(), 0, (sockaddr*)&server, sizeof(server));
        if (sendOk == SOCKET_ERROR) {
            std::cerr << "Sendto failed with error: " << WSAGetLastError() << std::endl;
            break;
        }

        // Wait a bit before sending the next packet
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    closesocket(sock);
    WSACleanup();
    return 0;
}
