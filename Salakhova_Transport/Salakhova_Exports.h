#pragma once
#include <wchar.h>

extern "C"
{
    __declspec(dllexport) bool Salakhova_Connect(const wchar_t* host, int port);
    __declspec(dllexport) void Salakhova_Disconnect();
    __declspec(dllexport) bool Salakhova_IsConnected();
    __declspec(dllexport) int  Salakhova_GetClientId();

    __declspec(dllexport) void Salakhova_Send(int target, int command, const wchar_t* text);

    __declspec(dllexport) bool Salakhova_Poll(int* outCommand, int* outTarget, int* outSource, wchar_t* outText, int outCapacity);
}
