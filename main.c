#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0600

#include "util.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <tchar.h>
#include <string.h>
#include <commctrl.h>

typedef BOOL (APIENTRY *NvRefreshProc) (DWORD);

HINSTANCE g_hInst;
RECT g_rcMain, g_rcPrimary, g_rcScreen;

struct monitor_info {
	HMONITOR hMon;
	RECT area;
	BOOL primary, hasDesc;
	HDC hdc;
	TCHAR dev[32];
	TCHAR desc[192];
} *g_monitors = NULL;
BOOL g_isRunning = FALSE, g_doStretch = TRUE;
enum mouse_status {M_OUT, M_OVER, M_DOWN} cur_mouse_st = M_OUT;
size_t g_lenMonitors = 0, g_selection;
HANDLE hNvCpl;
HANDLE hHeap;
HDC g_hVSDC = NULL;
HMONITOR g_hCurMon;
size_t g_idxCurMon;

int
	text_height = 40, screen_height = 160,
	card_width = 240, card_height/* = text_height + screen_height*/,
	card_xpad = 20, card_ypad = 20,
	padleft = 15, padright = 15,
	padtop = 15, padbottom = 15,
	selleft = 5, selright = 5,
	seltop = 5, selbottom = 5;

#ifndef MONITORINFOF_PRIMARY
#define MONITORINFOF_PRIMARY 0x00000001
#endif

BOOL CALLBACK MonEnumProc (HMONITOR hMonitor, HDC hDCMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	size_t g_explen;
	struct monitor_info current;
	MONITORINFOEX monInfoEx;
	if (g_monitors) {
		g_explen = HeapSize(hHeap, 0, g_monitors) / sizeof(struct monitor_info);
		if (g_lenMonitors >= g_explen) {
			LPVOID res = HeapReAlloc(hHeap,
				0, g_monitors,
				(g_lenMonitors + 1) * sizeof(struct monitor_info));
			if (!res)
				return FALSE;
		}
	} else {
		g_explen = GetSystemMetrics(SM_CMONITORS);
		g_explen = g_explen >= 1 ? g_explen : 1;
		g_monitors = HeapAlloc(hHeap, 0, g_explen * sizeof(struct monitor_info));
		g_lenMonitors = 0;
		if (!g_monitors) return FALSE;
	}

	ZeroMemory (&current, sizeof(struct monitor_info));
	current.hMon = hMonitor;
	current.primary = FALSE;
	current.hasDesc = FALSE;
	current.desc[0] = TEXT('?');
	current.desc[1] = TEXT('\0');

	if (hMonitor == g_hCurMon) {
		g_idxCurMon = g_lenMonitors;
	}
	
	ZeroMemory (&monInfoEx, sizeof(MONITORINFOEX));
	monInfoEx.cbSize = sizeof(MONITORINFOEX);
	if (GetMonitorInfo (hMonitor, (LPMONITORINFO) &monInfoEx)) {
		current.area = monInfoEx.rcMonitor;
		current.primary = (monInfoEx.dwFlags & MONITORINFOF_PRIMARY) == MONITORINFOF_PRIMARY;
		if (current.primary) {
			g_rcPrimary = monInfoEx.rcWork;
		}
		memcpy (current.dev, monInfoEx.szDevice, sizeof(monInfoEx.szDevice));
		_tprintf (TEXT("DEV : \"%s\"\n"), current.dev);
	}

	memcpy (g_monitors + (g_lenMonitors++), &current, sizeof(struct monitor_info));
}

