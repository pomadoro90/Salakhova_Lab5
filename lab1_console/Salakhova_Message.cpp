#include "Salakhova_Message.h"

Message::Message(MessageTypes messageType, const wstring& data)
    : data(data)
{
    header = { messageType,  int(data.length() * sizeof(wchar_t)) };
}

Message::Message(int to, MessageTypes messageType, const wstring& data)
    : data(data)
{
    header = { messageType,  int(data.length() * sizeof(wchar_t)), to };
}

void Message::send(const ITransport& ITransport)
{
    ITransport.send(*this);
}

void Message::receive(const ITransport& ITransport)
{
    ITransport.receive(*this);
}

void Message::sendMessage(const ITransport& ITransport, int to, MessageTypes messageType, const wstring& data)
{
    Message m(to, messageType, data);
    m.send(ITransport);
}

Message Message::receiveMessage(const ITransport& ITransport)
{
    Message m;
    m.receive(ITransport);
    return m;
}