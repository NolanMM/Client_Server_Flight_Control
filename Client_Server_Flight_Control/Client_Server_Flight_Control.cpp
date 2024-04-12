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
#include <ws2tcpip.h>
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
bool saveFinalAverageFuelConsumption(const string& filename, double avgFuelConsumption, string flight_date_start, string flight_date_end, bool end_flight) {
    try {
        if (end_flight)
        {
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
        else
        {
            ofstream file(filename, ios::app);
            if (!file.is_open()) {
                // File opened failed
                return false;
            }
            // Write data to the top of the file
            file.seekp(0, ios::beg);
            file << "Data Dates Recorded: " << flight_date_end << " - Fuel consumption: " << avgFuelConsumption << endl;
            file.close();
            return true;
        }

    }
    catch (const std::exception&) {
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
    double AverageFuelConsumption = 0;
    while (true) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            cout << "Client disconnected" << endl;
            break;
        }
        FlightData flight_data = DeserializeData(buffer, bytesReceived);
        if (flight_id.empty()) {
			cout << "Strange flight ID: " << flight_data.flight_id << " just connected and send first data record!" << endl;
		}
		else {
            cout << "Flight: " << flight_id << " connected and update the data" << endl;
        }
        flight_id = flight_data.flight_id;
        FlightData_Extract data = extractData(flight_data.data);
        lock_guard<mutex> lock(mtx);
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
        AverageFuelConsumption = Flight_Consumption / (line_count + 1);
        string filename = flight_id + ".txt";
        bool isSaved = saveFinalAverageFuelConsumption(filename, AverageFuelConsumption, flight_date_start, flight_date_end, false);
        if (isSaved) {
            cout << "Fuel consumption of flight: " << flight_id << " Upadated, Value: " << AverageFuelConsumption << endl;
        }
        else {
            cout << "Failed to save final average of flight: " << flight_id << " fuel consumption" << endl;
        }
    }
    double AverageFuelConsumption_ = Flight_Consumption / line_count;
    string filename = flight_id + ".txt";
    bool isSaved = saveFinalAverageFuelConsumption(filename, AverageFuelConsumption_, flight_date_start, flight_date_end, true);
    if (isSaved) {
        cout << "Final average of flight: " << flight_id << " fuel consumption saved to file" << endl;
    }
    else {
        cout << "Failed to save final average of flight: " << flight_id << " fuel consumption" << endl;
    }
    closesocket(clientSocket);
}

int main() {	
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
    // Convert IP address to binary form
    if (InetPton(AF_INET, TEXT("192.168.56.1"), &SvrAddr.sin_addr) != 1) {
        closesocket(ServerSocket);
        WSACleanup();
        return 1;
    }

    SvrAddr.sin_port = htons(27000);
    if (bind(ServerSocket, (struct sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR)
    {
        closesocket(ServerSocket);
        WSACleanup();
        return 0;
    }
    //listen on a socket
    if (listen(ServerSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(ServerSocket);
        WSACleanup();
        return 1;
    }

    // Accept connections
    cout << "Flight Station started. Waiting for flights connect..." << endl;
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
