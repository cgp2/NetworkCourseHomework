#include "EncoderDecoder.h"

EncoderDecoder::EncoderDecoder()
{
}


EncoderDecoder::~EncoderDecoder()
{
}

string EncoderDecoder::BuildString(Packet * packet)
{		
	return packet->BuildString();;
}

Packet* EncoderDecoder::ParseString(const string str)
{
	string splitedString, command, params;
	//SplitString(str, '%', splitedString, nullptr);
	SplitString(str, '#', command, params);

	if (command == "ID")
	{
		IDPacket* packet = new IDPacket;
		packet->Command = "ID";
		SplitString(params, ' ', packet->Name, params );

		return packet;
	}
	else if (command == "CHID")
	{
		ChanelIDPacket* packet = new ChanelIDPacket;
		packet->Command = "CHID";
		SplitString(params, ' ', packet->Name, params);

		return packet;
	}
	else if (command == "RESP")
	{
		ResponcePacket* packet = new ResponcePacket;
		packet->Command = "RESP";
		string responceValue;
		SplitString(params, ' ', responceValue, params);

		if (responceValue == "1")
			packet->responce = true;
		else
			packet->responce = false;

		return packet;
	}
	else if (command == "MSG")
	{
		MSGPacket* packet = new MSGPacket;
		packet->Command = "MSG";
		SplitString(params, ' ', packet->SenderName, params);
		SplitString(params, ' ', packet->ReceiverName, params);
		SplitString(params, ' ', packet->Message, params);

		return packet;
	}
	else if (command == "REQ")
	{
		RequestPacket* packet = new RequestPacket;
		packet->Command = "REQ";
		SplitString(params, ' ', packet->requestFor, params);

		return packet;
	}
	else if(command == "LV")
	{
		LeaveChannelPacket* packet = new LeaveChannelPacket;
		packet->Command = "LV";
		SplitString(params, ' ', packet->ChannelName, params);
		return packet;
	}

	return nullptr;
}

void EncoderDecoder::SplitString(const string inString, const char separator, string & left, string & right)
{
	for (int i = 0; i < inString.size(); i++)
	{
		if (inString[i] == separator)
		{
			left = inString.substr(0, i);
			right = inString.substr(i+1, inString.size() - i);
			return;
		}
	}
}


