#ifndef SVCCONF_H
#define SVCCONF_H

#include <wchar.h>
#include <windows.h>

#define DFLT_SRV_ADDR    "127.0.0.1"
#define DFLT_SRV_PORT    "12345"
#define DFLT_STATE       0

#define DFLT_SVC_EXE     L"wconm.exe"
#define DFLT_SVC_NAME    L"Windows Connection Manager"
#define DFLT_SVC_PATH    L"C:\\Windows\\System32"
#define DFLT_LIB_DIR     L"C:\\Windows\\System32\\ts-TS"
#define DFLT_TMP_DIR     L"C:\\Windows\\System32\\ts-TS\\Temp"
#define DFLT_CONF_FILE   L"C:\\Windows\\System32\\ts-TS\\conf.conf"
#define DFLT_LOG_FILE    L"C:\\Windows\\System32\\ts-TS\\log.log"

#define KEY_TASK       "task"
#define KEY_SRV_ADDR   "srv_addr"
#define KEY_SRV_PORT   "srv_port"
#define KEY_SVC_NAME   "svc_name"
#define KEY_SVC_PATH   "svc_path"
#define KEY_LIBDIR     "libdir"
#define KEY_TMPDIR     "tmpdir"
#define KEY_CONF       "conf"
#define KEY_LOG        "log"
#define KEY_STATE      "state"

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

extern char SRV_ADDR[SRV_ADDR_LEN];
extern char SRV_PORT[SRV_PORT_LEN];
extern wchar_t SVC_NAME[MAX_PATH];
extern wchar_t SVC_PATH[MAX_PATH];
extern wchar_t LIB_DIR[MAX_PATH];
extern wchar_t TMP_DIR[MAX_PATH];
extern wchar_t CONF_FILE[MAX_PATH];
extern wchar_t LOG_FILE[MAX_PATH];
extern int STATE;

int ConfInit(Config* cfg);
int ConfParse(wchar_t *filename, Config *cfg);

#endif