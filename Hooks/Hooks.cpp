#include "stdafx.h"
#include "..\LeftTray\dbprintf.h"

#pragma data_seg(".shared")
HWND g_controllerHwnd{};
UINT g_uiHookMsg{}, g_uiUnhookMsg{}, g_uiPosMsg{};
#pragma data_seg()
#pragma comment(linker, "/section:.shared,rws")

HINSTANCE g_hInstanceDll{};
HWND g_hwndTaskbar{}, g_hwndBar{}, g_hwndTray{};
LONG g_placementLeft = 0, g_trayWidth = 0;

enum { IdBar, IdTray };

BOOL CALLBACK FindLeftBound(HWND hwnd, LPARAM lParam)
{
	RECT rect;
	if (hwnd != g_hwndBar && hwnd != g_hwndTray&& IsWindowVisible(hwnd) && GetParent(hwnd) == g_hwndTaskbar && GetWindowRect(hwnd, &rect)) {
		if (rect.right > g_placementLeft)
			g_placementLeft = rect.right;
	}

	return true;
}

void RecalculatePosition() {
	g_placementLeft = 0;
	EnumChildWindows(g_hwndTaskbar, FindLeftBound, NULL);

	RECT rect;
	GetWindowRect(g_hwndTray, &rect);
	g_trayWidth = rect.right - rect.left;
}

LRESULT CALLBACK SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (uMsg == WM_WINDOWPOSCHANGING)
	{
		LPWINDOWPOS pwp = reinterpret_cast<LPWINDOWPOS>(lParam);
		if (pwp && !(pwp->flags & SWP_NOMOVE))
		{
			RecalculatePosition();

			if (uIdSubclass == IdBar)
			{
				pwp->x = g_placementLeft + g_trayWidth;
			}
			else if (uIdSubclass == IdTray)
			{
				pwp->x = g_placementLeft;
			}
		}
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void InstallSubClass(HWND hwndTaskbar)
{
	dbwprintf_s(L"Installing subclass to taskbar and tray");
	// increment refcount to keep DLL in explorer
	if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(InstallSubClass), &g_hInstanceDll))
		return;
	if (HWND hwndTray = FindWindowEx(hwndTaskbar, nullptr, L"TrayNotifyWnd", nullptr))
	{
		if (HWND hwndBar = FindWindowEx(hwndTaskbar, nullptr, L"ReBarWindow32", nullptr))
		{
			g_hwndTaskbar = hwndTaskbar;
			g_hwndBar = hwndBar;
			g_hwndTray = hwndTray;

			SetWindowSubclass(g_hwndBar, SubclassProc, IdBar, 0);
			SetWindowSubclass(g_hwndTray, SubclassProc, IdTray, 0);
		}
	}
}

void RemoveSubClass(HWND hwndTaskbar)
{
	if (g_hwndBar)
	{
		RemoveWindowSubclass(g_hwndBar, SubclassProc, IdBar);
		g_hwndBar = nullptr;
	}
	if (g_hwndTray)
	{
		RemoveWindowSubclass(g_hwndTray, SubclassProc, IdTray);
		g_hwndTray = nullptr;
	}
	if (g_hInstanceDll)
	{
		HINSTANCE hMod = g_hInstanceDll;
		g_hInstanceDll = nullptr;
		FreeLibrary(hMod);
	}
}


extern "C" {

__declspec(dllexport) void HookInit(HWND controllerHwnd, UINT uiHookMsg, UINT uiUnhookMsg, UINT uiPosMsg)
{
	g_controllerHwnd = controllerHwnd;
	g_uiHookMsg = uiHookMsg;
	g_uiUnhookMsg = uiUnhookMsg;
	g_uiPosMsg = uiPosMsg;
}

__declspec(dllexport) LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		CWPSTRUCT* pcwp = reinterpret_cast<CWPSTRUCT*>(lParam);
		if (pcwp)
		{
			if (pcwp->message == g_uiHookMsg)
				InstallSubClass(pcwp->hwnd);
			else
			if (pcwp->message == g_uiUnhookMsg)
				RemoveSubClass(pcwp->hwnd);
		}
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

};
