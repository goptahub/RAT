#include "encodeconv.h"
#include <windows.h>
#include <stdio.h>


int AnsiToUtf16Stat(const char* ansiStr, wchar_t* utf16Str, int bufSize) {
    int utf16Size = MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, NULL, 0);
    
    if (utf16Size == 0) {
        // printf("[-] MultiByteToWideChar failed: %lu\n", GetLastError());
        return 1;
    }

    if (utf16Size > bufSize) {
        // printf("[-] buf capacity is not sufficient for utf16Str\n");
        return 1;
    }

    int result = MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, utf16Str, bufSize);
    if (result == 0) {
        // printf("[-] MultiByteToWideChar failed: %lu\n", GetLastError());
        free(utf16Str);
        return 1;
    }

    return 0;
}


int Utf16ToAnsiStat(wchar_t* utf16Str, char* ansiStr, int bufSize)
{
    if (!utf16Str || !ansiStr)
        return 1;

    int len = (int)wcslen(utf16Str);

    int required = WideCharToMultiByte(CP_ACP, 0, utf16Str, len, NULL, 0, NULL, NULL);
    if (required == 0)
        return 1;

    if (bufSize < required + 1)
        return 1;

    int written = WideCharToMultiByte(CP_ACP, 0, utf16Str, len, ansiStr, bufSize, NULL, NULL);
    if (written == 0)
        return 1;

    ansiStr[written] = '\0';

    return 0;
}
