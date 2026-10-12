#ifndef SRV_LOG_H
#define SRV_LOG_H

#include <stdio.h>
#include <stdarg.h>

#define LOG_FILE "srvlog.log"

// #define CONSOLE_LOGGING
// #define FILE_LOGGING

#if defined(CONSOLE_LOGGING)
    #define Log(...) printf(__VA_ARGS__)
#elif defined(FILE_LOGGING)
    void LogFile(const char *format, ...);
    #define Log(...) LogFile(__VA_ARGS__)
#else
    #define Log(...) ((void)0)
#endif

#endif