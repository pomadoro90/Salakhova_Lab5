#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <boost/asio.hpp>
#include <mutex>
#include "../lab1_console/Salakhova_Interfaces.h"
#include "../lab1_console/Salakhova_Message.h"

class __declspec(dllexport) SocketTransport : public ITransport
{
    boost::asio::ip::tcp::socket& sock;
    std::mutex& writeMx;

public:
    SocketTransport(boost::asio::ip::tcp::socket& s, std::mutex& mx);

    virtual void send(Message& m) const override;
    virtual void receive(Message& m) const override;
};
