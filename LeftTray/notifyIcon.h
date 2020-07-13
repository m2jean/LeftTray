#pragma once

#include <Windows.h>
#include <shellapi.h>

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "dbprintf.h"

struct CTrayIcon {
public:
	UINT TrayIconMessage() { return m_iconMsg; }

	void Initialize(const HWND hWnd, const HICON& icon) {
		if (m_added)
			return;

		m_hWnd = hWnd;
		m_iconMsg = RegisterWindowMessage(MSG_TRAYICON);

		NOTIFYICONDATA notifyIconData = {};
		notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
		notifyIconData.hWnd = hWnd;
		notifyIconData.uID = 0;
		notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE;
		notifyIconData.hIcon = icon;
		notifyIconData.uCallbackMessage = m_iconMsg;

		bool result = Shell_NotifyIcon(NIM_ADD, &notifyIconData);
		if (!result)
			dbwprintf_s(L"Adding tray icon failed.");

		notifyIconData.uVersion = NOTIFYICON_VERSION_4;
		result = Shell_NotifyIcon(NIM_SETVERSION, &notifyIconData);

		m_added = true;
	}

	void Uninitialize() {
		if (!m_added)
			return;

		NOTIFYICONDATA notifyIconData = {};
		notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
		notifyIconData.hWnd = m_hWnd;
		notifyIconData.uID = 0;
		notifyIconData.guidItem = GUID();

		bool result = Shell_NotifyIcon(NIM_DELETE, &notifyIconData);

		m_added = false;
	}

	static CTrayIcon InitializeFromResource(HINSTANCE hInst, HWND hWnd, WORD resourceId) {
		HICON hIcon;
		LoadIconMetric(hInst, MAKEINTRESOURCE(resourceId), LIM_SMALL, &hIcon);
		CTrayIcon trayIcon;
		trayIcon.Initialize(hWnd, hIcon);
		return trayIcon;
	}

private:
	const WCHAR* MSG_TRAYICON = TEXT("LeftTray::TrayIconMsg");

	bool m_added = false;
	HWND m_hWnd;
	UINT m_iconMsg = 0;
};
