#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <list>
#include <string>
#include <iostream>
#include "EncoderDecoder.h"

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

using namespace std;

struct IoOperationData {
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	CHAR buffer[DEFAULT_BUFLEN];
	DWORD bytesSent;
	DWORD bytesRecv;
};

struct ConnectionData {
	SOCKET socket;
};

struct UsersData;

struct ChannelData
{
	string Name;
	list<UsersData*> Users;
};

struct UsersData
{
	string Name;
	list<ChannelData*> CurrentChannels;
	ConnectionData* UserConnectionData;
};



list <UsersData*> CurrentUsers;
list<ChannelData*> CurrentChannels;


DWORD WINAPI ServerWorkerThread(LPVOID pCompletionPort);

int __cdecl main(void)
{
	int error;

	WSADATA wsaData;
	error = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (error != 0) {
		printf("WSAStartup failed with error: %d\n", error);
		return EXIT_FAILURE;
	}

	// Создаем порт завершения
	HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hCompletionPort == NULL) {
		printf("CreateIoCompletionPort failed with error %d\n", GetLastError());
		WSACleanup();
		return EXIT_FAILURE;
	}

	// Определяеи количество процессоров в системе
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	// Создаем рабочие потоки в зависимости от количества процессоров, по два потока на процессор
	for (int i = 0; i < (int)systemInfo.dwNumberOfProcessors * 2; ++i) {
		// Создаем поток и передаем в него порт завершения
		DWORD threadId;
		HANDLE hThread = CreateThread(NULL, 0, ServerWorkerThread, hCompletionPort, 0, &threadId);
		if (hThread == NULL) {
			printf("CreateThread() failed with error %d\n", GetLastError());
			WSACleanup();
			CloseHandle(hCompletionPort);
			return EXIT_FAILURE;
		}

		// Закрываем дескриптор потока, поток при этом не завершается
		CloseHandle(hThread);
	}

	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Преобразуем адрес и номер порта
	struct addrinfo *localAddr = NULL;
	error = getaddrinfo(NULL, DEFAULT_PORT, &hints, &localAddr);
	if (error != 0) {
		printf("getaddrinfo failed with error: %d\n", error);
		WSACleanup();
		return EXIT_FAILURE;
	}

	SOCKET listenSocket = WSASocketW(localAddr->ai_family, localAddr->ai_socktype, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(localAddr);
		WSACleanup();
		return EXIT_FAILURE;
	}

	// Привязываем сокет TCP к адресу и ждем подключения
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

	cout << "Start socket listening\n";

	// Принимаем соединения и связывкем их с портом завершения
	for (; ; )
	{
		SOCKET clientSocket = WSAAccept(listenSocket, NULL, NULL, NULL, 0);
		if (clientSocket == SOCKET_ERROR) {
			printf("WSAAccept failed with error %d\n", WSAGetLastError());
			return EXIT_FAILURE;
		}
		printf_s("New client connected \r\n");

		ConnectionData* conData = new ConnectionData;
		conData->socket = clientSocket;
		// Связываем клиентский сокет с портом завершения
		if (CreateIoCompletionPort((HANDLE)clientSocket, hCompletionPort, (ULONG_PTR)conData, 0) == NULL)
		{
			printf("CreateIoCompletionPort failed with error %d\n", GetLastError());
			return EXIT_FAILURE;
		}

		//  Создаем структуру для операций ввода-вывода и запускаем обработку
		IoOperationData *pIoData = new IoOperationData;
		ZeroMemory(&(pIoData->overlapped), sizeof(OVERLAPPED));
		pIoData->bytesSent = 0;
		pIoData->bytesRecv = 0;
		pIoData->wsaBuf.len = DEFAULT_BUFLEN;
		pIoData->wsaBuf.buf = pIoData->buffer;

		DWORD flags = 0;
		DWORD bytesRecv;
		if (WSARecv(clientSocket, &(pIoData->wsaBuf), 1, &bytesRecv, &flags, &(pIoData->overlapped), NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				printf("WSARecv failed with error %d\n", WSAGetLastError());
				return EXIT_FAILURE;
			}
		}
	}
}

