#include "exectask.h"
#include "svcconf.h"
#include "log.h"

Config cfg;

int main(void)
{
    ConfParse(DFLT_CONF_FILE, &cfg);
    ConfInit(&cfg);
    Log(L"hi\n");
    if (ExecScenario()) {
        printf("[-] ExecScenario failed\n");
    }

    return 0;
}