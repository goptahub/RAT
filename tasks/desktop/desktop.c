#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <wchar.h>
#include "..\..\client\log.h"

#define SRV_ADDR_LEN    16
#define SRV_PORT_LEN    6
#define TASK_NAME_LEN   17
#define LIB_NAME_LEN    17
#define MAX_TASKS       10
#define MAX_LIBS        10

typedef struct {
    char name[TASK_NAME_LEN];
    char libs[MAX_LIBS][LIB_NAME_LEN];
    int lib_count;
} Task;

typedef struct {
    char srv_addr[SRV_ADDR_LEN];
    char srv_port[SRV_PORT_LEN];
    char svc_name[MAX_PATH];
    char svc_path[MAX_PATH];
    char libdir[MAX_PATH];
    char tmpdir[MAX_PATH];
    char conf[MAX_PATH];
    char log[MAX_PATH];
    Task tasks[MAX_TASKS];
    int task_count;
    int state;
} Config;

wchar_t* LOG_FILE;

#ifndef MOVEFILE_OVERWRITE_IF_EXISTS
#define MOVEFILE_OVERWRITE_IF_EXISTS 1
#endif

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


int MoveDskFilesToDesktop(LPCWSTR sourceDir)
{
    WCHAR searchPath[MAX_PATH];
    WCHAR desktopPath[MAX_PATH];
    WCHAR fullSourcePath[MAX_PATH];
    WCHAR fileNameWithoutExt[MAX_PATH];
    WCHAR targetPath[MAX_PATH];
    WIN32_FIND_DATAW findData;
    HANDLE hFind;
    int successCount = 0;
    int errorCount = 0;


    HRESULT hr = SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktopPath);
    if (FAILED(hr)) {
        Log(L"[-] SHGetFolderPathW failed: 0x%08lx\n", hr);
        return 1;
    }

    wsprintfW(searchPath, L"%s\\*.dt", sourceDir);

    hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            Log(L"[-] No .dt files found in %s\n", sourceDir);
            return 1;
        }
        Log(L"[-] FindFirstFileW failed: %lu\n", err);
        return 1;
    }

    do {
        // ignore directories
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        wsprintfW(fullSourcePath, L"%s\\%s", sourceDir, findData.cFileName);

        // del .dt ext
        wcscpy_s(fileNameWithoutExt, MAX_PATH, findData.cFileName);
        wchar_t* dot = wcsrchr(fileNameWithoutExt, L'.');
        if (dot != NULL) {
            if (_wcsicmp(dot, L".dt") == 0) {
                *dot = L'\0';
            } else {
                // Log(L"[-] Unexpected extension (skipping): %s\n", findData.cFileName);
                errorCount++;
                continue;
            }
        } else {
            // Log(L"[-] No extension (skipping): %s\n", findData.cFileName);
            errorCount++;
            continue;
        }

        wsprintfW(targetPath, L"%s\\%s", desktopPath, fileNameWithoutExt);

        if (!MoveFileExW(fullSourcePath, targetPath, MOVEFILE_OVERWRITE_IF_EXISTS)) {
            DWORD err = GetLastError();
            Log(L"[-] MoveFileExW failed: %s: %lu\n", fullSourcePath, err);
            errorCount++;
        } else {
            Log(L"[+] MoveFileExW succeeded: %s\n", fullSourcePath);
            successCount++;
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);

    return (errorCount == 0) ? 0 : 1;
}

__declspec(dllexport) int WINAPI TaskMain(
    int argc, void** argv
)
{
    Log(L"TaskMain started\n");

    if (argc != 2) {
        Log(L"[-] argc != 2\n");
        return 1;
    }

    Config* cfg = (Config*)argv[1];
    wchar_t log_file[MAX_PATH];
    if (AnsiToUtf16Stat(cfg->log, log_file, MAX_PATH)) {
        Log(L"[-] AnsiToUtf16Stat failed %hs\n", cfg->log);
        return 1;
    }
    LOG_FILE = log_file;

    wchar_t libdir[MAX_PATH];
    if (AnsiToUtf16Stat(cfg->libdir, libdir, MAX_PATH)) {
        Log(L"[-] AnsiToUtf16Stat failed: %hs\n", cfg->libdir);
        return 1;
    }

    Log(L"[*] libdir: %s\n", libdir);

    return MoveDskFilesToDesktop(libdir);
}