#include <string>
#include "Packet.h"
#pragma once

using namespace std;
class EncoderDecoder
{
public:
	EncoderDecoder();
	~EncoderDecoder();

	static string BuildString(Packet* packet);
	static Packet* ParseString(const string str);
	static void SplitString(const string inString, const char separator, string &left, string &right);
};


