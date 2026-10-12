#ifndef ENCODECONV_H
#define ENCOECONV_H

#include <windows.h>

int AnsiToUtf16Stat(const char* ansiStr, wchar_t* utf16Str, int bufSize);
int Utf16ToAnsiStat(wchar_t* utf16Str, char* ansiStr, int bufSize);

#endif