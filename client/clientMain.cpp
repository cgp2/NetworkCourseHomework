#define WIN32_LEAN_AND_MEAN


#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "string"
#include <thread>
#include <iostream>
#include <vector>
#include <list>
#include <process.h>
#include <algorithm>
#include "EncoderDecoder.h"

#pragma comment (lib, "Ws2_32.lib")
//#pragma comment (lib, "Mswsock.lib")
//#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

using namespace std;

enum UserStates
{
	Idle,
	Auth,
	Request,
	ID,
	Send,
	Users,
	Chan,
	Leave,
	Join,
};

struct IoOperationData {
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	CHAR buffer[DEFAULT_BUFLEN];
	DWORD bytesSent;
	DWORD bytesRecv;
};

SOCKET clientSocket;
string UserName;
list<string> CurrentChannels;
UserStates CurrentState = Idle;

string requestedChannel;



void OnStateChanging(string commandName)
{
	switch (CurrentState)
	{
	case Idle:
	{
		CurrentState = Auth;
		break;
	}
	case Auth:
	{
		if (commandName == "MSG")
		{
			CurrentState = Send;
			break;
		}
		else if (commandName == "ID")
		{
			CurrentState = ID;
			break;
		}
		else if (commandName == "JOIN")
		{
			CurrentState = Join;
			break;
		}
		else if (commandName == "USERS")
		{
			CurrentState = Users;
			break;
		}
		else if (commandName == "LEAVE")
		{
			CurrentState = Leave;
			break;
		}
		else if (commandName == "CH")
		{
			CurrentState = Chan;
			break;
		}
	}
	case Chan:
	{
		if (commandName == "false")
		{
			cout << "Channel error\n";
		}
		else
		{
			CurrentChannels.push_back(requestedChannel);
		}

		requestedChannel = "";

		CurrentState = Auth;
		break;
	}
	default:
	{
		CurrentState = Auth;
		break;
	}
	}
	cout << "Current State: " << CurrentState << "\n";
}

void OnMessageReceived(MSGPacket* msgPacket)
{
	cout << msgPacket->SenderName << ": " << msgPacket->Message << "\n";
	delete msgPacket;
}

void OnPacketReceived(Packet* packet)
{
	if (packet->Command == "ID")
	{
		IDPacket* idPacket = dynamic_cast<IDPacket*>(packet);

		delete idPacket;
	}
	else if (packet->Command == "RESP")
	{
		ResponcePacket* respPacket = dynamic_cast<ResponcePacket*>(packet);

		if (respPacket->responce)
			OnStateChanging("true");
		else
			OnStateChanging("false");

		cout << "Server responce:" << respPacket->responce << "\n";
	}
	else if (packet->Command == "MSG")
	{
		MSGPacket* msgPacket = dynamic_cast<MSGPacket*>(packet);
		OnMessageReceived(msgPacket);

	}
	else if (packet->Command == "Users")
	{
		NamesPacket* namesPacket = dynamic_cast<NamesPacket*>(packet);
		cout << "*******" << "Active users" << "*******\n";
		for (string name : namesPacket->Names)
		{
			cout << name << "\n";
		}
		delete namesPacket;
	}
}

bool SendPacket(Packet* packet)
{
	string sendString = packet->BuildString();
	const char *sendbuf = sendString.data();
	int bytesSent = send(clientSocket, sendbuf, (int)strlen(sendbuf), 0);
	if (bytesSent == SOCKET_ERROR)
	{
		printf("send failed with error: %d\n", WSAGetLastError());
		return false;
	}

	//	printf("Bytes Sent: %ld\n", bytesSent);
	return true;
}

void ParseUserCommand(string userCommand)
{
	string command;
	if (userCommand[userCommand.length()] != ' ')
		userCommand += ' ';

	string paramsString;
	EncoderDecoder::SplitString(userCommand, ' ', command, paramsString);
	for (auto & c : command) c = toupper(c);

	if (CurrentState == Auth)
	{
		if (command == "MSG")
		{
			MSGPacket* msgPacket = new MSGPacket;
			msgPacket->SenderName = UserName;
			EncoderDecoder::SplitString(paramsString, ' ', msgPacket->ReceiverName, paramsString);
			msgPacket->Message = paramsString;

			SendPacket(msgPacket);
		}
		if (command == "USERS")
		{
			RequestPacket* reqPacket = new RequestPacket;
			reqPacket->requestFor = "Users";
			SendPacket(reqPacket);
		}
		if (command == "CHANNELS")
		{
			cout << "*******" << "User channels" << "*******\n";
			for (string name : CurrentChannels)
			{
				cout << name << "\n";
			}
		}
		if (command == "CH")
		{
			ChanelIDPacket* chidPacket = new ChanelIDPacket;
			EncoderDecoder::SplitString(paramsString, ' ', chidPacket->Name, paramsString);

			requestedChannel = chidPacket->Name;
			OnStateChanging(command);

			SendPacket(chidPacket);
		}
		if (command == "LEAVE")
		{
			LeaveChannelPacket* lvPacket = new LeaveChannelPacket;
			EncoderDecoder::SplitString(paramsString, ' ', lvPacket->ChannelName, paramsString);

			OnStateChanging(command);

			SendPacket(lvPacket);
		}
	}
}

