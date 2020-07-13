#include "pch.h"
#include <strsafe.h>
#include <io.h>
#include <fcntl.h>
#include <windowsx.h>
#include "resource.h"
#include "notifyIcon.h"

HANDLE g_stdout;
#define Log(text) { \
	WriteConsole(g_stdout, text, ARRAYSIZE(text) - 1, NULL, NULL); \
}
#define LogLine(text) { \
	WriteConsole(g_stdout, text, ARRAYSIZE(text) - 1, NULL, NULL); \
	WriteConsole(g_stdout, TEXT("\n"), ARRAYSIZE(TEXT("\n")) - 1, NULL, NULL); \
}

HINSTANCE g_hInst;
UINT g_uiHookMsg, g_uiUnhookMsg, g_uiPosMsg;
HINSTANCE g_hDll{};
HOOKPROC g_hHookProc{};
DWORD g_dwHookThread{};
HWND g_hwndTaskbar{};
HWND hwndTray = NULL, hwndBar = NULL;
LONG g_placementLeft = -1;
CTrayIcon g_trayIcon;

BOOL CALLBACK FindTragetsAndLeftBound(HWND hwnd, LPARAM lParam)
{
	TCHAR lpClassName[MAX_CLASS_NAME];
	GetClassName(hwnd, lpClassName, MAX_CLASS_NAME);

	RECT rect;
	if (CompareStringOrdinal(lpClassName, -1, TEXT("TrayNotifyWnd"), -1, false) == CSTR_EQUAL)
	{
		hwndTray = hwnd;
	}
	else if (CompareStringOrdinal(lpClassName, -1, TEXT("ReBarWindow32"), -1, false) == CSTR_EQUAL)
	{
		hwndBar = hwnd;
	}
	else if (GetParent(hwnd) == g_hwndTaskbar && IsWindowVisible(hwnd) && GetWindowRect(hwnd, &rect)) {
		if (rect.right > g_placementLeft)
			g_placementLeft = rect.right;
	}

	return true;
}

bool GetProperClientRect(HWND hWnd, HWND hWndParent, LPRECT prc)
{
	if (!GetWindowRect(hWnd,prc))
		return false;
	if (hWndParent == HWND_DESKTOP)
		return true; // Success (no-op)

	// MapWindowPoints returns 0 on failure but 0 is also a valid result (window whose client area is at 0,0), so use SetLastError/GetLastError to clarify.
	DWORD dwErrorPreserve = ::GetLastError();
	::SetLastError(ERROR_SUCCESS);
	if (::MapWindowPoints(HWND_DESKTOP, hWndParent, reinterpret_cast<LPPOINT>(prc), 2) == 0)
	{
		if (::GetLastError() != ERROR_SUCCESS)
			return false;
		::SetLastError(dwErrorPreserve);
	}

	return true; // Success
}

