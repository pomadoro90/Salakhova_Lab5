#pragma once

#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <tchar.h>
#include <mutex>

inline void DoWrite()
{
	std::cout << std::endl;
}

template <class T, typename... Args> inline void DoWrite(T& value, Args... args)
{
	std::cout << value << " ";
	DoWrite(args...);
}

static std::mutex console_mx;
template<typename... Args> inline void SafeWrite(Args... args)
{
	std::lock_guard<std::mutex> lock(console_mx);
	DoWrite(args...);
}