void sel_init (BOOL initial) {
	size_t numMonitors;
	int x, y, cx, cy, rows, columns;
	DWORD iAdapNum, iMonNum, iEntryNum;
	RECT rcWnd, rcOrig;
	DISPLAY_DEVICE adaptDev, monDev;

	SetTimer (g_hWndMain, 63, 1000, NULL);
	ShowWindow (g_hWndMain, SW_HIDE);
	ShowWindow (g_hWndMain, SW_SHOW);
	SetWindowLong (g_hWndMain, GWL_EXSTYLE, 0);
	SetWindowLong (g_hWndMain, GWL_STYLE, WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_OVERLAPPED);
	g_isRunning = FALSE;
	if (g_hVSDC)
		ReleaseDC (NULL, g_hVSDC);
	g_hVSDC = GetDC(NULL);

	// Workaround for nVIDIA connnection state cache
	if (hNvCpl) {
		NvRefreshProc procRefresh = (NvRefreshProc)
			GetProcAddress (hNvCpl, "NvCplRefreshConnectedDevices");
		if (procRefresh) (*procRefresh) (1); // NVREFRESH_NONINTRUSIVE
	}

	numMonitors = GetSystemMetrics (SM_CMONITORS);
	if (g_monitors) {
		HeapFree (hHeap, 0, g_monitors);
		g_monitors = NULL;
	}

	g_lenMonitors = 0; // Just to make sure
	puts ("ENUMERATING");
	g_hCurMon = MonitorFromWindow(g_hWndMain, MONITOR_DEFAULTTONEAREST);
	EnumDisplayMonitors (NULL, NULL, MonEnumProc, 0);

	// Map dev to desc
	ZeroMemory (&adaptDev, sizeof(DISPLAY_DEVICE));
	adaptDev.cb = sizeof(DISPLAY_DEVICE);
	ZeroMemory (&monDev, sizeof(DISPLAY_DEVICE));
	monDev.cb = sizeof(DISPLAY_DEVICE);
	for (iAdapNum = 0; EnumDisplayDevices(NULL, iAdapNum, &adaptDev, 0); iAdapNum++) {
		_tprintf (TEXT("Started enum of (%u) adapter %s (%s)\n"), iAdapNum, adaptDev.DeviceName, adaptDev.DeviceString);
		for (iMonNum = 0; EnumDisplayDevices(adaptDev.DeviceName, iMonNum, &monDev, 0); iMonNum++) {
			_tprintf (TEXT("\t(%u) monitor %s (%s)\n"), iAdapNum, adaptDev.DeviceName, adaptDev.DeviceString);
			for (iEntryNum = 0; iEntryNum < g_lenMonitors; iEntryNum++) {
				TCHAR * entryName;
				size_t entryNameLen;
				if (g_monitors[iEntryNum].hasDesc) continue;
				_tprintf (TEXT("\tComparing monitor entry %s with Dev %s\n"), g_monitors[iEntryNum].dev, monDev.DeviceName);
				entryName = g_monitors[iEntryNum].dev;
				entryNameLen = _tcslen(entryName);
				if (_tcsncmp(entryName, monDev.DeviceName, 32) == 0 ||
						(_tcsncmp(entryName, monDev.DeviceName, entryNameLen) == 0 &&
						entryNameLen < 32 &&
						monDev.DeviceName[entryNameLen] == '\\')) {
					puts ("\t => MATCH!");
					_sntprintf(g_monitors[iEntryNum].desc, 256, TEXT("%d. %s"),
						iEntryNum + 1, monDev.DeviceString);
					g_monitors[iEntryNum].hasDesc = TRUE;
				}
			}
		}
	}

	// Assume g_lenMonitors > 0 for drawing GUI
	// 0~3=>0, 4~7=1
	// -3~0=>0, 1~4=>1
	rows = (g_lenMonitors + 3) / 4;
	columns = g_lenMonitors > 4 ? 4 : g_lenMonitors;
	// W = left_pad + (item_w + w_pad) * columns - w_pad + right_pad
	cx = padleft + (card_width + card_xpad) * columns - card_xpad + padright;
	// H = top_pad + (item_h + w_pad) * rows - w_pad + right_pad
	cy = padtop + (card_height + card_ypad) * rows - card_ypad + padbottom;
	printf ("COUNT: %d, GRID: %dx%d, SIZE: %dx%d\n", g_lenMonitors, rows, columns, cx, cy);

	rcWnd.left = 0; rcWnd.top = 0;
	rcWnd.right = cx; rcWnd.bottom = cy;

	AdjustWindowRectEx(&rcWnd,
		GetWindowLong(g_hWndMain, GWL_STYLE),
		GetMenu(g_hWndMain) != NULL,
		GetWindowLong(g_hWndMain, GWL_EXSTYLE));
	
	ZeroMemory (&rcOrig, sizeof(RECT));
	printf ("Getting Window position from HWND %p\n", g_hWndMain);
	if (initial || !GetWindowRect(g_hWndMain, &rcOrig)) {
		MONITORINFO monInfo = {sizeof(MONITORINFO)};
		
		GetMonitorInfo (MonitorFromWindow(g_hWndMain, MONITOR_DEFAULTTONEAREST), &monInfo);
		rcOrig = monInfo.rcWork;
	}

	cx = rcWnd.right - rcWnd.left;
	cy = rcWnd.bottom - rcWnd.top;

	// A = (Os + Oe) / 2 - N / 2 = (Os + Oe - N) / 2

	printf ("Old center x: %d, y: %d\n", (rcOrig.left + rcOrig.right) / 2, (rcOrig.top + rcOrig.bottom) / 2);
	x = (rcOrig.left + rcOrig.right - cx) / 2;
	y = (rcOrig.top + rcOrig.bottom - cy) / 2;

	printf ("Set pos x: %d, y: %d, cx: %d, cy: %d\n", x, y, cx, cy);
	SetWindowPos (g_hWndMain, NULL, x, y, cx, cy, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}

void sel_getrect (size_t item, LPRECT rect) {
	int x, y;

	x = padleft + (card_width + card_xpad) * (item % 4);
	y = padtop + (card_height + card_ypad) * (item / 4);
	(*rect).left = x;
	(*rect).top = y;
	(*rect).right = x + card_width;
	(*rect).bottom = y + card_height;
}

BOOL sel_drawframe (HDC hdc, size_t item) {
	RECT rcItem;
	RECT rcCur, rcText;
	HBRUSH hBrush;
	HGDIOBJ hOldFont;
	COLORREF oldBkColor;
	int oldBkMode;
	DWORD txtCntr, txtWd;
	
	sel_getrect (item, &rcItem);

	oldBkMode = SetBkMode (hdc, OPAQUE);
	if (oldBkMode == 0) return FALSE;

	hBrush = (HBRUSH) ((g_idxCurMon == item ? COLOR_3DDKSHADOW : COLOR_3DFACE) + 1);
	if (cur_mouse_st != M_OUT && g_selection == item) {
		if (cur_mouse_st == M_OVER) {
			hBrush = (HBRUSH) (COLOR_3DHILIGHT + 1);
		} else if (cur_mouse_st == M_DOWN) {
			hBrush = (HBRUSH) (COLOR_3DSHADOW + 1);
		}
	}
	oldBkColor = GetSysColor((int)(INT_PTR)(hBrush) - 1);
	//printf ("Using color %06X\n", oldBkColor);
	oldBkColor = SetBkColor (hdc, oldBkColor);
	if (oldBkColor == CLR_INVALID) {
		SetBkMode (hdc, oldBkMode);
		return FALSE;
	}

// AABBBBCC
// AAbbbbCC
// AA    CC
// AA    CC
// AA    CC
// DDDDDDCC

	// A
	rcCur.left = rcItem.left - selleft;
	rcCur.top = rcItem.top - seltop;
	rcCur.right = rcItem.left;
	rcCur.bottom = rcItem.bottom;
	FillRect (hdc, &rcCur, hBrush);

	// B
	rcCur.left = rcItem.left;
	rcCur.top = rcItem.top - seltop;
	rcCur.right = rcItem.right;
	rcCur.bottom = rcItem.top + text_height;
	FillRect (hdc, &rcCur, hBrush);

	// C
	rcCur.left = rcItem.right;
	rcCur.top = rcItem.top - seltop;
	rcCur.right = rcItem.right + selright;
	rcCur.bottom = rcItem.bottom + selbottom;
	FillRect (hdc, &rcCur, hBrush);

	// D
	rcCur.left = rcItem.left - selleft;
	rcCur.top = rcItem.bottom;
	rcCur.right = rcItem.right;
	rcCur.bottom = rcItem.bottom + selbottom;
	FillRect (hdc, &rcCur, hBrush);

	// text
	rcCur.left = rcItem.left;
	rcCur.top = rcItem.top;
	rcCur.right = rcItem.right;
	rcCur.bottom = rcItem.top + text_height;
	hOldFont = SelectObject (hdc, GetStockObject(SYSTEM_FIXED_FONT));
	if (!DrawText (hdc, g_monitors[item].desc, -1, &rcCur,
			DT_BOTTOM | DT_CENTER | DT_NOPREFIX | DT_WORDBREAK | DT_NOCLIP)) {
		printf ("DrawText Failure");
	}
	SelectObject (hdc, hOldFont);

	SetBkMode (hdc, oldBkMode);
	SetBkColor (hdc, oldBkColor);
}

BOOL sel_drawscr (HDC hdc, size_t item) {
	RECT rcItem;
	int oldStrMode;
	RECT* rcSrc;
	BOOL retv;
	POINT oldBO;

	sel_getrect (item, &rcItem);
	rcItem.top = rcItem.bottom - screen_height;
	rcSrc = &(g_monitors[item].area);

	if (!(oldStrMode = SetStretchBltMode (hdc, HALFTONE))) {
		return FALSE;
	}

	SetBrushOrgEx (hdc, 0, 0, &oldBO);

	retv = StretchBlt (hdc, rcItem.left, rcItem.top,
		rcItem.right - rcItem.left,
		rcItem.bottom - rcItem.top,
		g_hVSDC,
		(*rcSrc).left, (*rcSrc).top,
		(*rcSrc).right - (*rcSrc).left,
		(*rcSrc).bottom - (*rcSrc).top,
		SRCCOPY);

	SetStretchBltMode (hdc, oldStrMode);
	SetBrushOrgEx (hdc, oldBO.x, oldBO.y, NULL);
	return retv;
}

void sel_drawallscr (HDC hdc) {
	size_t i;

	for (i = 0; i < g_lenMonitors; i++) {
		sel_drawscr (hdc, i);
	}
}

void sel_drawall (HDC hdc) {
	size_t i;

	for (i = 0; i < g_lenMonitors; i++) {
		sel_drawframe (hdc, i);
		sel_drawscr (hdc, i);
	}
}

void scr_init ();

void sel_updatesel (HWND hWnd, UINT nMsg) {
	POINT ptCur;
	long int x, y;
	size_t cursel = 0xffffffff, oldsel = 0xffffffff, rows;
	BOOL valid = FALSE, oldValid = FALSE, oldDrag = FALSE;
	HDC hdc;

	GetCursorPos (&ptCur);
	ScreenToClient (hWnd, &ptCur);

	do {
		if (nMsg == WM_MOUSELEAVE) break;
		// pad-relative pos
		x = ptCur.x - (padleft - selleft);
		if (x < 0 || (x % (selleft + card_width + card_xpad) >= (selleft + card_width + selright))) break;
		y = ptCur.y - (padtop - seltop);
		if (y < 0 || (y % (seltop + card_height + card_ypad) >= (seltop + card_height + selbottom))) break;

		// Align selection
		// W = left_pad + (item_w + w_pad) * columns - w_pad + right_pad
		// H = top_pad + (item_h + w_pad) * rows - w_pad + right_pad
		x = (ptCur.x - (padleft - card_xpad / 2)) / (card_width + card_xpad);
		if (x < 0 || x >= 4) break;
		y = (ptCur.y - (padtop - card_ypad / 2)) / (card_height + card_ypad);
		rows = (g_lenMonitors + 3) / 4;
		if (y < 0 || y >= rows) break;
		if (y == rows - 1 && x >= ((3 + g_lenMonitors) % 4 + 1)) break;
		cursel = y * 4 + x;
		valid = TRUE;
	} while (0);
	hdc = GetDC(hWnd);
	oldsel = g_selection;
	oldValid = cur_mouse_st != M_OUT;
	oldDrag = cur_mouse_st == M_DOWN;
	if (nMsg == WM_LBUTTONDOWN) {
		// Start dragging
		if (valid) {
			cur_mouse_st = M_DOWN;
			g_selection = cursel;
			sel_drawframe(hdc, cursel);
			SetCapture (hWnd);
		}
	} else if (nMsg == WM_LBUTTONUP || nMsg == WM_CAPTURECHANGED || nMsg == WM_CANCELMODE) {
		// Stop dragging
		cur_mouse_st = valid ? M_OVER : M_OUT;
		if (valid) {
			g_selection = cursel;
			sel_drawframe(hdc, cursel);
		}
		ReleaseCapture ();
		if (oldDrag && valid) {
			ReleaseDC(hWnd, hdc);
			if (cursel != g_idxCurMon || IDOK == MessageBox(g_hWndMain, TEXT("현재 모니터입니다. 계속하시겠습니까?"), NULL, MB_ICONWARNING | MB_OKCANCEL))
				scr_init();
			return;
		}
	} else if (cur_mouse_st != M_DOWN) {
		// Moving mouse
		// Redraw only if this wasn't initially the selected one.
		cur_mouse_st = valid ? M_OVER : M_OUT;

		if (valid && (!oldValid || oldsel != cursel)) {
			g_selection = cursel;
			sel_drawframe(hdc, cursel);
		}
	}
	// Redraw old selection only when
	// 1. The mouse is not currently dragging.
	// 2. There was an old selection
	// if
	// 1. The selection is M_OUT, or
	// 2. The selection is different from the old one.
	if (cur_mouse_st != M_DOWN && oldValid && (cur_mouse_st == M_OUT || cursel != oldsel)) {
		printf ("Selection: %d -> %d\n", oldsel, cursel);
		sel_drawframe(hdc, oldsel);
	}
	ReleaseDC(hWnd, hdc);
}

void update_stretch_mode() {
	int tgw, tgh, dsw, dsh;
	dsw = g_rcScreen.right - g_rcScreen.left;
	dsh = g_rcScreen.bottom - g_rcScreen.top;
	tgw = g_rcMain.right - g_rcMain.left;
	tgh = g_rcMain.bottom - g_rcMain.top;
	g_doStretch = (dsw != tgw || dsh != tgh);
}

void scr_init() {
	MONITORINFO monInfo = {sizeof(MONITORINFO)};
	int tgw, tgh, dsw, dsh;
	g_rcScreen = g_monitors[g_selection].area;
	GetMonitorInfo (MonitorFromWindow(g_hWndMain, MONITOR_DEFAULTTONEAREST), &monInfo);

	SetWindowLong (g_hWndMain, GWL_EXSTYLE, WS_EX_TOPMOST);
	SetWindowLong (g_hWndMain, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE);
	ShowWindow (g_hWndMain, SW_MAXIMIZE);
	
	SetWindowPos(g_hWndMain, NULL,
		monInfo.rcMonitor.left,
		monInfo.rcMonitor.top,
		monInfo.rcMonitor.right - monInfo.rcMonitor.left,
		monInfo.rcMonitor.bottom - monInfo.rcMonitor.top,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOREDRAW);
	GetClientRect (g_hWndMain, &g_rcMain);
	update_stretch_mode();
	g_isRunning = TRUE;

	SetCursorPos ((g_rcScreen.left + g_rcScreen.right) / 2, (g_rcScreen.top + g_rcScreen.bottom) / 2);
	InvalidateRect (g_hWndMain, NULL, FALSE);
	SetTimer (g_hWndMain, 63, 1000 / 30, NULL); // 30fps!
}

BOOL scr_drawscr (HDC hdc) {
	int oldStrMode;
	BOOL retv;

	if (g_doStretch) {
		if (!(oldStrMode = SetStretchBltMode (hdc, COLORONCOLOR))) {
			return FALSE;
		}

		retv = StretchBlt (hdc, g_rcMain.left, g_rcMain.top,
			g_rcMain.right - g_rcMain.left,
			g_rcMain.bottom - g_rcMain.top,
			g_hVSDC,
			g_rcScreen.left, g_rcScreen.top,
			g_rcScreen.right - g_rcScreen.left,
			g_rcScreen.bottom - g_rcScreen.top,
			SRCCOPY);

		SetStretchBltMode (hdc, oldStrMode);
	} else {
		retv = BitBlt (hdc, g_rcMain.left, g_rcMain.top,
			g_rcMain.right - g_rcMain.left,
			g_rcMain.bottom - g_rcMain.top,
			g_hVSDC,
			g_rcScreen.left, g_rcScreen.top,
			SRCCOPY);
	}
	return retv;
}

LRESULT CALLBACK WndProc (HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam) {
	switch (nMsg) {
	case WM_CREATE:
	case WM_INITDIALOG:
		// May be called multiple times
		g_hWndMain = hWnd;
		g_hCurMon = MonitorFromWindow(g_hWndMain, MONITOR_DEFAULTTONEAREST);
		// sel_init (TRUE);
		break;
	case WM_MOVE:
		{
			HMONITOR hMon = MonitorFromWindow(g_hWndMain, MONITOR_DEFAULTTONEAREST);
			
			if (hMon != g_hCurMon) {
				size_t i, old;
				HDC hdc = GetDC(hWnd);
				
				for (i = 0; i < g_lenMonitors; i++) {
					if (g_monitors[i].hMon == hMon) {
						old = g_idxCurMon;
						g_idxCurMon = i;
						sel_drawframe (hdc, i);
						sel_drawframe (hdc, old);
						break;
					}
				}
				if (i == g_lenMonitors) {
					printf ("Cannot find monitor index for handle %p, iterated %d times\n", hMon, i);
				}
				g_hCurMon = hMon;
				ReleaseDC(hWnd, hdc);
			}
		}
		break;
	case WM_ERASEBKGND:
		if (g_isRunning) break;
		return DefWindowProc (hWnd, nMsg, wParam, lParam);
	case WM_SIZE:
		GetClientRect (g_hWndMain, &g_rcMain);
		if (g_isRunning)
			update_stretch_mode();
		break;
	case WM_LBUTTONDBLCLK:
		if (g_isRunning) {
	case WM_DISPLAYCHANGE:
			sel_init (TRUE);
		}
		break;
	case WM_NCMOUSELEAVE:
	case WM_MOUSELEAVE:
	case WM_MOUSEMOVE:
	case WM_MOUSEHOVER:
	case WM_CAPTURECHANGED:
	case WM_CANCELMODE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		if (!g_isRunning)
			sel_updatesel(hWnd, nMsg);
		break;
	case WM_CHAR:
		if (wParam == VK_ESCAPE)
			sel_init(TRUE);
		break;
	case WM_TIMER:
		if (wParam == 63) {
			HDC hdc = GetDC (hWnd);
			if (g_isRunning)
				scr_drawscr (hdc);
			else
				sel_drawallscr (hdc);
			ReleaseDC (hWnd, hdc);
		} else return DefWindowProc (hWnd, WM_TIMER, wParam, lParam);
		break;
	case WM_PAINT:
		{
			HDC hdc; PAINTSTRUCT ps;
			hdc = BeginPaint (hWnd, &ps);
			if (g_isRunning)
				scr_drawscr (hdc);
			else
				sel_drawall (hdc);
			EndPaint (hWnd, &ps);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc (hWnd, nMsg, wParam, lParam);
	}
	return 1;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	WNDCLASS wndClass = {
		CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_PARENTDC,
		WndProc, 0, 0,
		hInstance, NULL, LoadCursor(NULL, IDC_ARROW),
		(HBRUSH) (COLOR_WINDOW + 1),
		NULL, TEXT("DisplayCloneCls")};
	MSG msg;
	ATOM atomCls;

	g_hInst = hInstance;
	card_height = text_height + screen_height;

	hNvCpl = LoadLibrary(TEXT("NvCpl"));
	hHeap = GetProcessHeap();

	if (!(atomCls = RegisterClass (&wndClass)))
		return 1;

	if (!(g_hWndMain = CreateWindowEx (0,
			MAKEINTATOM (atomCls),
			TEXT("미러링할 디스플레이를 선택하세요"),
			WS_CAPTION | WS_SYSMENU | WS_OVERLAPPED,
			0, 0, 640, 480,
			HWND_DESKTOP, NULL, hInstance, NULL)))
		return 2;

	sel_init (TRUE);

	while (GetMessage (&msg, NULL, 0, 0)) {
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	if (g_hVSDC)
		ReleaseDC(NULL, g_hVSDC);

	UnregisterClass (MAKEINTATOM (atomCls), hInstance);
	FreeLibrary (hNvCpl);

	return (int) msg.wParam;
}
