#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <map>
#include <mutex>
#include <vector>
#include <chrono>
#include <set>

#include "Salakhova_ThreadManager.h"
#include "Salakhova_SysProgh.h"
#include "Salakhova_Session.h"
#include "Salakhova_SocketTransport.h"

using namespace boost::asio;
using boost::asio::ip::tcp;
using namespace std;

// Специальные адреса
constexpr int ADDR_BROADCAST = -1;
constexpr int ADDR_SERVER = -2;

// Настройки таймаутов
constexpr int SERVER_TIMEOUT_CHECK_SEC = 5;
constexpr int SERVER_TIMEOUT_SECONDS = 30;

// Глобальный счетчик для выдачи ID клиентам
int nextClientId = 1;
set<int> freeIds;

void BroadcastClientList()
{
    lock_guard<mutex> lg(SRLocal::mx);
    wstring list;
    for (auto& pair : SRLocal::sessions_map)
    {
        if (!list.empty()) list += L';';
        list += to_wstring(pair.first) + L':' + pair.second->clientName;
    }
    for (auto& pair : SRLocal::sessions_map)
    {
        Message bcast(ADDR_SERVER, MT_CONFIRM, list);
        pair.second->addMessage(bcast);
    }
    SafeWrite("BroadcastClientList: sent to", SRLocal::sessions_map.size(), "clients");
}

void ClientWorker(shared_ptr<tcp::socket> sock)
{
    int clientId;
    shared_ptr<mutex> writeMx = make_shared<mutex>();
    wstring clientName = L"Client";

    {
        lock_guard<mutex> lg(SRLocal::mx);
        if (!freeIds.empty()) {
            auto it = freeIds.begin();
            clientId = *it;
            freeIds.erase(it);
        } else {
            clientId = nextClientId++;
        }
        Session* s = new Session(clientId, clientName);
        s->setSocket(sock);
        SRLocal::sessions_map[clientId] = s;
    }

    SafeWrite("Client", clientId, "connected.");

    // Ставим в очередь сообщение с ID клиента (отправится при следующем GETDATA)
    {
        Message idMsg(ADDR_SERVER, MT_CONFIRM, to_wstring(clientId));
        lock_guard<mutex> lg(SRLocal::mx);
        if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
            SRLocal::sessions_map[clientId]->addMessage(idMsg);
    }
    SafeWrite("Client", clientId, "assigned ID, queued MT_CONFIRM");

    try
    {
        SocketTransport tr(*sock, *writeMx);

        // Рассылаем всем новый список клиентов
        BroadcastClientList();

        while (true)
        {
            Message m;
            m.receive(tr); // Ожидаем сообщение от клиента

            if (m.header.messageType == MT_GETDATA)
            {
                lock_guard<mutex> lg(SRLocal::mx);
                if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
                {
                    SRLocal::sessions_map[clientId]->updateActivity();

                    Message dataMsg;
                    if (SRLocal::sessions_map[clientId]->getMessage(dataMsg))
                    {
                        // Есть сообщение — отправляем его клиенту
                        dataMsg.send(tr);
                        SafeWrite("Server -> Client", clientId, "sent MT_DATA, from", dataMsg.header.from);
                    }
                    else
                    {
                        // Нет сообщений — отправляем MT_NODATA
                        Message noData(ADDR_SERVER, MT_NODATA, L"");
                        noData.send(tr);
                    }
                }
            }
            else if (m.header.messageType == MT_INFO)
            {
                lock_guard<mutex> lg(SRLocal::mx);
                if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
                    SRLocal::sessions_map[clientId]->updateActivity();
            }
            else if (m.header.messageType == MT_DATA)
            {
                if (m.header.to == ADDR_BROADCAST) // Всем (Broadcast)
                {
                    SafeWrite("Client", clientId, "sent broadcast.");
                    lock_guard<mutex> lg(SRLocal::mx);
                    for (auto& pair : SRLocal::sessions_map)
                    {
                        if (pair.first != clientId) // Не отправляем самому себе
                        {
                            Message copy = m;
                            copy.header.from = clientId; // Обязательно указываем отправителя
                            pair.second->addMessage(copy);
                        }
                    }
                }
                else // Конкретному адресату
                {
                    SafeWrite("Client", clientId, "sent message to", m.header.to);
                    lock_guard<mutex> lg(SRLocal::mx);
                    if (SRLocal::sessions_map.find(m.header.to) != SRLocal::sessions_map.end())
                    {
                        Message copy = m;
                        copy.header.from = clientId; // Обязательно указываем отправителя
                        SRLocal::sessions_map[m.header.to]->addMessage(copy);
                    }
                }
            }
            else if (m.header.messageType == MT_QUIT)
            {
                break; // Клиент сам захотел отключиться
            }
        }
    }
    catch (const boost::system::system_error& e) {
        SafeWrite("Client", clientId, "connection error:", e.what());
    }
    catch (const std::exception& e) {
        SafeWrite("Client", clientId, "error:", e.what());
    }
    catch (...) {
        SafeWrite("Client", clientId, "unknown error.");
    }

    SafeWrite("Client", clientId, "disconnected.");
    {
        lock_guard<mutex> lg(SRLocal::mx);
        if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
        {
            SRLocal::sessions_map[clientId]->closeSocket();
            delete SRLocal::sessions_map[clientId];
            SRLocal::sessions_map.erase(clientId);
            freeIds.insert(clientId);
        }
    }

    // Рассылаем остальным измененный список
    BroadcastClientList();
}