bool FixClockPosition(HWND hwndTaskbar, bool fRestore)
{
	LogLine(TEXT("Repositioning..."));
	g_placementLeft = 0;
	EnumChildWindows(hwndTaskbar, FindTragetsAndLeftBound, NULL);
	dbwprintf_s(L"Left position is %d", g_placementLeft);

	if (hwndTray != NULL && hwndBar != NULL && g_placementLeft != -1)
	{
		RECT rcBar, rcTray;
		if (GetProperClientRect(hwndBar, hwndTaskbar, &rcBar)
		&&	GetProperClientRect(hwndTray, hwndTaskbar, &rcTray))
		{
			APPBARDATA abd{sizeof(APPBARDATA)};
			if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd))
			{
				if ((abd.rc.right - abd.rc.left) > (abd.rc.bottom - abd.rc.top))
				{
					// horizontal
					int trayWidth = rcTray.right - rcTray.left;
					int barWidth = rcBar.right - rcBar.left;
					if (fRestore)
					{
						LogLine(TEXT("Restoring tray to the right..."))
						SetWindowPos(hwndBar, HWND_NOTOPMOST, g_placementLeft, rcBar.top, 0, 0,
							SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
						SetWindowPos(hwndTray, HWND_NOTOPMOST, g_placementLeft + barWidth, rcTray.top, 0, 0,
							SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
					}
					else
					{
						LogLine(TEXT("Moving tray to the left..."))
						if (rcBar.left != g_placementLeft + trayWidth)
							SetWindowPos(hwndBar, HWND_NOTOPMOST, g_placementLeft + trayWidth, rcBar.top, 0, 0,
								SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
						if (rcTray.left != g_placementLeft)
							SetWindowPos(hwndTray, HWND_NOTOPMOST, g_placementLeft, rcTray.top, 0, 0,
								SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
					}
				}

				return true;
			}
		}
	}
	return false;
}

bool InstallHook()
{
	LogLine(TEXT("Installing Hook..."))
	if (!FixClockPosition(g_hwndTaskbar, false))
		return false;
	DWORD dwThread = GetWindowThreadProcessId(g_hwndTaskbar, nullptr);
	if (HHOOK hHook = SetWindowsHookEx(WH_CALLWNDPROC, g_hHookProc, g_hDll, dwThread))
	{
		LogLine(TEXT("Injecting subclass..."))
		SendMessage(g_hwndTaskbar, g_uiHookMsg, 0, 0);
		UnhookWindowsHookEx(hHook);
		g_dwHookThread = dwThread;
		return true;
	}

	g_dwHookThread = 0;
	return false;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//dbwprintf_s(L"message %d(%#x) received", uMsg, uMsg);
	switch (uMsg)
	{
	case WM_CREATE:
		g_trayIcon = CTrayIcon::InitializeFromResource(g_hInst, hWnd, IDI_NOTIFYICON);
		return 0;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		g_trayIcon.Uninitialize();
		PostQuitMessage(0);
		return 0;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_QUIT:
			DestroyWindow(hWnd);
			break;
		}

		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	default:
		if (uMsg == g_uiPosMsg)
		{
			dbwprintf_s(L"Taskbar window repositioned");
			EnumChildWindows(g_hwndTaskbar, FindTragetsAndLeftBound, NULL);
			dbwprintf_s(L"New Left position is %d", g_placementLeft);
		}
		else if (uMsg == g_trayIcon.TrayIconMessage())
		{
			WORD event = LOWORD(lParam);
			if (event == WM_CONTEXTMENU)
			{
				float x = GET_X_LPARAM(wParam);
				float y = GET_Y_LPARAM(wParam);

				// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenuex
				// To display a context menu for a notification icon, the current window must be the foreground window before the application calls TrackPopupMenu or TrackPopupMenuEx.
				// Otherwise, the menu will not disappear when the user clicks outside of the menu or the window that created the menu (if it is visible).
				// If the current window is a child window, you must set the (top-level) parent window as the foreground window.
				SetForegroundWindow(hWnd);

				HMENU hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_TRAY_MENU));
				HMENU subMenu = GetSubMenu(hMenu, 0);
				TrackPopupMenuEx(subMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON, x, y, hWnd, NULL);
				DestroyMenu(hMenu);
			}
		}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#define CLASSNAME L"LeftTray"
#define MSGHOOK	  L"LeftTray::Hook"
#define MSGUNHOOK L"LeftTray::Unhook"

typedef void (*HOOKINIT)(HWND, UINT, UINT, UINT);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
	if (!AttachConsole(ATTACH_PARENT_PROCESS))
#ifdef _DEBUG
		AllocConsole();
#else
		;
#endif // _DEBUG


	g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	LogLine(TEXT("Console attached"));

	if (HWND hwndExisting = FindWindow(CLASSNAME, nullptr))
	{
		LogLine(TEXT("Program running. Exiting..."))
		PostMessage(hwndExisting, WM_CLOSE, 0, 0);
		return 0;
	}

	WNDCLASS wc{};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = CLASSNAME;
	RegisterClass(&wc);

	if (!(g_hDll = LoadLibrary(L"Hooks.dll")))
	{
		LogLine(TEXT("Loading dll failed"));
		return 0;
	}

	HOOKINIT hHookInit = reinterpret_cast<HOOKINIT>( GetProcAddress(g_hDll, "HookInit") );
	g_hHookProc = reinterpret_cast<HOOKPROC>( GetProcAddress(g_hDll, "HookProc") );
	if (hHookInit && g_hHookProc)
	{
		LogLine(TEXT("Loading hooks succeeded"));

		if (HWND hwndMain = CreateWindowEx(0, CLASSNAME, nullptr, WS_POPUP, 0, 0, 0, 0, HWND_DESKTOP, nullptr, GetModuleHandle(nullptr), 0))
		{
			if (g_hwndTaskbar = FindWindow(L"Shell_TrayWnd", nullptr))
			{
				g_uiHookMsg = RegisterWindowMessage(MSGHOOK);
				g_uiUnhookMsg = RegisterWindowMessage(MSGUNHOOK);
				g_uiPosMsg = RegisterWindowMessage(TEXT("TrayShifterPosChanged"));
				hHookInit(hwndMain, g_uiHookMsg, g_uiUnhookMsg, g_uiPosMsg);

				bool bRslt = ChangeWindowMessageFilterEx(hwndMain, g_uiPosMsg, MSGFLT_ALLOW, NULL);

				InstallHook();

				dbwprintf_s(L"Taskbar reposition window running, hwnd=%#08x", hwndMain);
				MSG msg;
				while (GetMessage(&msg, nullptr, 0, 0) > 0)
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				if (g_dwHookThread)
				{
					if (HHOOK hHook = SetWindowsHookEx(WH_CALLWNDPROC, g_hHookProc, g_hDll, g_dwHookThread))
					{
						SendMessage(g_hwndTaskbar, g_uiUnhookMsg, 0, 0);
						UnhookWindowsHookEx(hHook);
					}
				}

				FixClockPosition(g_hwndTaskbar, true);
			}
		}
	}
	FreeLibrary(g_hDll);
	return 0;
}

