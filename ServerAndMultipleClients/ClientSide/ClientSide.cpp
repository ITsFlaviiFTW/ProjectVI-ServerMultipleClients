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

using namespace std;

chrono::system_clock::time_point parseTimestamp(const string& dateTime) {
    tm t = {};
    istringstream ss(dateTime);
    ss >> get_time(&t, "%d_%m_%Y %H:%M:%S");
    if (ss.fail()) {
        cerr << "Failed to parse timestamp: " << dateTime << endl;
        return chrono::system_clock::time_point();
    }
    return chrono::system_clock::from_time_t(mktime(&t));
}

string generateFlightID() {
    auto now = chrono::system_clock::now();
    auto now_ms = chrono::time_point_cast<chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();
    stringstream ss;
    ss << hex << value;
    return ss.str();
}

string pickFlight() {
    vector<string> telemetryFiles = {
        "Test.txt"
    };

    // Generate a random index
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, telemetryFiles.size() - 1);
    int randomIndex = dis(gen);

    // Return the randomly selected telemetry file name
    return telemetryFiles[randomIndex];
}

int main() {
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server;
    string serverAddress = "127.0.0.1"; // Server IP
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
        WSACleanup();
        return 1;
    }

    // Set up the server address structure
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, serverAddress.c_str(), &server.sin_addr);

    string id = generateFlightID();
    ifstream telemetryFile(pickFlight());
    if (!telemetryFile.is_open()) {
        cerr << "Failed to open telemetry file." << endl;
        return 1;
    }

    string line;
    bool isLastLine = false;

    while (getline(telemetryFile, line) || isLastLine) {
        if (isLastLine) {
            stringstream eofPacket;
            eofPacket << id << ",EOF";
            string eofPacketStr = eofPacket.str();
            cout << "Sending EOF packet: " << eofPacketStr << endl;
            int sendOk = sendto(sock, eofPacketStr.c_str(), eofPacketStr.size(), 0, (sockaddr*)&server, sizeof(server));
            if (sendOk == SOCKET_ERROR) {
                cerr << "Sendto failed with error: " << WSAGetLastError() << endl;
            }
            break; // Exit the loop after sending EOF
        }

        // Parse the line into timestamp and fuel
        stringstream linestream(line);
        string dateTime, fuelString;
        getline(linestream, dateTime, ',');
        getline(linestream, fuelString, ',');

        chrono::system_clock::time_point timePoint = parseTimestamp(dateTime);
        if (timePoint == chrono::system_clock::time_point()) {
            cerr << "Invalid timestamp skipped: " << dateTime << endl;
            continue;
        }

        long long timestamp = chrono::duration_cast<chrono::milliseconds>(timePoint.time_since_epoch()).count();

        stringstream packet;
        packet << id << ',' << timestamp << ',' << fuelString;

        string packetStr = packet.str();
        cout << "Sending packet: " << packetStr << endl;

        // Send the packet to the server
        int sendOk = sendto(sock, packetStr.c_str(), packetStr.size(), 0, (sockaddr*)&server, sizeof(server));
        if (sendOk == SOCKET_ERROR) {
            cerr << "Sendto failed with error: " << WSAGetLastError() << endl;
            break;
        }

        this_thread::sleep_for(chrono::milliseconds(100));

        // Check if the next read would hit EOF
        isLastLine = telemetryFile.peek() == EOF;
    }

    // Cleanup
    closesocket(sock);
    WSACleanup();
    return 0;
}