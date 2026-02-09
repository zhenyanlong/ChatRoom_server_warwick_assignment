
#define _WIN32_WINNT 0x0601  
#define WIN32_LEAN_AND_MEAN  


#include <windows.h>          
#include <iostream>
#include <string>
#include <thread>             
#include <map>
#include <mutex>
#include "utils.h"



#include <winsock2.h>
#include <ws2tcpip.h>


#pragma comment(lib, "ws2_32.lib")


#define DEFAULT_PORT 8080          // Default listening port
#define DEFAULT_BUFFER_SIZE 512    // Data buffer size	


CRITICAL_SECTION g_cs_console;
std::mutex g_mutex_clients;
std::mutex g_mutex_Send;

// The first parameter is the client name, and the second parameter is the corresponding socket
std::map<std::string, SOCKET> client_sockets;

bool SendMessage(std::string client_name, std::string message) {
	std::lock_guard<std::mutex> lock(g_mutex_Send);
	auto it = client_sockets.find(client_name);
	if (it != client_sockets.end()) {
		SOCKET client_socket = it->second;
		message += "\n";
		int send_bytes = send(client_socket, message.c_str(), static_cast<int>(message.size()), 0);
		return send_bytes != SOCKET_ERROR;
	}
	return false;
}

bool SendUserList(std::string client_name) {
	std::string userlist_msg;
	{
		std::lock_guard<std::mutex> lock(g_mutex_Send);
		bool first = true;
		for (const auto& pair : client_sockets) {
			if (!first) {
				userlist_msg += ",";
			}
			userlist_msg += pair.first;
			first = false;
		}
	}
	AddCommandHeader(userlist_msg, USER_LIST_MSG);
	
	return SendMessage(client_name, userlist_msg);
}

bool BroadcastMessage(std::string sender_name, std::string message) {
	bool all_success = true;
	std::string broadcast_msg = sender_name + ": " + message;  
	AddCommandHeader(broadcast_msg, BROADCAST_MSG);
	broadcast_msg += "\n";
	std::lock_guard<std::mutex> lock(g_mutex_Send);  // Lock while iterating map
	for (const auto& pair : client_sockets) {
		SOCKET client_socket = pair.second;
		int send_bytes = send(client_socket, broadcast_msg.c_str(), static_cast<int>(broadcast_msg.size()), 0);
		if (send_bytes == SOCKET_ERROR) {
			all_success = false;
			// Print error but do not terminate broadcast
			EnterCriticalSection(&g_cs_console);
			std::cerr << "[Broadcast failed] Failed to send message to client " << pair.first << ", error code: " << WSAGetLastError() << std::endl;
			LeaveCriticalSection(&g_cs_console);
		}
	}
	return all_success;
}

bool BroadcastAddUser(std::string new_user_name) {
	bool all_success = true;
	
	std::string adduser_msg = new_user_name;  
	AddCommandHeader(adduser_msg, ADD_USER_MSG);
	adduser_msg += "\n";
	std::lock_guard<std::mutex> lock(g_mutex_Send);  // Lock while iterating map
	for (const auto& pair : client_sockets) {
		// Exclude the new user themselves
		if (pair.first == new_user_name) {
			continue;
		}
		SOCKET client_socket = pair.second;
		int send_bytes = send(client_socket, adduser_msg.c_str(), static_cast<int>(adduser_msg.size()), 0);
		if (send_bytes == SOCKET_ERROR) {
			all_success = false;
			
			EnterCriticalSection(&g_cs_console);
			std::cerr << "[Broadcast failed] Failed to send message to client " << pair.first << ", error code: " << WSAGetLastError() << std::endl;
			LeaveCriticalSection(&g_cs_console);
		}
	}
	return all_success;
}

