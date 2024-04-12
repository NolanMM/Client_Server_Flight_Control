#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <winsock2.h>
#include <thread>
#include <chrono>
#include <ws2tcpip.h>
#include <filesystem>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
string __file__name__ = "katl-kefd-B737-700.txt";

typedef struct {
    int flight_id_length;
    string flight_id;
    int data_length;
} HeadPacket;

typedef struct {
    string data;
} FlightData;

char* SerializedData(int& size, HeadPacket head_packet, FlightData flight_data) {
    size = sizeof(head_packet.flight_id_length) + head_packet.flight_id.length() + sizeof(head_packet.data_length) + flight_data.data.length();

    char* buffer = new char[size];
    memcpy_s(buffer, size, &head_packet.flight_id_length, sizeof(int));
    memcpy(buffer + sizeof(int), head_packet.flight_id.c_str(), head_packet.flight_id.length());
    memcpy(buffer + sizeof(int) + head_packet.flight_id.length(), &head_packet.data_length, sizeof(int));
    memcpy(buffer + sizeof(int) + head_packet.flight_id.length() + sizeof(int), flight_data.data.c_str(), flight_data.data.length());

    return buffer;
}

string extractUserID(const string& filename) {
    size_t pos = filename.find_last_of("\\/");
    if (pos != string::npos) {
        // Extract the filename without path
        string nameOnly = filename.substr(pos + 1);
        pos = nameOnly.find(".txt");
        if (pos != string::npos) {
            // Extract the substring before ".txt"
            return nameOnly.substr(0, pos);
        }
    }
    return filename; // Default case, though ideally should not occur
}

string RetrieveCurrentDirectory(const std::string& filename) {
    std::filesystem::path cwd = std::filesystem::current_path() / filename;
    return cwd.string();
}

int main() {
    // Use an absolute path for the filename
    std::string filename = RetrieveCurrentDirectory(__file__name__);

    WSADATA wsaData;
    if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        return 0;
    }

    SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientSocket == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }

    sockaddr_in SvrAddr;
    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_port = htons(27000);
    if (InetPton(AF_INET, TEXT("192.168.56.1"), &SvrAddr.sin_addr) != 1) {
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    if (connect(ClientSocket, (struct sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR) {
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        closesocket(ClientSocket);
        WSACleanup();
        return 4;
    }

    string userID = extractUserID(filename);
    string line;
    int line_count = 0;
    int size = 0;
    while (getline(file, line)) {
        HeadPacket head_packet;
        FlightData flight_data;
        head_packet.flight_id_length = userID.length();
        head_packet.flight_id = userID;
        string pattern = "FUEL TOTAL QUANTITY,";
        if (line_count == 0) {
            line = line.substr(pattern.length(), line.length() - pattern.length() - 2);
        }
        else {
            line = line.substr(1, line.length() - 3);
        }
        head_packet.data_length = line.length();
        flight_data.data = line;
        char* buffer = SerializedData(size, head_packet, flight_data);
        send(ClientSocket, buffer, size, 0);
        this_thread::sleep_for(chrono::seconds(1));
        line_count++;
        delete[] buffer; // Don't forget to free the allocated memory
    }

    closesocket(ClientSocket);
    WSACleanup();
    file.close();

    return 0;
}
