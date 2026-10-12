#include "svcconf.h"
#include <stdio.h>

int main(void) {
    Config cfg;
    if (ConfParse("conf.conf", &cfg) == 0) {
        printf("Parsed successfully.\n");
        printf("srv_addr: %s\n", cfg.srv_addr);
        printf("srv_port: %s\n", cfg.srv_port);
        printf("svc_name: %s\n", cfg.svc_name);
        printf("svc_path: %s\n", cfg.svc_path);
        printf("libdir: %s\n", cfg.libdir);
        printf("tmpdir: %s\n", cfg.tmpdir);
        printf("conf: %s\n", cfg.conf);
        printf("log: %s\n", cfg.log);
        printf("Task count: %d\n", cfg.task_count);
        for (int i = 0; i < cfg.task_count; i++) {
            printf("Task %d: name=%s, libs=", i, cfg.tasks[i].name);
            for (int j = 0; j < cfg.tasks[i].lib_count; j++)
                printf("%s ", cfg.tasks[i].libs[j]);
            printf("\n");
        }
    } else {
        fprintf(stderr, "Failed to open config file.\n");
    }

    printf("=====================================\n");
    ConfInit(NULL);
    printf("srv_addr: %s\n", SRV_ADDR);
    printf("srv_port: %s\n", SRV_PORT);
    wprintf(L"svc_name: %ls\n", SVC_NAME);
    wprintf(L"svc_path: %ls\n", SVC_PATH);
    wprintf(L"libdir: %ls\n",   LIB_DIR);
    wprintf(L"tmpdir: %ls\n",   TMP_DIR);
    wprintf(L"conf: %ls\n",     CONF_FILE);
    wprintf(L"log: %ls\n",      LOG_FILE);
    return 0;
}