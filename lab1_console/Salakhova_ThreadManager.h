#pragma once
#include <windows.h>
#include <map>
#include <mutex>
#include "Salakhova_Session.h"
#include "Salakhova_Interfaces.h"

using namespace std;

class SRLocal : public ITransport
{
public:
    int id;

    static map<int, Session*> sessions_map;
    static mutex mx;

    SRLocal(int id = -1);

    virtual void send(Message& m) const override;
    virtual void receive(Message& m) const override;
};

DWORD WINAPI MyThread(LPVOID lpParameter);