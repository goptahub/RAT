#include "log.h"
#include "svcconf.h"
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

int LogPrintf(const wchar_t* format, ...)
{
    wchar_t buffer[2048];
    va_list args;
    va_start(args, format);
    int len = _vsnwprintf(buffer, sizeof(buffer) / sizeof(wchar_t), format, args);
    va_end(args);
    if (len < 0 || len >= (int)(sizeof(buffer) / sizeof(wchar_t))) {
        buffer[sizeof(buffer) / sizeof(wchar_t) - 1] = L'\0';
    }

    FILE* log_file;
    errno_t err = _wfopen_s(&log_file, LOG_FILE, L"a+");
    if (err != 0) return 1;
    fwprintf(log_file, L"%s", buffer);
    fclose(log_file);
    return 0;
}


