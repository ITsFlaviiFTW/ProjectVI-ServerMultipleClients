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

// Parses the date-time string into a time_point
chrono::system_clock::time_point parseTimestamp(const string& dateTime) {
    tm t = {};
    istringstream ss(dateTime);
    ss >> get_time(&t, "%d_%m_%Y %H:%M:%S");
    return chrono::system_clock::from_time_t(mktime(&t));
}

// Function to generate a unique flight ID based on the current time
string generateFlightID() {
    auto now = chrono::system_clock::now();
    auto now_ms = chrono::time_point_cast<chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();

    // Generate a unique flight ID based on the current time
    stringstream ss;
    ss << hex << value;
    return ss.str();
}

string pickFlight() {
    vector<string> telemetryFiles = {
        "katl-kefd-B737-700.txt",
        "Telem_2023_3_12 14_56_40.txt",
        "Telem_2023_3_12 16_26_4.txt",
        "Telem_czba-cykf-pa28-w2_2023_3_1 12_31_27.txt"
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
    char buffer[512];
    string serverAddress = "127.0.0.1"; // Server IP
    int port = 27000; // Server port

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Winsock Initialization failed." << endl;
        return 1;
    }

    // Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "Socket creation failed." << endl;
        return 1;
    }

    // Set up the server address structure
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    //server.sin_addr.S_un.S_addr = inet_addr(serverAddress.c_str());
    inet_pton(AF_INET, serverAddress.c_str(), &server.sin_addr);

    // Open the telemetry data file
    ifstream telemetryFile(pickFlight());
    string line;
    string id = generateFlightID();

    if (!telemetryFile.is_open()) {
        cerr << "Failed to open telemetry file." << endl;
        return 1;
    }

    // Read and send each line of the file
    while (getline(telemetryFile, line)) {
        // Check if it's the last line in the file
        bool eof = telemetryFile.eof();

        stringstream linestream(line);
        string timestamp, fuelString;
        getline(linestream, timestamp, ',');
        getline(linestream, fuelString, ',');

        // Remove any leading whitespace
        fuelString.erase(0, fuelString.find_first_not_of(" "));

        // Create the packet data
        stringstream packet;
        packet << id << ',' << parseTimestamp(timestamp).time_since_epoch().count() << ',' << fuelString << ',' << eof;

        // Send the packet to the server
        int sendOk = sendto(sock, packet.str().c_str(), packet.str().size(), 0, (sockaddr*)&server, sizeof(server));
        if (sendOk == SOCKET_ERROR) {
            cerr << "Sendto failed with error: " << WSAGetLastError() << endl;
            break;
        }

        // If it's the last line, exit the loop
        if (eof) {
            break;
        }

        // Wait a bit before sending the next packet
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    // Cleanup
    closesocket(sock);
    WSACleanup();
    return 0;
}
