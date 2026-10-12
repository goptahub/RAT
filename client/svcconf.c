#include "svcconf.h"
#include "encodeconv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

char SRV_ADDR[SRV_ADDR_LEN] = { 0 };
char SRV_PORT[SRV_PORT_LEN] = { 0 };
wchar_t SVC_NAME[MAX_PATH] = { 0 };
wchar_t SVC_PATH[MAX_PATH] = { 0 };
wchar_t LIB_DIR[MAX_PATH] = { 0 };
wchar_t TMP_DIR[MAX_PATH] = { 0 };
wchar_t CONF_FILE[MAX_PATH] = { 0 };
wchar_t LOG_FILE[MAX_PATH] = { 0 };
int STATE = DFLT_STATE;

int ConfParse(wchar_t *filename, Config *cfg) {
    memset(cfg, 0, sizeof(Config));

    FILE *file = _wfopen(filename, L"r");
    if (!file) return 0;

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        /* Удаляем \n и \r */
        line[strcspn(line, "\n")] = '\0';
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';

        /* Разбор слов с учётом кавычек */
        char *words[32];
        int word_count = 0;
        char *p = line;
        int error = 0;

        while (*p) {
            while (*p && *p == ' ') p++;
            if (!*p) break;

            if (*p == '"') {
                p++;  /* пропускаем открывающую кавычку */
                char *start = p;
                while (*p && *p != '"') p++;
                if (*p != '"') {
                    error = 1;
                    break;
                }
                int wlen = p - start;
                char *word = malloc(wlen + 1);
                if (!word) { error = 1; break; }
                strncpy(word, start, wlen);
                word[wlen] = '\0';
                words[word_count++] = word;
                p++;  /* пропускаем закрывающую кавычку */
            } else {
                char *start = p;
                while (*p && *p != ' ') p++;
                int wlen = p - start;
                char *word = malloc(wlen + 1);
                if (!word) { error = 1; break; }
                strncpy(word, start, wlen);
                word[wlen] = '\0';
                words[word_count++] = word;
            }
        }

        if (error) {
            for (int i = 0; i < word_count; i++) free(words[i]);
            continue;
        }
        if (word_count == 0) continue;

        char *key = words[0];

        /* -------- Обработка ключей через макросы -------- */
        if (strcmp(key, KEY_TASK) == 0) {
            if (cfg->task_count < MAX_TASKS && word_count >= 2) {
                Task *t = &cfg->tasks[cfg->task_count];
                strncpy(t->name, words[1], TASK_NAME_LEN - 1);
                t->name[TASK_NAME_LEN - 1] = '\0';
                for (int i = 2; i < word_count && t->lib_count < MAX_LIBS; i++) {
                    strncpy(t->libs[t->lib_count], words[i], LIB_NAME_LEN - 1);
                    t->libs[t->lib_count][LIB_NAME_LEN - 1] = '\0';
                    t->lib_count++;
                }
                cfg->task_count++;
            }
        }
        else if (strcmp(key, KEY_SRV_ADDR) == 0) {
            if (word_count >= 2) {
                strncpy(cfg->srv_addr, words[1], SRV_ADDR_LEN - 1);
                cfg->srv_addr[SRV_ADDR_LEN - 1] = '\0';
            }
        }
        else if (strcmp(key, KEY_SRV_PORT) == 0) {
            if (word_count >= 2) {
                strncpy(cfg->srv_port, words[1], SRV_PORT_LEN - 1);
                cfg->srv_port[SRV_PORT_LEN - 1] = '\0';
            }
        }
        else if (strcmp(key, KEY_SVC_NAME) == 0) {
            if (word_count >= 2) {
                strncpy(cfg->svc_name, words[1], MAX_PATH - 1);
                cfg->svc_name[MAX_PATH - 1] = '\0';
            }
        }
        else if (strcmp(key, KEY_SVC_PATH) == 0) {
            if (word_count >= 2) {
                strncpy(cfg->svc_path, words[1], MAX_PATH - 1);
                cfg->svc_path[MAX_PATH - 1] = '\0';
            }
        }
        else if (strcmp(key, KEY_LIBDIR) == 0) {
            if (word_count >= 2) {
                strncpy(cfg->libdir, words[1], MAX_PATH - 1);
                cfg->libdir[MAX_PATH - 1] = '\0';
            }
        }
        else if (strcmp(key, KEY_TMPDIR) == 0) {
            if (word_count >= 2) {
                strncpy(cfg->tmpdir, words[1], MAX_PATH - 1);
                cfg->tmpdir[MAX_PATH - 1] = '\0';
            }
        }
        else if (strcmp(key, KEY_CONF) == 0) {
            if (word_count >= 2) {
                strncpy(cfg->conf, words[1], MAX_PATH - 1);
                cfg->conf[MAX_PATH - 1] = '\0';
            }
        }
        else if (strcmp(key, KEY_LOG) == 0) {
            if (word_count >= 2) {
                strncpy(cfg->log, words[1], MAX_PATH - 1);
                cfg->log[MAX_PATH - 1] = '\0';
            }
        }
        else if (strcmp(key, KEY_STATE) == 0) {
            if (word_count >= 2) {
                if (!strcmp(words[1], "1")) {
                    cfg->state = 1;
                }
                else if (!strcmp(words[1], "0")) {
                    cfg->state = 0;
                }
                else {
                    cfg->state = DFLT_STATE;
                }
            }
        }

        for (int i = 0; i < word_count; i++) free(words[i]);
    }

    fclose(file);
    return 0;
}

int ConfInit(Config* cfg)
{
    if (cfg) {
        strncpy(SRV_ADDR, cfg->srv_addr, SRV_ADDR_LEN);
        strncpy(SRV_PORT, cfg->srv_port, SRV_PORT_LEN);
        AnsiToUtf16Stat(cfg->svc_name, SVC_NAME, MAX_PATH);
        AnsiToUtf16Stat(cfg->svc_path, SVC_PATH, MAX_PATH);
        AnsiToUtf16Stat(cfg->libdir, LIB_DIR, MAX_PATH);
        AnsiToUtf16Stat(cfg->tmpdir, TMP_DIR, MAX_PATH);
        AnsiToUtf16Stat(cfg->conf, CONF_FILE, MAX_PATH);
        AnsiToUtf16Stat(cfg->log, LOG_FILE, MAX_PATH);
        STATE = cfg->state;
    }
    else {
        strncpy(SRV_ADDR, DFLT_SRV_ADDR, SRV_ADDR_LEN);
        strncpy(SRV_PORT, DFLT_SRV_PORT, SRV_PORT_LEN);
        wcsncpy(SVC_NAME, DFLT_SVC_NAME, MAX_PATH);
        wcsncpy(SVC_PATH, DFLT_SVC_PATH, MAX_PATH);
        wcsncpy(LIB_DIR,  DFLT_LIB_DIR,  MAX_PATH);
        wcsncpy(TMP_DIR,  DFLT_TMP_DIR,  MAX_PATH);
        wcsncpy(CONF_FILE,DFLT_CONF_FILE,MAX_PATH);
        wcsncpy(LOG_FILE, DFLT_LOG_FILE, MAX_PATH);
        STATE = DFLT_STATE;
    }
    return 0;
}
