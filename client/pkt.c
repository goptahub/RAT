#include "pkt.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int PktView(char* bytes, PKT_T* pkt)
{
    if (!bytes || !pkt) return -1;
    PKT_HEADER_T* header = &pkt->header;
    memmove(&header->type,       &bytes[TYPE_OFFS],     TYPE_SZ);
    memmove(&header->flags,      &bytes[FLAGS_OFFS],    FLAGS_SZ);
    memmove(&header->data_len,   &bytes[DATA_LEN_OFFS], DATA_LEN_SZ);
    memmove(&header->data_offs,  &bytes[DATA_OFFS_OFFS],DATA_OFFS_SZ);
    memmove(&header->num,        &bytes[NUM_OFFS],      NUM_SZ);
    memmove(header->name,        &bytes[NAME_OFFS],     NAME_SZ);
    pkt->data = &bytes[pkt->header.data_offs];
    return 0;
}

int PktConstr(char* bytes, PKT_T* pkt)
{
    if (!bytes || !pkt) return -1;
    PKT_HEADER_T* header = &pkt->header;
    memmove(&bytes[TYPE_OFFS],     &header->type,       TYPE_SZ);
    memmove(&bytes[FLAGS_OFFS],    &header->flags,      FLAGS_SZ);
    memmove(&bytes[DATA_LEN_OFFS], &header->data_len,   DATA_LEN_SZ);
    memmove(&bytes[DATA_OFFS_OFFS],&header->data_offs,  DATA_OFFS_SZ);
    memmove(&bytes[NUM_OFFS],      &header->num,        NUM_SZ);
    memmove(&bytes[NAME_OFFS],     header->name,        NAME_SZ);
    memmove(&bytes[header->data_offs], pkt->data, header->data_len);
    return 0;
}