void ListeningThread()
{
	for (;;)
	{
		int bytesReceived = 0;
		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;
		do
		{
			bytesReceived = recv(clientSocket, recvbuf, recvbuflen, 0);
			if (bytesReceived > 0)
			{
				//	printf("Bytes received: %d\n", bytesReceived);
				Packet* inPacket = EncoderDecoder::ParseString(recvbuf);
				OnPacketReceived(inPacket);
			}
			else if (bytesReceived == 0)
			{
				printf("Connection closed\n");
			}
			else
			{
				printf("recv failed with error: %d\n", WSAGetLastError());
				return;
			}

		} while (bytesReceived > 0);
		this_thread::sleep_for(0.1s);
	}
}

int __cdecl main(int argc, char *argv[])
{
	// Проверка параметров командной строки
	//if (argc != 2) {
	//	printf("usage: %s server-name\n", argv[0]);
	//	return EXIT_FAILURE;
	//}
	/*HANDLE thread = CreateThread(
		NULL,
		0,
		);*/
	int error;

	// Запускаем Winsock
	WSADATA wsaData;
	error = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (error != 0)
	{
		printf("WSAStartup failed with error: %d\n", error);
		return EXIT_FAILURE;
	}

	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Определяем адрес порт сервера
	struct addrinfo *serverAddr;
	error = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &serverAddr);

	if (error != 0)
	{
		printf("getaddrinfo failed with error: %d\n", error);
		WSACleanup();
		return EXIT_FAILURE;
	}

	// Пытаемся подключиться к серверу по одному из опреденных адресов

	for (struct addrinfo *ptr = serverAddr; ptr != NULL; ptr = ptr->ai_next)
	{
		// Пытаемся создать SOCKET для подключения
		clientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (clientSocket == INVALID_SOCKET)
		{
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return EXIT_FAILURE;
		}

		// Осуществлем подключение
		error = connect(clientSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (error == SOCKET_ERROR)
		{
			closesocket(clientSocket);
			clientSocket = INVALID_SOCKET;
			continue;
		}
		cout << "Connected to server!\n";

		break;
	}

	freeaddrinfo(serverAddr);

	if (clientSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return EXIT_FAILURE;
	}

	cout << "Please, insert your name:\n";
	cin >> UserName;

	// Отправляем пакет
	IDPacket* idPacket = new IDPacket;
	idPacket->Name = UserName;
	/*string sendString = idPacket.BuildString();
	const char *sendbuf = sendString.data();
	int bytesSent = send(clientSocket, sendbuf, (int)strlen(sendbuf), 0);
	if (bytesSent == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}*/
	SendPacket(idPacket);

	thread t1(ListeningThread);

	for (;;)
	{
		//cout << "Print your command \n";
		char input[100];
		cin.getline(input, sizeof(input));
		string command(input);
		ParseUserCommand(command);
	}

	/*char c = 'a';
	cin >> c;*/

	//// Закрываем отправляющую часть соединения, так как больше не собираемся отправлять данные
	//error = shutdown(clientSocket, SD_SEND);
	//if (error == SOCKET_ERROR) {
	//	printf("shutdown failed with error: %d\n", WSAGetLastError());
	//	closesocket(clientSocket);
	//	WSACleanup();
	//	return EXIT_FAILURE;
	//}

	// Пытаемся получить данные, пока сервер не закроет соединение
	//int bytesReceived = 0;
	//char recvbuf[DEFAULT_BUFLEN];
	//int recvbuflen = DEFAULT_BUFLEN;
	//do 
	//{
	//	bytesReceived = recv(clientSocket, recvbuf, recvbuflen, 0);
	//	if (bytesReceived > 0)
	//		printf("Bytes received: %d\n", bytesReceived);
	//	else if (bytesReceived == 0)
	//		printf("Connection closed\n");
	//	else
	//		printf("recv failed with error: %d\n", WSAGetLastError());
	//} while (bytesReceived > 0);

	//// Очистка
	//closesocket(clientSocket);
	//WSACleanup();

	t1.join();
	return EXIT_SUCCESS;
}



