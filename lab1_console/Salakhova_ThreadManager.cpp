#include "Salakhova_ThreadManager.h"
#include "Salakhova_SysProgh.h"
#include <iostream>
#include <fstream>

// Инициализация статических переменных
map<int, Session*> SRLocal::sessions_map;
mutex SRLocal::mx;

SRLocal::SRLocal(int id) : id(id) {}

void SRLocal::send(Message& m) const
{
    lock_guard<mutex> lg(mx); // Защищаем доступ к словарю
    if (id < 0)
        sessions_map[m.header.to]->addMessage(m);
    else
        sessions_map[id]->addMessage(m);
}

void SRLocal::receive(Message& m) const
{
    Session* targetSession = nullptr;
    {
        lock_guard<mutex> lg(mx);
        targetSession = sessions_map[id];
    }

    if (targetSession != nullptr) {
        targetSession->getMessage(m);
    }
}

DWORD WINAPI MyThread(LPVOID lpParameter)
{
    auto session = static_cast<Session*>(lpParameter);
    int id = session->sessionID;

    SafeWrite("session", id, "is created.");

    while (true)
    {
        Message m = Message::receiveMessage(SRLocal(id));

        switch (m.header.messageType)
        {
        case MT_CLOSE:
        {
            SafeWrite("session", id, "is closed.");

            {
                lock_guard<mutex> lg(SRLocal::mx);
                SRLocal::sessions_map.erase(id);
            }

            delete session;
            return 0;
        }
        case MT_DATA:
        {
            wstring fileName = to_wstring(id) + L".txt";

            // ios::app - дозапись (append)
            wofstream fout(fileName, ios::app);

            fout.imbue(locale("ru_RU.UTF-8"));

            fout << m.data << endl;
            fout.close();

            SafeWrite("session", id, "wrote data to file.");
        }
        break;
        }
    }
    return 0;
}