// 第一步：先定义Windows版本宏（必须放在所有头文件之前）
#define _WIN32_WINNT 0x0601  // 目标Windows 7及以上，兼容所有WinSock 2.2 API
#define WIN32_LEAN_AND_MEAN  // 排除windows.h中不必要的组件，避免引入旧版winsock.h

// 必要的头文件（严格按顺序）
#include <windows.h>          // 先包含windows.h（如果需要），但已用WIN32_LEAN_AND_MEAN避免冲突
#include <iostream>
#include <string>
#include <thread>             // 多线程核心头文件
#include <map>
#include <mutex>
#include "utils.h"


// WinSock核心头文件（必须先包含winsock2.h，再包含ws2tcpip.h）
#include <winsock2.h>
#include <ws2tcpip.h>

// 链接WinSock库（必须，否则编译报错）
#pragma comment(lib, "ws2_32.lib")

// 定义常量
#define DEFAULT_PORT 8080          // 默认监听端口
#define DEFAULT_BUFFER_SIZE 512    // 数据缓冲区大小

// 全局临界区对象：用于同步控制台输出（避免多线程打印乱码）
CRITICAL_SECTION g_cs_console;
std::mutex g_mutex_clients;

std::map<std::string, SOCKET> client_sockets;

bool SendMessage(std::string client_name, std::string message) {
	std::lock_guard<std::mutex> lock(g_mutex_clients);  
	auto it = client_sockets.find(client_name);
	if (it != client_sockets.end()) {
		SOCKET client_socket = it->second;
		int send_bytes = send(client_socket, message.c_str(), static_cast<int>(message.size()), 0);
		return send_bytes != SOCKET_ERROR;
	}
	return false;
}

bool BroadcastMessage(std::string sender_name, std::string message) {
	bool all_success = true;
	std::string broadcast_msg = sender_name + ": " + message;  // 只拼接一次

	std::lock_guard<std::mutex> lock(g_mutex_clients);  // 加锁遍历map
	for (const auto& pair : client_sockets) {
		SOCKET client_socket = pair.second;
		int send_bytes = send(client_socket, broadcast_msg.c_str(), static_cast<int>(broadcast_msg.size()), 0);
		if (send_bytes == SOCKET_ERROR) {
			all_success = false;
			// 打印错误但不终止广播
			EnterCriticalSection(&g_cs_console);
			std::cerr << "[广播失败] 给客户端 " << pair.first << " 发送消息失败，错误码: " << WSAGetLastError() << std::endl;
			LeaveCriticalSection(&g_cs_console);
		}
	}
	return all_success;
}

// 接收客户端名称（增加错误处理和超时）
bool ReceiveClientName(SOCKET client_socket, std::string& client_name) {
	// 设置接收超时（避免客户端不发名称导致阻塞）
	//DWORD timeout = 5000;  // 5秒超时
	//setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

	char name_buffer[DEFAULT_BUFFER_SIZE] = { 0 };
	int name_bytes = recv(client_socket, name_buffer, DEFAULT_BUFFER_SIZE - 1, 0);

	if (name_bytes > 0) {
		name_buffer[name_bytes] = '\0';
		client_name = std::string(name_buffer);
		// 移除名称中的换行/回车（客户端可能附带）
		client_name.erase(std::remove(client_name.begin(), client_name.end(), '\n'), client_name.end());
		client_name.erase(std::remove(client_name.begin(), client_name.end(), '\r'), client_name.end());
		return true;
	}
	else if (name_bytes == 0) {
		EnterCriticalSection(&g_cs_console);
		std::cerr << "[客户端名称接收] 客户端主动断开连接" << std::endl;
		LeaveCriticalSection(&g_cs_console);
	}
	else {
		EnterCriticalSection(&g_cs_console);
		std::cerr << "[客户端名称接收] 失败，错误码: " << WSAGetLastError() << std::endl;
		LeaveCriticalSection(&g_cs_console);
	}
	return false;
}

// 移除客户端（线程安全）
void RemoveClient(const std::string& client_name) {
	std::lock_guard<std::mutex> lock(g_mutex_clients);
	auto it = client_sockets.find(client_name);
	if (it != client_sockets.end()) {
		closesocket(it->second);  // 确保套接字关闭
		client_sockets.erase(it);
		EnterCriticalSection(&g_cs_console);
		std::cout << "[清理] 客户端 " << client_name << " 已从列表移除" << std::endl;
		LeaveCriticalSection(&g_cs_console);
	}
}


