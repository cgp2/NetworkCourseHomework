#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
//#include <pthread.h>

#pragma comment (lib, "Ws2_32.lib")

typedef struct SOCKET_DATA {
	SOCKET socket;
} SOCKET_DATA, *PSOCKET_DATA;


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

#define MAX_THREADS 2

DWORD WINAPI ReceiveData(LPVOID lpParam);
DWORD WINAPI CheckConnections(LPVOID lpParam);

DWORD  ThreadIdArray[MAX_THREADS];
PSOCKET_DATA SocketArray[MAX_THREADS];
HANDLE ThreadArray[MAX_THREADS];
int currentThreads;

int __cdecl main(void)
{
	for (int i = 0; i < MAX_THREADS; i++)
	{
		// Allocate memory for thread data.

		SocketArray[i] = (PSOCKET_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(SOCKET_DATA));
	}
	int error;

	// ��������� Winsock
	WSADATA wsaData;
	error = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (error != 0) {
		printf("WSAStartup failed with error: %d\n", error);
		return EXIT_FAILURE;
	}

	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// ����������� ����� � ����� �����
	struct addrinfo *localAddr = NULL;
	error = getaddrinfo(NULL, DEFAULT_PORT, &hints, &localAddr);
	if (error != 0) {
		printf("getaddrinfo failed with error: %d\n", error);
		WSACleanup();
		return EXIT_SUCCESS;
	}

	// ������� SOCKET �� ������� ����� ��������� ����������
	SOCKET listenSocket = socket(localAddr->ai_family, localAddr->ai_socktype, localAddr->ai_protocol);
	if (listenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(localAddr);
		WSACleanup();
		return EXIT_FAILURE;
	}

	// ����������� ����� TCP � ������ � ���� �����������
	error = bind(listenSocket, localAddr->ai_addr, (int)localAddr->ai_addrlen);
	if (error == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(localAddr);
		closesocket(listenSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}

	freeaddrinfo(localAddr);

	error = listen(listenSocket, SOMAXCONN);
	if (error == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}

	HANDLE MainThread = CreateThread(
		NULL,
		0,
		CheckConnections,
		NULL,
		0,
		&ThreadIdArray[currentThreads]
	);

	// ��������� ���������� �� �������
	while (true)
	{
		SOCKET clientSocket = accept(listenSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(listenSocket);
			WSACleanup();
			return EXIT_FAILURE;
		}
		if (currentThreads < MAX_THREADS)
		{
			SocketArray[currentThreads] = (PSOCKET_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				sizeof(PSOCKET_DATA));
			SocketArray[currentThreads]->socket = clientSocket;

			ThreadArray[currentThreads] = CreateThread(
				NULL,
				0,
				ReceiveData,
				&SocketArray[currentThreads]->socket,
				0,
				&ThreadIdArray[currentThreads]
			);
			currentThreads++;
		}
		else
		{
			printf("Not Enough Threads /n");
		}
		/*else
		{
			for (int i = 0; i < MAX_THREADS; i++)
			{
				SocketArray[i] = (PSOCKET_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
					sizeof(SOCKET_DATA));

			}
		}*/
		
	}

	// ���������� ����� ������ �� �����
//	closesocket(listenSocket);

	//FROM HERE RECEIVE DATA

	char c = 'a';
	scanf_s(&c);
	// ��������� ����������
	/*error = shutdown(clientSocket, SD_SEND);
	if (error == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}*/

	// �������
//	closesocket(clientSocket);
	//WSACleanup();

	return EXIT_SUCCESS;
}

DWORD WINAPI ReceiveData(LPVOID lpParam)
{
	PSOCKET_DATA data = (PSOCKET_DATA)lpParam;
	SOCKET clientSocket = data->socket;
	int bytesReceived = 0;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	do {
		bytesReceived = recv(clientSocket, recvbuf, recvbuflen, 0);
		if (bytesReceived > 0) {
			printf_s("Bytes received: %d\n", bytesReceived);

			// ���������� ��������� ������� �������
			int bytesSent = send(clientSocket, recvbuf, bytesReceived, 0);
			if (bytesSent == SOCKET_ERROR) {
				printf_s("send failed with error: %d\n", WSAGetLastError());
				closesocket(clientSocket);
				//WSACleanup();
				return EXIT_FAILURE;
			}
			printf_s("Bytes sent: %d\n", bytesSent);
		}
		else if (bytesReceived == 0)
		{
			printf_s("Connection closing...\n");
			closesocket(clientSocket);
			//WSACleanup();
		}
	} while (bytesReceived > 0);

	return 0;
}


DWORD WINAPI CheckConnections(LPVOID lpParam)
{
	while (true)
	{
		for (int i = 0; i < MAX_THREADS; i++)
		{
			if (SocketArray[i]->socket)
			{
				int nSendBytes = send(SocketArray[i]->socket, NULL, 0, 0);
				if (nSendBytes == SOCKET_ERROR)
				{
					printf_s("Client is gone\n");
					SocketArray[i] = (PSOCKET_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
						sizeof(SOCKET_DATA));
					ThreadArray[i] = NULL;
					currentThreads--;
					//WSACleanup();
				}
			}
		}
	}
	return 0;
}

