// Server.cpp
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <sstream>
#include <fstream>
#include <cstring>
#include <winsock2.h>
#include <unordered_map>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

mutex mtx;

typedef struct {
    int flight_id_length;
    string flight_id;
    int data_length;
}HeadPacket;

typedef struct {
    string flight_id;
    string data;
} FlightData;

// Data structure to hold flight information
struct FlightData_Extract {
    string time;
    double fuelRemaining;

    void print() {
        cout << "Time: " << time << ", Fuel remaining: " << fuelRemaining << endl;
    }
};

// Vector to store flight data for each client
unordered_map<string, vector<FlightData_Extract>> clientFlightData;


FlightData DeserializeData(char* buffer, int size)
{
    HeadPacket head_packet;
    memcpy_s(&head_packet.flight_id_length, sizeof(int), buffer, sizeof(int));
    buffer += sizeof(int);
    head_packet.flight_id = string(buffer, head_packet.flight_id_length);
    buffer += head_packet.flight_id_length;
    memcpy_s(&head_packet.data_length, sizeof(int), buffer, sizeof(int));
    buffer += sizeof(int);
    FlightData flight_data;
    flight_data.data = string(buffer, head_packet.data_length);
    flight_data.flight_id = head_packet.flight_id;
    return flight_data;
}

FlightData_Extract extractData(const string& data) {
    stringstream ss(data);
    string token;
    FlightData_Extract flightData;
    getline(ss, token, ',');
    flightData.time = token;
    getline(ss, token, ',');
    flightData.fuelRemaining = stod(token);
    return flightData;
}

// Function to save final average fuel consumption
bool saveFinalAverageFuelConsumption(const string& filename, double avgFuelConsumption, string flight_date_start, string flight_date_end) {
    try {
        ofstream file(filename, ios::app);
        if (!file.is_open()) {
            // File opened failed
            return false;
        }
        // Write data to the top of the file
        file.seekp(0, ios::beg);
        file << "Flight Dates: " << flight_date_start << " - " << flight_date_end << " - Final average fuel consumption: " << avgFuelConsumption << endl;
        file.close();
        return true;
    }
    catch (const std::exception&) {
        // Exception occurred
        return false;
    }
}

// Function to handle client connection
void handleClient(SOCKET clientSocket) {
    // Receive data from client
    int line_count = 0;

    double Flight_Consumption = 0;
    string flight_id;
    string flight_date_start;
    string flight_date_end;
    double previous_fuel = 0;

    while (true) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            cout << "Client disconnected" << endl;
            break;
        }
        cout << "Client: " << flight_id << " connected" << endl;
        FlightData flight_data = DeserializeData(buffer, bytesReceived);
        flight_id = flight_data.flight_id;
        FlightData_Extract data = extractData(flight_data.data);
        lock_guard<mutex> lock(mtx);
        clientFlightData[flight_data.flight_id].push_back(data);
        // cv.notify_one();
        if (line_count == 0) {
            flight_id = flight_data.flight_id;
            flight_date_start = data.time;
            previous_fuel = data.fuelRemaining;
        }
        else
        {
            flight_date_end = data.time;
            Flight_Consumption += previous_fuel - data.fuelRemaining;
            previous_fuel = data.fuelRemaining;
        }
        line_count++;
    }
    double AverageFuelConsumption = Flight_Consumption / line_count;
    bool isSaved = saveFinalAverageFuelConsumption(flight_id, AverageFuelConsumption, flight_date_start, flight_date_end);
    if (isSaved) {
        cout << "Final average of flight: " << flight_id << " fuel consumption saved to file" << endl;
    }
    else {
        cout << "Failed to save final average of flight: " << flight_id << " fuel consumption" << endl;
    }
    clientFlightData.erase(flight_id);
    closesocket(clientSocket);
}

int main() {
    // Initialize Winsock
    //starts Winsock DLLs		
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return 0;

    //create server socket
    SOCKET ServerSocket;
    ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ServerSocket == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }

    //binds socket to address
    sockaddr_in SvrAddr;
    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_addr.s_addr = INADDR_ANY;
    SvrAddr.sin_port = htons(27000);
    if (bind(ServerSocket, (struct sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR)
    {
        closesocket(ServerSocket);
        WSACleanup();
        return 0;
    }
    //listen on a socket
    if (listen(ServerSocket, 1) == SOCKET_ERROR) {
        closesocket(ServerSocket);
        WSACleanup();
        return 0;
    }

    // Accept connections
    cout << "Server started. Waiting for clients..." << endl;
    sockaddr_in client;
    int clientSize = sizeof(client);
    while (true) {
        //accepts a connection from a client
        SOCKET ConnectionSocket;
        ConnectionSocket = SOCKET_ERROR;
        if ((ConnectionSocket = accept(ServerSocket, NULL, NULL)) == SOCKET_ERROR) {
            closesocket(ServerSocket);
            WSACleanup();
            return 0;
        }
        // Handle client in a separate thread
        thread t(handleClient, ConnectionSocket);
        t.detach();
    }

    // Close socket
    closesocket(ServerSocket);
    WSACleanup();

    return 0;
}
