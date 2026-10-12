#ifndef FTP_H
#define FTP_H

#include <wchar.h>

int FtpInit(void);
int FtpListen(void);
int FtpClose(void);

int FtpCreateDir(const wchar_t* path);

#endif
