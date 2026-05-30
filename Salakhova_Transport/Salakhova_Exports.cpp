#include "pch.h"
#include "Salakhova_Exports.h"
#include "Salakhova_SocketTransport.h"

#include <windows.h>
#include <boost/asio.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <memory>
#include <string>
#include <chrono>
#include <cstdlib>

namespace
{
    using boost::asio::ip::tcp;

    boost::asio::io_context        g_io;
    std::shared_ptr<tcp::socket>   g_sock;
    std::mutex                     g_writeMx;
    std::mutex                     g_qMx;         // mutex for inbox queue
    std::queue<Message>            g_inbox;       // incoming message queue
    std::thread                    g_reader;      // reader thread
    std::thread                    g_poller;      // poller thread (GETDATA every 500ms)
    std::thread                    g_pinger;      // ping thread (INFO every 10s)
    std::atomic<bool>              g_alive{ false };
    std::atomic<int>               g_clientId{ -1 };

    const int ADDR_BROADCAST = -1;
    const int ADDR_SERVER = -2;

    // Helper: send a message via SocketTransport
    void sendMsg(int to, MessageTypes type, const std::wstring& data = L"")
    {
        if (!g_sock) return;
        Message m(to, type, data);
        if (g_clientId.load() > 0)
            m.header.from = g_clientId.load();
        SocketTransport tr(*g_sock, g_writeMx);
        m.send(tr);
    }

    // Reader thread: receives messages from the socket
    void readerLoop()
    {
        try
        {
            SocketTransport tr(*g_sock, g_writeMx);
            while (g_alive.load())
            {
                Message m;
                m.receive(tr);

                // Skip MT_NODATA (no data available)
                if (m.header.messageType == MT_NODATA)
                    continue;

                // Handle MT_CLOSE - server closed connection
                if (m.header.messageType == MT_CLOSE)
                {
                    g_alive.store(false);
                    continue;
                }

                // Handle MT_CONFIRM - extract client ID on first receipt
                if (m.header.messageType == MT_CONFIRM && !m.data.empty())
                {
                    try
                    {
                        int id = std::stoi(m.data);
                        if (id > 0 && g_clientId.load() == -1)
                        {
                            g_clientId.store(id);
                            // Do NOT queue the CONFIRM that sets our ID
                            continue;
                        }
                    }
                    catch (...)
                    {
                        // Non-numeric CONFIRM payload - queue it normally
                    }
                }

                // All other messages go to inbox
                {
                    std::lock_guard<std::mutex> lg(g_qMx);
                    g_inbox.push(std::move(m));
                }
            }
        }
        catch (...)
        {
            g_alive.store(false);
        }
    }

    // Poller thread: sends MT_GETDATA every 500ms (pull model)
    void pollerLoop()
    {
        try
        {
            while (g_alive.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (!g_alive.load()) break;
                sendMsg(ADDR_SERVER, MT_GETDATA);
            }
        }
        catch (...)
        {
            g_alive.store(false);
        }
    }

    // Ping thread: sends MT_INFO every 10s
    void pingLoop()
    {
        try
        {
            while (g_alive.load())
            {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                if (!g_alive.load()) break;
                sendMsg(ADDR_SERVER, MT_INFO);
            }
        }
        catch (...)
        {
            g_alive.store(false);
        }
    }
}

extern "C"
{
    __declspec(dllexport) bool Salakhova_Connect(const wchar_t* host, int port)
    {
        if (g_alive.load()) return true;

        try
        {
            std::string h;
            if (host)
            {
                while (*host) h.push_back(static_cast<char>(*host++));
            }
            else
            {
                h = "127.0.0.1";
            }

            // TCP connect
            g_sock = std::make_shared<tcp::socket>(g_io);
            tcp::resolver r(g_io);
            boost::asio::connect(*g_sock, r.resolve(h, std::to_string(port)));

            g_clientId.store(-1);
            g_alive.store(true);

            // Send MT_INIT to start handshake
            Message initMsg(ADDR_SERVER, MT_INIT);
            {
                SocketTransport tr(*g_sock, g_writeMx);
                initMsg.send(tr);
            }

            // Start reader thread (will receive CONFIRM with our ID)
            g_reader = std::thread(readerLoop);

            // Poll loop: send GETDATA until CONFIRM with numeric ID received (5s timeout)
            auto startTime = std::chrono::steady_clock::now();
            const std::chrono::seconds timeout(5);
            bool gotId = false;

            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (g_clientId.load() > 0)
                {
                    gotId = true;
                    break;
                }
                sendMsg(ADDR_SERVER, MT_GETDATA);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (!gotId)
            {
                // Timeout - cleanup
                g_alive.store(false);
                if (g_sock)
                {
                    boost::system::error_code ec;
                    g_sock->shutdown(tcp::socket::shutdown_both, ec);
                    g_sock->close(ec);
                }
                if (g_reader.joinable()) g_reader.join();
                g_sock.reset();
                return false;
            }

            // Start poller and ping threads
            g_poller = std::thread(pollerLoop);
            g_pinger = std::thread(pingLoop);

            return true;
        }
        catch (...)
        {
            g_sock.reset();
            g_alive.store(false);
            g_clientId.store(-1);
            return false;
        }
    }

    __declspec(dllexport) void Salakhova_Disconnect()
    {
        g_alive.store(false);

        // Send MT_QUIT to signal disconnection
        if (g_sock)
        {
            try
            {
                sendMsg(ADDR_SERVER, MT_QUIT);
            }
            catch (...) {}
        }

        if (g_sock)
        {
            boost::system::error_code ec;
            g_sock->shutdown(tcp::socket::shutdown_both, ec);
            g_sock->close(ec);
        }

        if (g_poller.joinable()) g_poller.join();
        if (g_pinger.joinable()) g_pinger.join();
        if (g_reader.joinable()) g_reader.join();

        g_sock.reset();
        g_clientId.store(-1);

        std::lock_guard<std::mutex> lg(g_qMx);
        std::queue<Message> empty;
        g_inbox.swap(empty); // clear queue
    }

    __declspec(dllexport) bool Salakhova_IsConnected()
    {
        return g_alive.load();
    }

    __declspec(dllexport) int Salakhova_GetClientId()
    {
        return g_clientId.load();
    }

    __declspec(dllexport) void Salakhova_Send(int target, int command, const wchar_t* text)
    {
        if (!g_alive.load() || !g_sock) return;

        try
        {
            std::wstring str = text ? text : L"";
            Message m(target, static_cast<MessageTypes>(command), str);
            if (g_clientId.load() > 0)
                m.header.from = g_clientId.load();

            SocketTransport tr(*g_sock, g_writeMx);
            m.send(tr);
        }
        catch (...)
        {
            g_alive.store(false);
        }
    }

    __declspec(dllexport) bool Salakhova_Poll(int* outCommand, int* outTarget, int* outSource, wchar_t* outText, int outCapacity)
    {
        std::lock_guard<std::mutex> lg(g_qMx);
        if (g_inbox.empty()) return false;

        Message m = std::move(g_inbox.front());
        g_inbox.pop();

        if (outCommand) *outCommand = m.header.messageType;
        if (outTarget)  *outTarget = m.header.to;
        if (outSource)  *outSource = m.header.from; // sender ID

        if (outText && outCapacity > 0)
        {
            int n = static_cast<int>(m.data.size());
            if (n >= outCapacity) n = outCapacity - 1;
            for (int i = 0; i < n; ++i) outText[i] = m.data[i];
            outText[n] = L'\0';
        }
        return true;
    }
}