bool SendPacket(Packet* packet, ConnectionData* toUser)
{
	string sendString = packet->BuildString();
	const char *sendbuf = sendString.data();
	int bytesSent = send(toUser->socket, sendbuf, (int)strlen(sendbuf), 0);
	if (bytesSent == SOCKET_ERROR)
	{
		printf("send failed with error: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool ValidateUserName(string userName)
{
	for (UsersData* userData : CurrentUsers)
	{
		if (userData->Name == userName)
			return false;
	}
	return true;
}

bool ValidateChannelName(string channelName)
{
	if (channelName[0] != '$')
		return false;

	for (ChannelData* channelData : CurrentChannels)
	{
		if (channelData->Name == channelName)
			return false;
	}
	return true;
}

ChannelData* FindChannel(string channelName)
{
	for (ChannelData* channel : CurrentChannels)
	{
		if (channel->Name == channelName)
			return channel;
	}
	return nullptr;
}

UsersData* FindUser(ConnectionData* conData)
{
	for (UsersData* user : CurrentUsers)
	{
		if (user->UserConnectionData->socket == conData->socket)
			return user;
	}
	return nullptr;
}

bool ChannelContainUser(ChannelData* channel, UsersData* user)
{
	for (UsersData* us : channel->Users)
	{
		if (us->Name == user->Name)
			return true;
	}
	return false;
}


void OnClientConnected(IDPacket* packet, ConnectionData* conData)
{
	bool isValidUserName = ValidateUserName(packet->Name);
	if (isValidUserName)
	{
		UsersData* userData = new UsersData;
		userData->Name = packet->Name;
		userData->UserConnectionData = conData;

		CurrentUsers.push_back(userData);

		cout << "Hello " << packet->Name << "\n";
	}

	ResponcePacket* respPacket = new ResponcePacket;
	respPacket->responce = isValidUserName;

	SendPacket(respPacket, conData);
}

void OnClientDisconected(ConnectionData* clientConnectionData)
{
	UsersData* user = FindUser(clientConnectionData);
	if (user == nullptr)
		return;

	for (ChannelData* userChannel : user->CurrentChannels)
		LeaveChannel(userChannel, user);

	CurrentUsers.remove(user);
	cout << "Bye Bye " << user->Name << "\n";
	closesocket(clientConnectionData->socket);
}

void JoinChannel(ChannelData* channel, UsersData* user)
{
	if (!ChannelContainUser(channel, user))
	{
		channel->Users.push_back(user);
		user->CurrentChannels.push_back(channel);
		cout << "User " << user->Name << " joined channel " << channel->Name << "\n";
	}
}

void LeaveChannel(ChannelData* channel, UsersData* user)
{
	if (ChannelContainUser(channel, user))
	{
		for (UsersData* us : channel->Users)
		{
			if (us->Name == user->Name)
			{
				channel->Users.remove(user);
				break;
			}
				
		}
		//channel->Users.remove(user);
		user->CurrentChannels.remove(channel);
		cout << "User " << user->Name << " leaved channel " << channel->Name << "\n";
	}

	ResponcePacket* respPacket = new ResponcePacket;
	respPacket->responce = true;

	SendPacket(respPacket, user->UserConnectionData);
}

void OnChannelCreating(ChanelIDPacket* packet, ConnectionData* conData)
{
	bool isValidChannel = ValidateChannelName(packet->Name);
	if (isValidChannel)
	{
		ChannelData* channelData = new ChannelData;
		channelData->Name = packet->Name;

		CurrentChannels.push_back(channelData);

		UsersData* user = FindUser(conData);
		JoinChannel(channelData, user);

		cout << "New Channel Created " << packet->Name << "!\n";
	}

	ResponcePacket* respPacket = new ResponcePacket;
	respPacket->responce = isValidChannel;

	SendPacket(respPacket, conData);
}



void OnRequestReceive(RequestPacket* requestPacket, ConnectionData* fromUser)
{
	if (requestPacket->requestFor == "Users")
	{
		NamesPacket* userNamesPacket = new NamesPacket;
		for (UsersData* users : CurrentUsers)
		{
			userNamesPacket->Names.push_back(users->Name);
		}

		SendPacket(userNamesPacket, fromUser);
		//delete userNamesPacket;
		return;
	}
	if (requestPacket->requestFor == "Channels")
	{
		NamesPacket* channelNamesPacket = new NamesPacket;
		for (ChannelData* channel: CurrentChannels)
		{
			channelNamesPacket->Names.push_back(channel->Name);
		}

		SendPacket(channelNamesPacket, fromUser);
		//delete userNamesPacket;
		return;
	}
}

void TransferMessageToUser(MSGPacket* msgPacket)
{
	for (UsersData* userData : CurrentUsers)
	{
		if (userData->Name == msgPacket->ReceiverName)
		{
			SendPacket(msgPacket, userData->UserConnectionData);
			return;
		}
	}
}

void OnPacketReceived(Packet* packet, ConnectionData* fromClient)
{
	if (packet->Command == "ID")
	{
		IDPacket* idPacket = dynamic_cast<IDPacket*>(packet);

		OnClientConnected(idPacket, fromClient);

		//cout << CurrentUsers.size() <<"\n";
		delete idPacket;
		return;
	}
	else if (packet->Command == "CHID")
	{
		ChanelIDPacket* chidPacket = dynamic_cast<ChanelIDPacket*>(packet);

		ChannelData* channel = FindChannel(chidPacket->Name);
		if (channel == nullptr)
		{
			OnChannelCreating(chidPacket, fromClient);
		}
		else
		{
			UsersData* user = FindUser(fromClient);
			JoinChannel(channel, user);

			ResponcePacket* respPacket = new ResponcePacket;
			respPacket->responce = true;

			SendPacket(respPacket, fromClient);
		}


		//cout << CurrentChannels.size() << "\n";
		delete chidPacket;
		return;
	}
	else if (packet->Command == "MSG")
	{
		MSGPacket* msgPacket = dynamic_cast<MSGPacket*>(packet);
		if (msgPacket->ReceiverName[0] != '$')
		{
			TransferMessageToUser(msgPacket);
			cout << "Message from " << msgPacket->SenderName << " to " << msgPacket->ReceiverName << " transfered!\n";
		}
		else
		{
			UsersData* user = FindUser(fromClient);
			for (ChannelData* channel : user->CurrentChannels)
			{
				if (channel->Name == msgPacket->ReceiverName)
				{
					for (UsersData* user : channel->Users)
					{
						if (user->UserConnectionData->socket != fromClient->socket)
							SendPacket(msgPacket, user->UserConnectionData);
					}
					return;
				}
			}
		}

		return;
	}
	else if (packet->Command == "REQ")
	{
		RequestPacket* reqPacket = dynamic_cast<RequestPacket*>(packet);

		OnRequestReceive(reqPacket, fromClient);
		//delete reqPacket;
	}
	else if (packet->Command == "LV")
	{
		LeaveChannelPacket* lvPacket = dynamic_cast<LeaveChannelPacket*>(packet);

		ChannelData* channel = FindChannel(lvPacket->ChannelName);
		if (channel != nullptr)
		{
			UsersData* user = FindUser(fromClient);
			LeaveChannel(channel, user);
		}
	}

}

//void OnClientDisconected(Client)

DWORD WINAPI ServerWorkerThread(LPVOID pCompletionPort)
{
	HANDLE hCompletionPort = (HANDLE)pCompletionPort;

	for (; ; )
	{
		DWORD bytesTransferred;
		ConnectionData *pConnectionData;
		IoOperationData *pIoData;
		if (GetQueuedCompletionStatus(hCompletionPort, &bytesTransferred,
			(PULONG_PTR)&pConnectionData, (LPOVERLAPPED *)&pIoData, INFINITE) == 0)
		{

			OnClientDisconected(pConnectionData);
			//printf("GetQueuedCompletionStatus() failed with error %d\n", GetLastError());
			return 0;
		}

		// Проверим, не было ли проблем с сокетом и не было ли закрыто соединение
		if (bytesTransferred == 0)
		{
			OnClientDisconected(pConnectionData);
			continue;
		}

		// Если bytesRecv равно 0, то мы начали принимать данные от клиента
		// с завершением вызова WSARecv()
		//if (pIoData->bytesRecv == 0) 
		//{
		//	pIoData->bytesRecv = bytesTransferred;
		//	pIoData->bytesSent = 0;
		//}
		//else 
		//{
		//	pIoData->bytesSent += bytesTransferred;
		//}

		//if (pIoData->bytesRecv > pIoData->bytesSent) 
		//{
		//	DWORD bytesSent;
		//	// Посылаем очередно запрос на ввод-вывод WSASend()
		//	// Так как WSASend() может отправить не все данные, то мы отправляем
		//	// оставшиеся данные из буфера пока не будут отправлены все
		//	ZeroMemory(&(pIoData->overlapped), sizeof(OVERLAPPED));
		//	pIoData->wsaBuf.buf = pIoData->buffer + pIoData->bytesSent;
		//	pIoData->wsaBuf.len = pIoData->bytesRecv - pIoData->bytesSent;
		//	if (WSASend(pConnectionData->socket, &(pIoData->wsaBuf), 1, &bytesSent, 0, &(pIoData->overlapped), NULL) == SOCKET_ERROR)
		//	{
		//		if (WSAGetLastError() != ERROR_IO_PENDING)
		//		{
		//			printf("WSASend failed with error %d\n", WSAGetLastError());
		//			return 0;
		//		}
		//	}
		//}
		//else 
		//{
		DWORD bytesRecv;
		pIoData->bytesRecv = 0;
		// Когда все данные отправлены, посылаем запрос ввода-вывода на чтение WSARecv()
		DWORD flags = 0;
		ZeroMemory(&(pIoData->overlapped), sizeof(OVERLAPPED));
		pIoData->wsaBuf.len = DEFAULT_BUFLEN;
		pIoData->wsaBuf.buf = pIoData->buffer;
		if (WSARecv(pConnectionData->socket, &(pIoData->wsaBuf), 1, &bytesRecv, &flags, &(pIoData->overlapped), NULL) == SOCKET_ERROR)
		{
			Packet* inPacket = EncoderDecoder::ParseString(pIoData->buffer);
			//printf(pIoData->buffer);
			OnPacketReceived(inPacket, pConnectionData);
			/*if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				printf("WSARecv failed with error %d\n", WSAGetLastError());
				return 0;
			}*/
		}

		//}
	}
}


