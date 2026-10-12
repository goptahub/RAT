#include <winsock2.h>
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <shlwapi.h>
#include <time.h>
#include <locale.h>
#include <wininet.h>
#include <stdarg.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")

#define COPY_PATH "\xd3\xc5\xdf\xce\xc3\xce\xc4\xde\xd9\xcf\xcf\xc7\xcf"
#define XOR_BYTE 0xAA

BOOL GetCurrentUsername(wchar_t* buffer, DWORD bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }

    if (GetUserNameW(buffer, &bufferSize)) {
        return TRUE;
    }

    return FALSE;
}

int decrypt(char* path)
{
    memmove(path, COPY_PATH, sizeof(COPY_PATH));
    for (int i = 0; i < sizeof(COPY_PATH)-1; i++)
    {
        path[i] = path[i] ^ XOR_BYTE;
    }
    return 0;
}

int delExe(wchar_t* path)
{
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};

    wchar_t cmdLine[1024];
    wsprintfW(cmdLine, L"C:\\Windows\\System32\\cmd.exe /c del /f /q \"%s\"", path);


    BOOL success = CreateProcessW(
        NULL,
        cmdLine,
        NULL, NULL,
        FALSE,                
        DETACHED_PROCESS | CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi
    );

    if (success) {
        return 0;
    }
    return 1;
}

int main(void)
{ 
    char info[] = "it is great if you are reading it. tell me where i have been copied to :D";
    wchar_t installer_exe[MAX_PATH] = { 0 };
    wchar_t new_path[MAX_PATH] = {0};
    char filename[MAX_PATH] = { 0 };
    wchar_t username[MAX_PATH] = { 0 };
    DWORD length;
    length = GetModuleFileNameW(NULL, installer_exe, MAX_PATH);
    if (length <= 0) {
        // Log(L"[-] GetModuleFileNameW failed\n");
        return 1;
    }

    if (decrypt(filename)) return 1;

    if (!GetCurrentUsername(username, MAX_PATH)) return 1;

    wnsprintfW(new_path, MAX_PATH, L"C:\\Users\\%s\\%hs", username, filename);


    if (!CopyFileW(installer_exe, new_path, FALSE)) return 1;

    delExe(installer_exe);

    return 0;
}