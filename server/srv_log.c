#include "srv_log.h"
#include <stdarg.h>
#include <time.h>

void LogFile(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    FILE *fp = fopen(LOG_FILE, "a");
    if (fp) {
        vfprintf(fp, format, args);
        fflush(fp);
        fclose(fp);
    }

    va_end(args);
}