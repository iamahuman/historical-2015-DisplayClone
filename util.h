#ifndef __DC_UTIL_INCLUDED
#define __DC_UTIL_INCLUDED
#include <windows.h>
#include <tchar.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

#ifdef __DC_UTIL_C
#define UTIL_EXTERN
#define UTIL_DEF(x) = x
#else
#define UTIL_EXTERN extern
#define UTIL_DEF(x)
#endif

// Main Window
UTIL_EXTERN HWND g_hWndMain UTIL_DEF(NULL);

// Function prototypes

// Utilities
UTIL_EXTERN LPSTR GetFormatStrA (LPCSTR lpFormat, ...);
UTIL_EXTERN LPWSTR GetFormatStrW (LPCWSTR lpFormat, ...);
UTIL_EXTERN int ShowErrorImpl(LPCTSTR lpFile, const int line);
UTIL_EXTERN void LogMessage (LPCTSTR lpText, DWORD dwLen);

#ifdef _UNICODE
#define GetFormatStr GetFormatStrW
#else
#define GetFormatStr GetFormatStrA
#endif

#define ZeroOverlapped(ol) ZeroMemory((ol), sizeof(OVERLAPPED) - sizeof(HANDLE))
#define ShowError() ShowErrorImpl(TEXT(__FILE__), __LINE__)

#ifdef UNICODE
#define GetFormatStr GetFormatStrW
#else
#define GetFormatStr GetFormatStrA
#endif

#endif
