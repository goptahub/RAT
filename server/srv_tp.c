#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include "pkt.h"
#include "srv_log.h"

#define BUF_LEN 512
#define DFLT_SRV_PORT 12345
#define WAIT_TIMEOUT_SEC 10

#define CONNECT_TIMEOUT_S  5
#define RECV_TIMEOUT_S     5
#define SEND_TIMEOUT_S     5


int listen_sock;
int client_sock;

int SrvInit(void) {
    struct sockaddr_in server_addr;
    int port = DFLT_SRV_PORT;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_sock);
        return 1;
    }

    if (listen(listen_sock, 1) < 0) {
        perror("listen");
        close(listen_sock);
        return 1;
    }

    printf("Server is listening on port %d...\n", port);

    return 0;
}


int SrvAccept(void)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    // one connection a time (blocking accept)
    client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        printf("[-] accept failed\n");
        close(listen_sock);
        return 1;
    }

    printf("Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    

    struct timeval tv;
    tv.tv_sec = RECV_TIMEOUT_S;
    tv.tv_usec = 0;
    if (setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        Log("[-] setsockopt SO_RCVTIMEO failed\n");
    }
    tv.tv_sec = SEND_TIMEOUT_S;
    if (setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        Log("setsockopt SO_SNDTIMEO  failed\n");
    }
    
    return 0;
}

int ClearPktFromBuf(PKT_T* pkt, char* recvbuf, int* stored)
{
    unsigned long pkt_sz = pkt->header.data_len + HEADER_SZ;
    unsigned long rest_sz = BUF_LEN - pkt_sz;
    if (rest_sz < 0) return 1;

    // Log("stored: %d\npkt_sz: %lu\n rest_sz: %lu\n", *stored, pkt_sz, rest_sz);
    memmove(recvbuf, recvbuf + pkt_sz, rest_sz);
    memset(recvbuf + pkt_sz, 0, rest_sz);
    *stored = *stored - pkt_sz;
    return 0;
}

int SrvRecvPkt(char* recvbuf, int buf_len, int* stored)
{

    int iResult = 0;
    PKT_T pkt;

    if (*stored >= HEADER_SZ) {
        PktView(recvbuf, &pkt);
        unsigned long pkt_sz = pkt.header.data_len + HEADER_SZ;
        if (pkt_sz <= *stored) {
            return 0;
        }
    }


    do {
        iResult = recv(client_sock, &recvbuf[*stored], buf_len - *stored, 0);
        if (iResult > 0) {
            Log("Bytes received: %d\n", iResult);
            *stored += iResult;
            if (*stored >= HEADER_SZ) {
                PktView(recvbuf, &pkt);
                unsigned long pkt_sz = pkt.header.data_len + HEADER_SZ;
                if (pkt_sz <= *stored) {
                    return 0;
                }
            }
        }
        else if (iResult == 0) {
            Log("Connection closed\n");
            return 1;
        }
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                Log("Timeout: no data received from client\n");
            }
            else {
                Log("[-] recv failed\n");
                return 1;
            }      
        }
    } while (iResult > 0);

    return 0;
}


int SrvSendPkt(char* buf, int buf_len)
{
    ssize_t sent = send(client_sock, buf, buf_len, 0);
    if (sent < 0) {
        printf("[-] send failed\n");
        return 1;
    }
    return 0;
}


int SrvClose(void)
{
    close(client_sock);
    close(listen_sock);
    printf("Server finished.\n");

    return 0;
}