#include "netpipe.h"
#include <obs-module.h>

#pragma comment(lib, "WS2_32.lib") //显式连接套接字
#define _WINSOCK_DEPRECATED_NO_WARNINGS

PipeServer::PipeServer(int p, std::string ip)
{
	sock_local = INVALID_SOCKET;
	sock_remote = INVALID_SOCKET;
	acceptThread = NULL;
	addr = ip;
	port = p;

	WSADATA wsa;
	int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
	if (res < 0) {
		blog(LOG_ERROR, "WSAStartup failed with error code %d\n", res);
		throw("WSAStartup failed");
	}
}

PipeServer::~PipeServer()
{
	Stop();
	WSACleanup();
}

int PipeServer::Start()
{
	int ret = 0;
	//创建套接字
	sock_local = (unsigned int)socket(AF_INET, SOCK_STREAM, 0);
	if (sock_local == INVALID_SOCKET) {
		blog(LOG_ERROR, "sock_srv_local INVALID_SOCKET\n");
		return -1;
	}

	//绑定套接字
	SOCKADDR_IN sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(addr.c_str());
	sin.sin_port = htons(port);

	int r = bind(sock_local, (SOCKADDR *)&sin, sizeof(sin));
	if (r < 0) {
		blog(LOG_ERROR, "sock_srv_local bind faild\n%d\n",
		     WSAGetLastError());
		ret = -2;
		goto SOCK_CLOSE;
	}

	//打开监听
	if (listen(sock_local, 1) < 0) {
		blog(LOG_ERROR, "sock_srv_local listen faild\n");
		ret = -3;
		goto SOCK_CLOSE;
	}

	//等待连接请求-子线程
	acceptThread =
		(HANDLE)_beginthreadex(NULL, 0, accept_pro, this, 0, NULL);

	return 0;

SOCK_CLOSE:
	closesocket(sock_local);
	return ret;
}

unsigned int __stdcall PipeServer::accept_pro(void *pPara)
{
	PipeServer *pipe = (PipeServer *)pPara;

	SOCKADDR clntAddr;
	int nSize = sizeof(SOCKADDR);
	pipe->sock_remote = (unsigned int)accept(
		pipe->sock_local, (SOCKADDR *)&clntAddr, &nSize);
	return 0;
}

int PipeServer::Recive(char *buf, size_t len)
{
	if (sock_remote == INVALID_SOCKET)
		return 0;
	return recv(sock_remote, buf, len, 0);
}

int PipeServer::Send(const char *buf, size_t len)
{
	if (sock_remote == INVALID_SOCKET)
		return 0;
	return send(sock_remote, buf, len, 0);
}

void PipeServer::Stop()
{
	if (acceptThread != NULL)
		CloseHandle(acceptThread);
	if (sock_remote != INVALID_SOCKET)
		closesocket(sock_remote);
	if (sock_local != INVALID_SOCKET)
		closesocket(sock_local);
}

PipeClient::PipeClient(int p, std::string ip)
{
	sock = INVALID_SOCKET;
	connectThread = NULL;
	addr = ip;
	port = p;

	WSADATA wsa;
	int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
	if (res < 0) {
		blog(LOG_ERROR, "WSAStartup failed with error code %d\n", res);
		throw("WSAStartup failed");
	}
}

PipeClient::~PipeClient()
{
	Stop();
	WSACleanup();
}

int PipeClient::Start()
{
	//创建套接字
	sock = (unsigned int)socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		blog(LOG_ERROR, "sock_srv_local INVALID_SOCKET\n");
		return -1;
	}

	//发起连接请求-子线程
	connectThread =
		(HANDLE)_beginthreadex(NULL, 0, connect_pro, this, 0, NULL);
	return 0;
}

unsigned int __stdcall PipeClient::connect_pro(void *pPara)
{
	PipeClient *pipe = (PipeClient *)pPara;

	//连接套接字
	SOCKADDR_IN sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(pipe->addr.c_str());
	sin.sin_port = htons(pipe->port);
	while (true) {
		int ret = connect(pipe->sock, (SOCKADDR *)&sin, sizeof(sin));
		if (ret == 0)
			break;
		blog(LOG_WARNING, "client connect failed(%d)!\n", ret);
	}
	return 0;
}

int PipeClient::Recive(char *buf, size_t len)
{
	if (sock == INVALID_SOCKET)
		return 0;
	return recv(sock, buf, len, 0);
}

int PipeClient::Send(const char *buf, size_t len)
{
	if (sock == INVALID_SOCKET)
		return 0;
	return send(sock, buf, len, 0);
}

void PipeClient::Stop()
{
	if (connectThread != NULL)
		CloseHandle(connectThread);
	if (sock != INVALID_SOCKET)
		closesocket(sock);
}
