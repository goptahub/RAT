#ifndef LOG_H
#define LOG_H

#include <stdio.h>

int LogPrintf(const wchar_t* format, ...);

// #define CONSOLE_LOGGING
// #define LOGGING

#if defined(LOGGING) 
    #define Log(...) LogPrintf(__VA_ARGS__)
#elif defined(CONSOLE_LOGGING)
    #define Log(...) wprintf(__VA_ARGS__)
#else
    #define Log(wfstr) ((void)0)
#endif


#endif