void TimeoutMonitor()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(SERVER_TIMEOUT_CHECK_SEC)); // Проверяем каждые 5 секунд

        vector<int> timedOutClients;
        {
            lock_guard<mutex> lg(SRLocal::mx);
            for (auto& pair : SRLocal::sessions_map)
            {
                // Если клиент не пинговал 30 секунд — он "отвалился"
                if (pair.second->isTimedOut(SERVER_TIMEOUT_SECONDS))
                {
                    timedOutClients.push_back(pair.first);
                }
            }
        }

        if (!timedOutClients.empty())
        {
            for (int id : timedOutClients)
            {
                SafeWrite("Timeout alert: Client", id, "disconnected due to inactivity.");

                lock_guard<mutex> lg(SRLocal::mx);
                if (SRLocal::sessions_map.find(id) != SRLocal::sessions_map.end())
                {
                    SRLocal::sessions_map[id]->closeSocket();
                    Message closeMsg(ADDR_SERVER, MT_CLOSE, L"");
                    SRLocal::sessions_map[id]->addMessage(closeMsg);
                    delete SRLocal::sessions_map[id];
                    SRLocal::sessions_map.erase(id);
                    freeIds.insert(id);
                }
            }

            // Оповещаем живых клиентов, что кто-то отвалился по таймауту
            BroadcastClientList();
        }
    }
}

// Блокируем закрытие консоли — чтобы потоки клиентов успели завершиться
BOOL WINAPI ConsoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT ||
        signal == CTRL_CLOSE_EVENT || signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT)
    {
        SafeWrite("Server is shutting down. Press Ctrl+C again or close window to force.");
        // Возвращаем TRUE чтобы заблокировать дефолтное закрытие
        // Повторный Ctrl+C всё равно убьёт процесс
        return TRUE;
    }
    return FALSE;
}

int main()
{
    setlocale(LC_ALL, "Russian");
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    SafeWrite("Message Broker Server started. Port: 12345");

    // Запускаем фоновый поток для проверки таймаутов
    thread(TimeoutMonitor).detach();

    try
    {
        io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 12345));

        while (true)
        {
            auto sock = make_shared<tcp::socket>(io);
            acceptor.accept(*sock);

            // Запускаем независимый обработчик для каждого нового клиента
            thread(ClientWorker, sock).detach();
        }
    }
    catch (exception& e)
    {
        cerr << "Server error: " << e.what() << endl;
    }

    return 0;
}