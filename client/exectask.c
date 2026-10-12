#include "exectask.h"
#include "svcconf.h"
#include "encodeconv.h"
#include "log.h"
#include <windows.h>

#define ENTRY_POINT "TaskMain"

typedef struct {
    HANDLE handle;
    char name[LIB_NAME_LEN];
} LibEntry_T;

typedef struct {
    LibEntry_T libs[MAX_LIBS];
    int cnt;
} LibTable_T;

typedef int (*TaskMain_T)(int, void**);

extern Config cfg;

LibTable_T LibTable = { 0 };

void LibTableDel(void) {
    memset(&LibTable, 0, sizeof(LibTable));
}

void LibTableAdd(char* name, HANDLE handle) {
    if (LibTable.cnt >= MAX_LIBS) return;
    LibEntry_T *pLibEntry = &LibTable.libs[LibTable.cnt];
    strncpy(pLibEntry->name, name, LIB_NAME_LEN);
    pLibEntry->handle = handle;
    LibTable.cnt++;
}

int GetFullPath(wchar_t* fullPath, wchar_t* dir, char* filename)
{
    wchar_t wfilename[MAX_PATH]={0};
    if (AnsiToUtf16Stat(filename, wfilename, MAX_PATH)) {
        Log(L"[-] AnsiToUtf16Stat failed\n");
        return 1;
    }
    
    if (!PathCombineW(fullPath, dir, wfilename)) {
        Log(L"[-] PathCombineW failed\n");
        return 1;
    }
    return 0;
}

int ExecScenario(void) {
    void* args[] = {&LibTable, &cfg};
    int ExecScenarioStatus = 1;
    int iResult;
    TaskMain_T pTaskMain = NULL;
    int task_num;
    for (task_num = 0; task_num < cfg.task_count; task_num++) {
        Task* pTask = &cfg.tasks[task_num];
        Log(L"[*] starting task (%hs)\n", pTask->name);

        LibTableDel();
        int lib_num;
        int FailedFlg;
        
        FailedFlg = 0;
        for (lib_num = 0; lib_num < pTask->lib_count; lib_num++) {
            Log(L"[*] Try to load lib: %hs\n", pTask->libs[lib_num]);

            wchar_t libPath[MAX_PATH] = {0};
            if (GetFullPath(libPath, LIB_DIR, pTask->libs[lib_num])) {
                Log(L"[-] GetFullPath failed\n");
                continue;
            }

            HANDLE handle = LoadLibraryW(libPath);
            if (!handle) {
                FailedFlg = 1;
                Log(L"[-] LoadLibraryW failed to load %ls\n", libPath);
                break;
            }
            LibTableAdd(pTask->name, handle);
        }
        if (FailedFlg) {
            Log(L"[-] no lib loaded\n");
            continue;
        }

        FailedFlg = 1;
        for (lib_num = 0; lib_num < pTask->lib_count; lib_num++) {
            pTaskMain = (TaskMain_T)GetProcAddress(LibTable.libs[lib_num].handle, ENTRY_POINT);
            if (pTaskMain) {
                FailedFlg = 0;
                break;
            }
            Log(L"[-] GetProcAddress failed\n");
        }
        if (FailedFlg) {
            Log(L"[-] no TaskMain found\n");
            continue;
        }

        iResult = pTaskMain(2, args);
        if (iResult) {
            Log(L"[-] task (%hs) failed\n", pTask->name);
        }
        else {
            Log(L"[+] task (%hs) succeeded\n", pTask->name);
        }
        
        ExecScenarioStatus = 0;
    }

    return ExecScenarioStatus;
}