#include "ftp.h"
#include "svcconf.h"
#include "log.h"
#include "string.h"

int main(void)
{
    Config cfg;
    if (ConfParse(DFLT_CONF_FILE, &cfg)) {
        Log(L"[-] ConfParse failed\n");
        return 1;
    }
    if (!strcmp(cfg.conf, "")) {
        ConfInit(NULL);
        Log(L"no conf\n");
    }
    else {
        ConfInit(&cfg);
        Log(L"some conf\n");
    }
    
    if (FtpInit()) {
        Log(L"[-] FtpInit failed\n");
        return 1;
    }

    if (FtpListen()) {
        Log(L"[-] FtpListen failed\n");
        return 1;
    }
    if (FtpClose()) {
        Log(L"[-] FtpClose failed\n");
        return 1;   
    }
    return 0;
}