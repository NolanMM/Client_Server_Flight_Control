// Client.cpp
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <winsock2.h>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

std::string filename = "katl-kefd-B737-700.txt";

using namespace std;

typedef struct {
    int flight_id_length;
    string flight_id;
	int data_length;
}HeadPacket;

typedef struct {
	string data;
} FlightData;

char* SerializedData(int& size, HeadPacket head_packet, FlightData flight_data)
{
    size = sizeof(head_packet.flight_id_length) + head_packet.flight_id.length() + sizeof(head_packet.data_length) + flight_data.data.length();

    char* buffer = new char[size];
    memcpy_s(buffer, size, &head_packet.flight_id_length, sizeof(int));
    memcpy(buffer + sizeof(int), head_packet.flight_id.c_str(), head_packet.flight_id.length());
    memcpy(buffer + sizeof(int) + head_packet.flight_id.length(),& head_packet.data_length, sizeof(int));
    memcpy(buffer + sizeof(int) + head_packet.flight_id.length() + sizeof(int), flight_data.data.c_str(), flight_data.data.length());

    return buffer;
}

string extractUserID(const string& filename) {
    size_t pos = filename.find(".txt");
    if (pos != string::npos) {
        // Extract the substring before ".txt"
        return filename.substr(0, pos);
    }
    return filename;
}

int main() {
    //starts Winsock DLLs
    WSADATA wsaData;
    if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        return 0;
    }

    //initializes socket. SOCK_STREAM: TCP
    SOCKET ClientSocket;
    ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientSocket == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }

    //Connect socket to specified server
    sockaddr_in SvrAddr;
    SvrAddr.sin_family = AF_INET;						//Address family type itnernet
    SvrAddr.sin_port = htons(27000);				    //port (host to network conversion)
    SvrAddr.sin_addr.s_addr = inet_addr("127.0.0.1");   //IP address
    if ((connect(ClientSocket, (struct sockaddr*)&SvrAddr, sizeof(SvrAddr))) == SOCKET_ERROR) {
        closesocket(ClientSocket);
        WSACleanup();
        return 0;
    }

    ifstream file(filename);
    string userID = extractUserID(filename);
    if (!file.is_open()) {
        cerr << "Error opening file" << endl;
        closesocket(ClientSocket);
        WSACleanup();
        return 4;
    }

    string line;
    int line_count = 0;
    int size = 0;
    while (getline(file, line)) {
        HeadPacket head_packet;
        FlightData flight_data;
        head_packet.flight_id_length = userID.length();
        head_packet.flight_id = userID;
        // Check if line contain this pattern "FUEL TOTAL QUANTITY,"
        string pattern = "FUEL TOTAL QUANTITY,";
        if (line_count == 0)
        {
            line = line.substr(pattern.length(), line.length() - pattern.length() - 2);
        }
        else
        {
            line = line.substr(1, line.length() - 3);
        }
        head_packet.data_length = line.length();
        flight_data.data = line;
        char* buffer = SerializedData(size, head_packet, flight_data);
        send(ClientSocket, buffer, size, 0);
        this_thread::sleep_for(chrono::seconds(2));
        line_count++;
        //delete[] buffer;
    }

    // Close socket
    closesocket(ClientSocket);
    WSACleanup();
    file.close();

    return 0;
}
