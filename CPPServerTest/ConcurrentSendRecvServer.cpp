#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <conio.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFFER_SIZE 1024

std::atomic<bool> close = false;

void Send(SOCKET  client_socket) {
    int count = 0;
    // can use close.load(std::memory_order_relaxed) - it is better
    while (!close) {
        if (_kbhit()) { // non-blocking keyboard input 
            std::cout << "Send(" << count++ << "): ";
            std::string sentence;

            std::getline(std::cin, sentence); 
            if (sentence.empty()) sentence = " "; // in case we send just newline 

            if (sentence == "!bye") {
                close = true;
                std::cout << "Exiting\n";
            }

            // Send the sentence to the server
            if (send(client_socket, sentence.c_str(), static_cast<int>(sentence.size()), 0) == SOCKET_ERROR) {
                if (close) std::cout << "Terminating\n";
                else std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
                break;
            }
        }
    }
    closesocket(client_socket); // send does closing
}



void Receive(SOCKET client_socket) {
    int count = 0; 
    while (!close) {
        // Receive the reversed sentence from the server
        char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
        int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the received data
            std::cout << "Received(" << count++ << "): " << buffer << std::endl;
        }
        else if (bytes_received == 0) {
            std::cout << "Connection closed by server." << std::endl;
        }
        else if (close) {
            std::cout << "Terminating connection\n";
        }
        else {
            std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
        }
        if (strcmp(buffer, "!bye") == 0) {
            close = true; 
        }
    }
}


int server() {
    // Step 1: Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Step 2: Create a socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Step 3: Bind the socket
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(65432);  // Server port
    server_address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Step 4: Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 65432..." << std::endl;

    // Step 5: Accept a connection
    sockaddr_in client_address = {};
    int client_address_len = sizeof(client_address);
    SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;


   // Receive(client_socket);
   // Send(client_socket);
    std::thread t1 = std::thread(Send, client_socket);
    std::thread t2 = std::thread(Receive, client_socket);

    t1.join();
    t2.join(); 
   
    // Step 7: Clean up
  //  closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;

}

int main1() {
    server(); 
	return 0;
}