#pragma once

struct Message;

class ITransport
{
public:
    virtual void send(Message&) const = 0;
    virtual void receive(Message&) const = 0;
};