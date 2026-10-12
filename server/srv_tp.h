#ifndef SRV_TP_H
#define SRV_TP_H

#define DFLT_SRV_PORT  12345

#include "pkt.h"

int SrvInit(void);
int SrvAccept(void);
int SrvRecvPkt(char* buf, int buf_len, int* stored);
int SrvSendPkt(char* buf, int buf_len);
int ClearPktFromBuf(PKT_T* pkt, char* recvbuf, int* stored);
int SrvClose(void);

#endif