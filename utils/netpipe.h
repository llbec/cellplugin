#pragma once
#include <process.h>
#include <windows.h>
#include <string>

class PipeServer {
public:
	PipeServer(int p, std::string ip = "127.0.0.1");
	~PipeServer();

	int Start();
	void Stop();
	int Recive(char *buf, size_t len);
	int Send(const char *buf, size_t len);

private:
	HANDLE acceptThread;
	
	SOCKET sock_local;
	SOCKET sock_remote;

	std::string addr;
	int port;

	static unsigned int __stdcall accept_pro(void *pPara);
};

class PipeClient {
public:
	PipeClient(int p, std::string ip = "127.0.0.1");
	~PipeClient();

	int Start();
	void Stop();
	int Recive(char *buf, size_t len);
	int Send(const char *buf, size_t len);

private:
	HANDLE connectThread;
	SOCKET sock;

	std::string addr;
	int port;

	static unsigned int __stdcall connect_pro(void *pPara);
};
