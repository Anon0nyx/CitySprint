#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unordered_map>
#include <functional>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define WSAGetLastError() (errno)
#endif

extern std::unordered_map<std::string, std::string> mime_types;
extern std::unordered_map<std::string, std::function<std::string(const std::string&)>> get_routes;
extern std::unordered_map<std::string, std::function<std::string(const std::string&)>> post_routes;

void initialize_mime_types();
void initialize_routes();
void handle_client(SOCKET client_sock);
std::string get_mime_type(const std::string& path);
std::string get_file_content(const std::string& path);
std::string handle_get_request(const std::string& path);
std::string handle_post_request(const std::string& path, const std::string& body);
std::string get_current_time();

#endif // SERVER_H
