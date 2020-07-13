#pragma once

#include <Windows.h>
#include <strsafe.h>

template<typename... Args>
void dbwprintf_s(const WCHAR *format, Args ... args) {
	WCHAR buf[1024] = {};
	int count = swprintf_s(buf, format, args...);
	buf[count] = L'\r';
	buf[count + 1] = L'\n';
	buf[count + 2] = L'\0';
	OutputDebugStringW(buf);
}