bool BroadcastRemoveUser(std::string removed_user_name) {
	bool all_success = true;
	std::string removeuser_msg = removed_user_name;
	AddCommandHeader(removeuser_msg, REMOVE_USER_MSG);
	removeuser_msg += "\n";
	std::lock_guard<std::mutex> lock(g_mutex_Send);  // Lock while iterating map
	for (const auto& pair : client_sockets) {
		// Exclude the removed user themselves
		if (pair.first == removed_user_name) {
			continue;
		}
		SOCKET client_socket = pair.second;
		int send_bytes = send(client_socket, removeuser_msg.c_str(), static_cast<int>(removeuser_msg.size()), 0);
		if (send_bytes == SOCKET_ERROR) {
			all_success = false;
			// Print error but do not terminate broadcast
			EnterCriticalSection(&g_cs_console);
			std::cerr << "[Broadcast failed] Failed to send message to client " << pair.first << ", error code: " << WSAGetLastError() << std::endl;
			LeaveCriticalSection(&g_cs_console);
		}
	}
	return all_success;
}
// Receive client name
bool ReceiveClientName(SOCKET client_socket, std::string& client_name) {

	char name_buffer[DEFAULT_BUFFER_SIZE] = { 0 };
	int name_bytes = recv(client_socket, name_buffer, DEFAULT_BUFFER_SIZE - 1, 0);

	if (name_bytes > 0) {
		name_buffer[name_bytes] = '\0';
		client_name = std::string(name_buffer);
		// Remove newline/carriage return from name 
		client_name.erase(std::remove(client_name.begin(), client_name.end(), '\n'), client_name.end());
		client_name.erase(std::remove(client_name.begin(), client_name.end(), '\r'), client_name.end());
		return true;
	}
	else if (name_bytes == 0) {
		EnterCriticalSection(&g_cs_console);
		std::cerr << "[Client name reception] Client actively disconnected" << std::endl;
		LeaveCriticalSection(&g_cs_console);
	}
	else {
		EnterCriticalSection(&g_cs_console);
		std::cerr << "[Client name reception] Failed, error code: " << WSAGetLastError() << std::endl;
		LeaveCriticalSection(&g_cs_console);
	}
	return false;
}

// Remove client
void RemoveClient(const std::string& client_name) {
	std::lock_guard<std::mutex> lock(g_mutex_clients);
	auto it = client_sockets.find(client_name);
	if (it != client_sockets.end()) {
		closesocket(it->second);  // Ensure socket is closed
		client_sockets.erase(it);
		EnterCriticalSection(&g_cs_console);
		std::cout << "[Cleanup] Client " << client_name << " removed from the list" << std::endl;
		LeaveCriticalSection(&g_cs_console);
	}
}


void SendPrivateMessage(std::string remaining_message, std::string client_name)
{
	std::string target_name;
	std::string private_message;
	// private message format: !private target_name message_content
	SplitStringAtFirstSpace(remaining_message, target_name, private_message);

	std::string formatted_private_msg = client_name + " " + private_message;
	AddCommandHeader(formatted_private_msg, PRIVATE_MSG);
	std::cout << "[Private] From " << client_name << " to " << target_name << ": " << private_message << std::endl;
	SendMessage(target_name, formatted_private_msg);
}

// Thread function: handle communication logic for a single client (one thread per client)	
void ClientHandler(SOCKET client_socket, const char* client_ip, u_short client_port, std::string client_name) {
	// Loop to receive client data
	while (true) {
		char recv_buffer[DEFAULT_BUFFER_SIZE] = { 0 };
		int recv_bytes = recv(client_socket, recv_buffer, DEFAULT_BUFFER_SIZE - 1, 0);

		if (recv_bytes > 0) {
			recv_buffer[recv_bytes] = '\0';
			
			EnterCriticalSection(&g_cs_console);
			std::cout << "[Client " << client_ip << ":" << client_port << "] Sent data ("
				<< recv_bytes << " bytes): " << recv_buffer << std::endl;
			LeaveCriticalSection(&g_cs_console);

			std::string first_word; 
			std::string remaining_message;
			SplitStringAtFirstSpace(std::string(recv_buffer), first_word, remaining_message);

			if (first_word == BROADCAST_MSG)
			{
				// broadcast message in chat room
				BroadcastMessage(client_name, std::string(remaining_message));
			}
			else if (first_word == PRIVATE_MSG)
			{
				// send private message to specific client
				SendPrivateMessage(remaining_message, client_name);
			}
			else if (first_word == EXIT_MSG)
			{
				// client exit
				EnterCriticalSection(&g_cs_console);
				std::cout << "[Client " << client_ip << ":" << client_port << "] Requested to exit connection" << std::endl;
				LeaveCriticalSection(&g_cs_console);
				break; // exit the loop and clean up
			}
			
			
			
		}
		else if (recv_bytes == 0) {
			// Client actively disconnected
			EnterCriticalSection(&g_cs_console);
			std::cout << "[Client " << client_ip << ":" << client_port << "] Closed the connection" << std::endl;
			LeaveCriticalSection(&g_cs_console);
			break;
		}
		else {
			// Receive data failed
			EnterCriticalSection(&g_cs_console);
			std::cerr << "[Client " << client_ip << ":" << client_port << "] Receive data failed, error code: "
				<< WSAGetLastError() << std::endl;
			LeaveCriticalSection(&g_cs_console);
			break;
		}
	}

	// Clean up current client's resources
	closesocket(client_socket);
	// broadcast remove user message
	BroadcastRemoveUser(client_name);
	RemoveClient(client_name);
	EnterCriticalSection(&g_cs_console);
	std::cout << "[Client " << client_ip << ":" << client_port << "] Connection closed" << std::endl;
	LeaveCriticalSection(&g_cs_console);
}




