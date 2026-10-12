#ifndef SRV_FTP_H
#define SRV_FTP_H

int FtpInit(void);
int FtpClose(void);
int GetFullPath(char* path, char* dirname, char* filename);
int FtpSendFile(char* filename, int last_file_flg);
int FtpListen(void);


#endif