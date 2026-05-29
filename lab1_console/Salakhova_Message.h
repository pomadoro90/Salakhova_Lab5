#pragma once
#include <string>
#include "Salakhova_Interfaces.h"

using namespace std;

enum MessageTypes
{
    MT_INIT = 0, 
    MT_EXIT = 1,  
    MT_GETDATA = 2, 
    MT_DATA = 3,   
    MT_NODATA = 4, 
    MT_INFO = 5,
    MT_CONFIRM = 6,
    MT_CLOSE = 7,
    MT_QUIT = 8
};

struct MessageHeader
{
    int messageType;
    int size;
    int to;
    int from;
};

struct Message
{
    MessageHeader header = { 0 };
    wstring data;

    Message() = default;
    Message(MessageTypes messageType, const wstring& data = L"");
    Message(int to, MessageTypes messageType, const wstring& data = L"");

    void send(const ITransport& transport);
    void receive(const ITransport& transport);

    static void sendMessage(const ITransport& transport, int to, MessageTypes messageType, const wstring& data = L"");
    static Message receiveMessage(const ITransport& transport);
};