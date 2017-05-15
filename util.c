#define __DC_UTIL_C
#include "util.h"

LPSTR GetFormatStrA (LPCSTR lpFormat, ...) {
	LPSTR lpBuffer = NULL;
	va_list ap;

	va_start(ap, lpFormat);
	FormatMessageA (FORMAT_MESSAGE_FROM_STRING |
		FORMAT_MESSAGE_ALLOCATE_BUFFER,
		lpFormat, 0, 0, (LPSTR) &lpBuffer, 0, &ap);
	va_end(ap);
	return lpBuffer;
}

LPWSTR GetFormatStrW (LPCWSTR lpFormat, ...) {
	LPWSTR lpBuffer = NULL;
	va_list ap;

	va_start(ap, lpFormat);
	FormatMessageW (FORMAT_MESSAGE_FROM_STRING |
		FORMAT_MESSAGE_ALLOCATE_BUFFER,
		lpFormat, 0, 0, (LPWSTR) &lpBuffer, 0, &ap);
	va_end(ap);
	return lpBuffer;
}

void LogMessage (LPCTSTR lpText, DWORD dwLen) {
	DWORD bytesWritten = 0;
	/*
	while (g_hLog && g_hLog != INVALID_HANDLE_VALUE) {
		LPCSTR lpLog;
		int req;

#ifdef _UNICODE
		req = WideCharToMultiByte(CP_UTF8, 0, lpText, dwLen, NULL, 0, NULL, NULL);
		if (!req) break;
		lpLog = (LPCSTR) HeapAlloc (GetProcessHeap(), 0, req);
		if (!lpLog) break;
		req = WideCharToMultiByte(CP_UTF8, 0, lpText, dwLen, (LPSTR) lpLog, req, NULL, NULL);
#else
		lpLog = lpText; req = dwLen;
#endif
		if (req) WriteFile (g_hLog, lpLog, req, &bytesWritten, NULL);
		break;
	}
	*/
}

int ShowErrorImpl (LPCTSTR file, const int line) {
	DWORD dwErr = GetLastError (), dwLen, dwLenFinal;
	static const DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	LPTSTR buf = NULL, bufFinal = NULL;
	DWORD_PTR arg[] = {(DWORD_PTR) file, (DWORD_PTR) line, 0, 0};

	if (dwErr == ERROR_SUCCESS)
		return 0;

	arg[3] = dwErr;

	dwLen = FormatMessage (dwFlags, NULL, dwErr, LANG_USER_DEFAULT, (LPTSTR) &buf, 0, NULL);
	arg[2] = (DWORD_PTR) (dwLen && buf ? buf : _T("Unknown"));
	dwLenFinal = FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY, _T("Error %4!u! on %1:%2!u!\r\n%3%0"), 0, 0, (LPTSTR) &bufFinal, 0, (va_list*) arg);
	LogMessage (bufFinal, dwLenFinal);
	
	MessageBox (g_hWndMain, (dwLenFinal && bufFinal) ? bufFinal : _T("The error message could not be found or built."), NULL, MB_OK | MB_ICONHAND);
	if (dwLenFinal && bufFinal) LocalFree (bufFinal);
	if (dwLen && buf) LocalFree (buf);
	return 1;
}
