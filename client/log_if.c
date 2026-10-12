#include  "log.h"
#include "svcconf.h"
#include <wchar.h>
#include <stdio.h>

#define LOGGING

int main(void)
{
    Config cfg;
    ConfInit(NULL);
    Log(L"qwe");

    return 0;
}