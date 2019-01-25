#include <string>
#include <vector>

#pragma once

using namespace std;
class Packet
{
public:
	string Command;

	virtual string BuildString()
	{
		return string();
	}
};

class IDPacket : public Packet
{
public:
	string Name;

	string BuildString()
	{
		return "ID#" + Name + " " + "%";
	}
};

class ChanelIDPacket: public Packet
{
public:
	string Name;

	string BuildString()
	{
		return "CHID#" + Name + " " + "%";
	}
};

class ResponcePacket : public Packet
{
public:
	bool responce;

	string BuildString()
	{
		if (responce)
			return "RESP#1 %";
		else
			return "RESP#0 %";
	}
};

class RequestPacket : public Packet
{
public:
	string requestFor;

	string BuildString()
	{
		return "REQ#" + requestFor + " %";
	}
};

class NamesPacket : public Packet
{
public:
	vector<string> Names;

	string BuildString()
	{
		string ret = "NAMES#";
		for (string name : Names)
		{
			ret.append(name + " ");
		}

		return ret + "%";
	}
};

class LeaveChannelPacket : public Packet
{
public:
	string ChannelName;

	string BuildString()
	{
		return "LV#" + ChannelName + " %";
	}
};

class MSGPacket : public Packet
{
public:
	string SenderName;
	string ReceiverName;
	string Message;

	string BuildString()
	{
		return "MSG#" + SenderName + " " + ReceiverName + " " + Message + " %";
	}
};