// 线程函数：处理单个客户端的通信逻辑（每个客户端一个线程）
void ClientHandler(SOCKET client_socket, const char* client_ip, u_short client_port, std::string client_name) {
	// 线程内独立的通信循环
	while (true) {
		// 接收客户端数据
		char recv_buffer[DEFAULT_BUFFER_SIZE] = { 0 };
		int recv_bytes = recv(client_socket, recv_buffer, DEFAULT_BUFFER_SIZE - 1, 0);

		if (recv_bytes > 0) {
			recv_buffer[recv_bytes] = '\0';
			// 加临界区：确保打印信息不被多线程打断
			EnterCriticalSection(&g_cs_console);
			std::cout << "[客户端 " << client_ip << ":" << client_port << "] 发送数据 ("
				<< recv_bytes << " 字节): " << recv_buffer << std::endl;
			LeaveCriticalSection(&g_cs_console);

			std::string first_word; 
			std::string remaining_message;
			SplitStringAtFirstSpace(std::string(recv_buffer), first_word, remaining_message);

			if (first_word == "!broadcast")
			{
				// broadcast message in chat room
				BroadcastMessage(client_name, std::string(remaining_message));
			}
			else
			{
				//TODO: send message to specific client
			}

			// 向客户端发送响应
			//std::string response = "服务器已收到: " + std::string(recv_buffer);
			//int send_bytes = send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
			/*if (send_bytes == SOCKET_ERROR) {
				EnterCriticalSection(&g_cs_console);
				std::cerr << "[客户端 " << client_ip << ":" << client_port << "] 发送响应失败，错误码: "
					<< WSAGetLastError() << std::endl;
				LeaveCriticalSection(&g_cs_console);
				break;
			}
			else {
				EnterCriticalSection(&g_cs_console);
				std::cout << "[客户端 " << client_ip << ":" << client_port << "] 已发送响应 ("
					<< send_bytes << " 字节): " << response << std::endl;
				LeaveCriticalSection(&g_cs_console);
			}*/
			
			
			
		}
		else if (recv_bytes == 0) {
			// 客户端主动断开连接
			EnterCriticalSection(&g_cs_console);
			std::cout << "[客户端 " << client_ip << ":" << client_port << "] 主动关闭了连接" << std::endl;
			LeaveCriticalSection(&g_cs_console);
			break;
		}
		else {
			// 接收数据失败
			EnterCriticalSection(&g_cs_console);
			std::cerr << "[客户端 " << client_ip << ":" << client_port << "] 接收数据失败，错误码: "
				<< WSAGetLastError() << std::endl;
			LeaveCriticalSection(&g_cs_console);
			break;
		}
	}

	// 清理当前客户端的资源
	closesocket(client_socket);
	RemoveClient(client_name);
	EnterCriticalSection(&g_cs_console);
	std::cout << "[客户端 " << client_ip << ":" << client_port << "] 连接已关闭" << std::endl;
	LeaveCriticalSection(&g_cs_console);
}




int main() {
	// 初始化临界区（用于线程同步输出）
	InitializeCriticalSection(&g_cs_console);

	// 1. 初始化WinSock库
	WSADATA wsaData;
	int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsa_result != 0) {
		std::cerr << "WSAStartup失败，错误码: " << wsa_result << std::endl;
		DeleteCriticalSection(&g_cs_console);
		return 1;
	}
	std::cout << "WinSock 2.2 初始化成功" << std::endl;

	// 2. 创建服务器监听套接字（TCP）
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == INVALID_SOCKET) {
		std::cerr << "创建监听套接字失败，错误码: " << WSAGetLastError() << std::endl;
		WSACleanup();
		DeleteCriticalSection(&g_cs_console);
		return 1;
	}
	std::cout << "监听套接字创建成功" << std::endl;

	// 3. 配置服务器地址结构
	sockaddr_in server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(DEFAULT_PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	// 4. 绑定地址和端口
	if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
		std::cerr << "绑定地址/端口失败，错误码: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		DeleteCriticalSection(&g_cs_console);
		return 1;
	}
	std::cout << "成功绑定到端口: " << DEFAULT_PORT << " (监听所有网卡)" << std::endl;

	// 5. 监听模式
	if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "监听失败，错误码: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		DeleteCriticalSection(&g_cs_console);
		return 1;
	}
	std::cout << "服务器开始监听，等待客户端连接...（端口: " << DEFAULT_PORT << "）" << std::endl;

	// 6. 主线程循环：持续接受新客户端连接

	while (true) {
		sockaddr_in client_address = {};
		int client_address_len = sizeof(client_address);
		SOCKET client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_address), &client_address_len);

		if (client_socket == INVALID_SOCKET) {
			// 单个accept失败不终止服务器，仅打印错误后继续监听
			std::cerr << "接受客户端连接失败，错误码: " << WSAGetLastError() << std::endl;
			continue;
		}
		
		

		// 转换客户端IP和端口为可读格式
		char client_ip[INET_ADDRSTRLEN] = { 0 };
		inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
		u_short client_port = ntohs(client_address.sin_port);

		// 接收客户端名称
		std::string client_name;
		if (!ReceiveClientName(client_socket, client_name) || client_name.empty()) {
			closesocket(client_socket);
			std::cerr << "[客户端 " << client_ip << ":" << client_port << "] 未提供有效名称，关闭连接" << std::endl;
			continue;
		}

		// 检查名称是否重复
		std::lock_guard<std::mutex> lock(g_mutex_clients);
		if (client_sockets.find(client_name) != client_sockets.end()) {
			EnterCriticalSection(&g_cs_console);
			std::cerr << "[客户端 " << client_ip << ":" << client_port << "] 名称 " << client_name << " 已被占用，关闭连接" << std::endl;
			LeaveCriticalSection(&g_cs_console);
			closesocket(client_socket);
			continue;
		}

		// 存储客户端（不再用指针，直接存SOCKET）
		client_sockets[client_name] = client_socket;

		// 打印新连接信息
		EnterCriticalSection(&g_cs_console);
		std::cout << "新客户端连接 - 名称: " << client_name << ", IP: " << client_ip << ", 端口: " << client_port << std::endl;
		LeaveCriticalSection(&g_cs_console);
		// 7. 创建新线程处理该客户端（detach模式：主线程不等待子线程结束）
		std::thread client_thread(ClientHandler, client_socket, client_ip, client_port, client_name);
		client_thread.detach(); // 分离线程，由系统管理生命周期
	}

	// 8. 清理资源（实际中主线程循环不会退出，此处为规范写法）
	closesocket(server_socket);
	WSACleanup();
	DeleteCriticalSection(&g_cs_console);
	std::cout << "服务器资源已清理，程序退出" << std::endl;
	return 0;
}