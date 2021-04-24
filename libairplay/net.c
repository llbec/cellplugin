#include "net.h"
#include <obs-module.h>

#ifdef _WIN32
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET_ERROR -1
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;
#endif

bool net_init(void)
{
	WSADATA wsa;
	int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
	if (res < 0) {
		blog(LOG_ERROR, "WSAStartup failed with error %d", res);
		return false;
	}
	return true;
}

void net_cleanup(void)
{
	WSACleanup();
}

bool net_close(socket_t socket)
{
	return !closesocket(socket);
}

socket_t net_connect(uint32_t addr, uint16_t port)
{
	blog(LOG_DEBUG, "net_connect %x %d \n", addr, port);
	socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		blog(LOG_ERROR, "connect socket invalid!");
		return INVALID_SOCKET;
	}

	SOCKADDR_IN sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(addr);
	sin.sin_port = htons(port);

	if (connect(sock, (SOCKADDR *)&sin, sizeof(sin)) == SOCKET_ERROR) {
		blog(LOG_ERROR, "connect failed!");
		net_close(sock);
		return INVALID_SOCKET;
	}

	return sock;
}

socket_t net_listen(uint32_t addr, uint16_t port, int backlog)
{
	blog(LOG_DEBUG, "net_listen %x %d \n", addr, port);
	socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		blog(LOG_ERROR, "listen socket invalid!");
		return INVALID_SOCKET;
	}

	int reuse = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse,
		       sizeof(reuse)) == -1) {
		blog(LOG_ERROR, "setsockopt(SO_REUSEADDR)");
	}

	SOCKADDR_IN sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(addr); // htonl() harmless on INADDR_ANY
	sin.sin_port = htons(port);

	if (bind(sock, (SOCKADDR *)&sin, sizeof(sin)) == SOCKET_ERROR) {
		blog(LOG_ERROR, "listen bind failed!");
		net_close(sock);
		return INVALID_SOCKET;
	}

	if (listen(sock, backlog) == SOCKET_ERROR) {
		blog(LOG_ERROR, "listen failed!");
		net_close(sock);
		return INVALID_SOCKET;
	}

	return sock;
}

socket_t net_accept(socket_t server_socket)
{
	SOCKADDR_IN csin;
	socklen_t sinsize = sizeof(csin);
	return accept(server_socket, (SOCKADDR *)&csin, &sinsize);
}

int net_recv(socket_t socket, void *buf, size_t len)
{
	return recv(socket, buf, len, 0);
}

int net_recv_all(socket_t socket, void *buf, size_t len)
{
	return recv(socket, buf, len, MSG_WAITALL);
}

int net_send(socket_t socket, const void *buf, size_t len)
{
	return send(socket, buf, len, 0);
}

int net_send_all(socket_t socket, const void *buf, size_t len)
{
	int w = 0;
	while (len > 0) {
		w = send(socket, buf, len, 0);
		if (w == -1) {
			return -1;
		}
		len -= w;
		buf = (char *)buf + w;
	}
	return w;
}

bool net_shutdown(socket_t socket, int how)
{
	return !shutdown(socket, how);
}
