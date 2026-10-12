#include "srv_ftp.h"
#include "stdio.h"

int main(void)
{
    if (FtpInit()) {
        printf("[-] FtpInit failed\n");
        return 1;
    }

    if (FtpListen()) {
        printf("[-] FtpListen failed\n");
        return 1;
    }

    if (FtpClose()) {
        printf("[-] FtpClose failed\n");
        return 1;
    }

    return 0;
}