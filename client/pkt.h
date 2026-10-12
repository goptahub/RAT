#ifndef PKT_H
#define PKT_H

#include <stdbool.h>

#define TYPE_OFFS 0
#define TYPE_SZ 1

#define FLAGS_OFFS 1
#define FLAGS_SZ 1

#define DATA_LEN_OFFS 2
#define DATA_LEN_SZ 8

#define DATA_OFFS_OFFS 10
#define DATA_OFFS_SZ 1

#define NUM_OFFS 11
#define NUM_SZ 4

#define NAME_OFFS 15
#define NAME_SZ   17


#define HEADER_SZ 32
#define DATA_OFFS HEADER_SZ

#define FLAG_WAIT       (0b00000001)
#define FLAG_FILE_END   (0b00000010)
#define FLAG_FILE_START (0b00000100)


typedef unsigned char      TYPE_T;
typedef unsigned char      FLAGS_T;
typedef unsigned int       NUM_T;
typedef unsigned long long DATA_LEN_T;
typedef unsigned char      NAME_OFFS_T;
typedef unsigned char      DATA_OFFS_T;
typedef unsigned char      DATA_T;
typedef char               NAME_T;

typedef struct _PKT_HEADER_T {
    TYPE_T type;
    FLAGS_T flags;
    DATA_LEN_T data_len;
    DATA_OFFS_T data_offs;
    NUM_T num;
    NAME_T name[NAME_SZ];
} PKT_HEADER_T;

typedef struct _PKT_T {
    PKT_HEADER_T header;
    DATA_T* data;
} PKT_T;


typedef enum _PKT_TYPE_T {
    START,
    STOP,
    CLOSE,
    DATA,
    QUERY_UPDATE,
    QUERY_START,
    QUERY_STOP,
    QUERY_LOG,
    ACK
} PKT_TYPE_T;

int PktView(char* bytes, PKT_T* pkt);
int PktConstr(char* bytes, PKT_T* pkt);

#endif