int main() {
	InitializeCriticalSection(&g_cs_console);

	// init winsock
	WSADATA wsaData;
	int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsa_result != 0) {
		std::cerr << "WSAStartup failed, error code: " << wsa_result << std::endl;
		DeleteCriticalSection(&g_cs_console);
		return 1;
	}
	std::cout << "WinSock 2.2 initialized successfully" << std::endl;
	// Create server listening socket
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == INVALID_SOCKET) {
		std::cerr << "Failed to create listening socket, error code: " << WSAGetLastError() << std::endl;
		WSACleanup();
		DeleteCriticalSection(&g_cs_console);
		return 1;
	}
	std::cout << "Listening socket created successfully" << std::endl;

	// Configure server address structure
	sockaddr_in server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(DEFAULT_PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	// Bind the socket to the specified address and port
	if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
		std::cerr << "Failed to bind to address/port, error code: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		DeleteCriticalSection(&g_cs_console);
		return 1;
	}
	std::cout << "Successfully bound to port: " << DEFAULT_PORT << " (listening on all interfaces)" << std::endl;

	// Listen mode
	if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed, error code: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		DeleteCriticalSection(&g_cs_console);
		return 1;
	}
	std::cout << "Server is listening, waiting for client connections... (Port: " << DEFAULT_PORT << ")" << std::endl;

	// Main thread loop: continuously accept new client connections

	while (true) {
		sockaddr_in client_address = {};
		int client_address_len = sizeof(client_address);
		SOCKET client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_address), &client_address_len);

		if (client_socket == INVALID_SOCKET) {
			// Single accept failure does not terminate the server, just print error and continue listening
			std::cerr << "Failed to accept client connection, error code: " << WSAGetLastError() << std::endl;
			continue;
		}
		
		

		// Convert client IP and port to readable format
		char client_ip[INET_ADDRSTRLEN] = { 0 };
		inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
		u_short client_port = ntohs(client_address.sin_port);

		// Receive client name
		std::string client_name;
		if (!ReceiveClientName(client_socket, client_name) || client_name.empty()) {
			closesocket(client_socket);
			std::cerr << "[Client " << client_ip << ":" << client_port << "] Did not provide a valid name, closing connection" << std::endl;
			continue;
		}
		{


			// Check if the name is already taken
			std::lock_guard<std::mutex> lock(g_mutex_clients);
			if (client_sockets.find(client_name) != client_sockets.end()) {
				EnterCriticalSection(&g_cs_console);
				std::cerr << "[Client " << client_ip << ":" << client_port << "] Name " << client_name << " is already taken, closing connection" << std::endl;
				LeaveCriticalSection(&g_cs_console);
				closesocket(client_socket);
				continue;
			}
			// Store client (no longer using pointers, directly store SOCKET)
			client_sockets[client_name] = client_socket;
		}
		// Init and update user info for client
		// Broadcast new user join message to other clients
		BroadcastAddUser(client_name);
		

		// send user list to new client
		SendUserList(client_name);

		// Print new connection info
		EnterCriticalSection(&g_cs_console);
		std::cout << "New client connected - Name: " << client_name << ", IP: " << client_ip << ", Port: " << client_port << std::endl;
		LeaveCriticalSection(&g_cs_console);
		// Create a new thread to handle this client
		std::thread client_thread(ClientHandler, client_socket, client_ip, client_port, client_name);
		client_thread.detach(); 
	}

	// Clean up resources
	closesocket(server_socket);
	WSACleanup();
	DeleteCriticalSection(&g_cs_console);
	std::cout << "Server resources cleaned up, exiting program" << std::endl;
	return 0;
}