#ifndef TP_H
#define TP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include "pkt.h"


#define BUF_LEN 512

int EstablishConnection(SOCKET *pConnectSocket, char* srv_addr);
int SendPkt(SOCKET ConnectSocket, char *sendbuf, int len);
int RecvPkt(SOCKET ConnectSocket, char *recvbuf, int* stored);
int ShutdownSocket(SOCKET ConnectSocket);
int CloseConnection(SOCKET ConnectSocket);
int ClearPktFromBuf(PKT_T* pkt, char* recvbuf, int* stored);

#endif