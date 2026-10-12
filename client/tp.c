#include "tp.h"
#include "pkt.h"
#include "svcconf.h"
#include "log.h"
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define CONNECT_TIMEOUT_MS  60000   //60000
#define RECV_TIMEOUT_MS     5000   
#define SEND_TIMEOUT_MS     5000   


int ConnectWithTimeout(SOCKET sock, const struct sockaddr *addr, int addrlen, int timeout_ms) {
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        return SOCKET_ERROR;
    }

    int res = connect(sock, addr, addrlen);
    if (res == 0) {
        mode = 0;
        ioctlsocket(sock, FIONBIO, &mode);
        return 0;
    }

    if (WSAGetLastError() != WSAEWOULDBLOCK) {
        return SOCKET_ERROR;
    }

    fd_set fdWrite;
    FD_ZERO(&fdWrite);
    FD_SET(sock, &fdWrite);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int sel_res = select(0, NULL, &fdWrite, NULL, &tv);
    if (sel_res == 0) {
        WSASetLastError(WSAETIMEDOUT);
        return SOCKET_ERROR;
    } else if (sel_res == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }

    int error = 0;
    int len = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    if (error != 0) {
        WSASetLastError(error);
        return SOCKET_ERROR;
    }

    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);
    return 0;
}

int EstablishConnection(SOCKET *pConnectSocket, char* srv_addr)
{
    WSADATA wsaData;
    *pConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL, *ptr = NULL, hints;
    int iResult;


    // 1. Инициализация Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        Log(L"WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    // 2. Настройка структуры подсказок для getaddrinfo
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // Разрешаем IPv4 или IPv6
    hints.ai_socktype = SOCK_STREAM;  // Потоковый сокет (TCP)
    hints.ai_protocol = IPPROTO_TCP;

    // 3. Получение адреса сервера по его имени
    iResult = getaddrinfo(srv_addr, SRV_PORT, &hints, &result);
    if (iResult != 0) {
        Log(L"getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // 4. Пытаемся подключиться к одному из полученных адресов
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        *pConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (*pConnectSocket == INVALID_SOCKET) {
            Log(L"socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // ---- Устанавливаем таймауты на send/recv ----
        int rcv_timeout = RECV_TIMEOUT_MS;
        int snd_timeout = SEND_TIMEOUT_MS;
        if (setsockopt(*pConnectSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&rcv_timeout, sizeof(rcv_timeout)) == SOCKET_ERROR) {
            Log(L"setsockopt(SO_RCVTIMEO) failed: %d\n", WSAGetLastError());
        }
        if (setsockopt(*pConnectSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&snd_timeout, sizeof(snd_timeout)) == SOCKET_ERROR) {
            Log(L"setsockopt(SO_SNDTIMEO) failed: %d\n", WSAGetLastError());
        }

        // ---- Подключение с таймаутом ----
        iResult = ConnectWithTimeout(*pConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen, CONNECT_TIMEOUT_MS);
        if (iResult == SOCKET_ERROR) {
            closesocket(*pConnectSocket);
            *pConnectSocket = INVALID_SOCKET;
            Log(L"connect failed or timed out, trying next address...\n");
            continue;
        }
        break; // Успех
    }

    freeaddrinfo(result); // Освобождаем память, выделенную getaddrinfo

    if (*pConnectSocket == INVALID_SOCKET) {
        Log(L"Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    return 0;
}

int SendPkt(SOCKET ConnectSocket, char *sendbuf, int len)
{
    int iResult;
    iResult = send(ConnectSocket, sendbuf, len, 0);
    if (iResult == SOCKET_ERROR) {
        Log(L"send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    //Log(L"Bytes Sent: %ld\n", iResult);
    return 0;
}

int ShutdownSocket(SOCKET ConnectSocket)
{
    int iResult;
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        Log(L"shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }
    return 0;
}

int RecvPkt(SOCKET ConnectSocket, char *recvbuf, int* stored)
{
    /*
    RecvPkt is a layer over recv. recv is to return bytes received at once. 
    recv belongs to tcp layer. RecvPkt is to return a packet (bytes distinguished as
    packet). RecvPkt belongs to ftp layer.
    */
    int iResult = 0;
    PKT_T pkt;

    /*
    check wether buffer contains unprocessed packet from previous RecvPkt
    */
    if (*stored >= HEADER_SZ) {
        PktView(recvbuf, &pkt);
        unsigned long pkt_sz = pkt.header.data_len + HEADER_SZ;
        if (pkt_sz <= *stored) {
            return 0;
        }
    }

    /*
    collect new packet via recv
    */
    do {
        iResult = recv(ConnectSocket, &recvbuf[*stored], BUF_LEN - *stored, 0);
        if (iResult > 0) {
            Log(L"Bytes received: %d\n", iResult);
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
            Log(L"Connection closed\n");
            return 1;
        }
        else {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT)
                Log(L"recv timed out\n");
            else
                Log(L"recv failed with error: %d\n", err);
            return 1;
        }
    } while (iResult > 0);

    return 0;
}

int ClearPktFromBuf(PKT_T* pkt, char* recvbuf, int* stored)
{
    unsigned long pkt_sz = pkt->header.data_len + HEADER_SZ;
    unsigned long rest_sz = BUF_LEN - pkt_sz;
    if (rest_sz < 0) return 1;
    Log(L"stored: %d\npkt_sz: %lu\nrest_sz: %lu\n", *stored, pkt_sz, rest_sz);

    memmove(recvbuf, recvbuf + pkt_sz, rest_sz);
    memset(recvbuf + pkt_sz, 0, rest_sz);
    *stored = *stored - pkt_sz;
    Log(L"new stored: %d\n", *stored);
    return 0;
}

int CloseConnection(SOCKET ConnectSocket)
{
    Log(L"[*] CloseConnection\n");
    ShutdownSocket(ConnectSocket);
    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}
