#include "ftp.h"
#include "tp.h"
#include "pkt.h"
#include "svcconf.h"
#include "encodeconv.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static int active_flg = 1;

static SOCKET FtpSocket;
static int FtpLoopCond = 1;
static char ActiveName[NAME_SZ] = {0};

static int talktosrv = 0;

static int error_occured = 0;

#define BUF_LEN 512
#define MAX_DATA_SZ (BUF_LEN - HEADER_SZ)

int FtpCreateDir(const wchar_t* path) {
    if (CreateDirectoryW(path, NULL)) {
        //Log(L"[+] FtpCreateDir succeeded\n");
        return 0;
    }

    DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS) {
        DWORD attrs = GetFileAttributesW(path);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            //Log(L"[+] FtpCreateDir succeeded: already exists\n");
            return 0;
        } else {
            if (!DeleteFileW(path)) {
                DWORD err = GetLastError();
                Log(L"[-] DeleteFileW failed: %lu\n", err);
                return 1;
            }
            if (CreateDirectoryW(path, NULL)) {
                //Log(L"[+] FtpCreateDir succeeded after deleting file of the sane name\n");
                return 0;
            }
            else {
                error = GetLastError();
                Log(L"[-] CreateDirectoryW failed: %lu\n", error);
                return 1;
            }
        }
    }

    // other errors ...
    Log(L"[-] CreateDirectoryW failed: %lu\n", error);
    return 1;
}


int FtpInit(void)
{
    if (FtpCreateDir(LIB_DIR)) return 1;
    if (FtpCreateDir(TMP_DIR)) return 1;
    if (EstablishConnection(&FtpSocket, SRV_ADDR)) return 1;
    return 0;
}

int FtpClose(void)
{
    return CloseConnection(FtpSocket);
}

int FtpSendFile(char* filename, int last_file_flg)
{
    char* dstfilename = filename;
    int filename_sz = strlen(filename);
    for (int k = filename_sz-1 ; k >= 0 ; k--) {
        if (filename[k] == '/' || filename[k] == '\\') {
            dstfilename = filename + k + 1;
            if (dstfilename >= filename + filename_sz) {
                Log(L"[-] file ends on slash");
                return 1;
            }
            break;
        }
    }

    if (strlen(dstfilename) >= NAME_SZ) {
        Log(L"[-] strlen(dstfilename) >= NAME_SZ, [%hs]\n", dstfilename);
        return 1;
    }


    PKT_T pkt;
    PKT_HEADER_T* pheader = &pkt.header;
    unsigned char type = DATA;
    unsigned long data_len;
    char data_offs = 32;
    int num = 0;
    unsigned char sendbuf[HEADER_SZ+MAX_DATA_SZ];
    unsigned char readbuf[MAX_DATA_SZ];
    
    memmove(&pheader->type, &type, TYPE_SZ);
    memmove(&pheader->data_offs, &data_offs, DATA_OFFS_SZ);
    memmove(&pheader->num, &num, NUM_SZ);
    memmove(&pheader->name, dstfilename, NAME_SZ);
    pkt.data = readbuf;
    

    FILE* fp;
    unsigned long file_sz;
    unsigned long file_idx = 0;

    fp = fopen(filename, "rb");
    if (!fp) {
        Log(L"[-] fopen failed\n");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    file_sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_sz == (unsigned long)-1) {
        Log(L"[-] ftell failed\n");
        fclose(fp);
        return 1;
    }

    // Log(L"[*] file_sz: %lu\n", file_sz);

    int flag_file_start = 1;
    while (file_idx < file_sz) {
        data_len = fread(readbuf, 1, MAX_DATA_SZ, fp);
        // Log("[*] datalen: %lu\n", data_len);
        // Log("readbuf: [%s]\n", readbuf);

        file_idx += MAX_DATA_SZ;

        unsigned char flags = 0;
        if (flag_file_start) {
            flag_file_start = 0;
            flags = flags | FLAG_FILE_START;
        }
        if (file_idx >= file_sz) {
            flags = flags | FLAG_FILE_END;
        }

        memmove(&pheader->flags, &flags, FLAGS_SZ);
        memmove(&pheader->data_len, &data_len, DATA_LEN_SZ);
        
        PktConstr(sendbuf, &pkt);
        if (SendPkt(FtpSocket, sendbuf, HEADER_SZ+data_len)) {
            Log(L"[-] FtpSendFile: SendPkt failed\n");
            break;
        }
    }
    
        
    fclose(fp);
    return 0;
}

int FtpSendFileW(wchar_t* wfilename, int last_file_flg)
{
    char ansiStr[MAX_PATH] = { 0 };
    Utf16ToAnsiStat(wfilename, ansiStr, MAX_PATH);
    return FtpSendFile(ansiStr, last_file_flg);
}

int FtpWriteFileW(const wchar_t* dir, const wchar_t* filename, const void* data, DWORD dataSize, int FileStartFlg)
{
    wchar_t fullPath[MAX_PATH];
    
    if (!PathCombineW(fullPath, dir, filename)) {
        Log(L"[-] PathCombineW failed\n");
        return 1;
    }

    const wchar_t* mode = FileStartFlg ? L"wb" : L"ab";
    FILE* file = _wfopen(fullPath, mode);
    if (file == NULL) {
        Log(L"[-] _wfopen failed %s\n", fullPath);
        return 1;
    }

    size_t written = fwrite(data, 1, dataSize, file);
    if (written != dataSize) {
        Log(L"[-] fwrite failed: written %zu from %lu bytes\n", written, dataSize);
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}

int FtpSaveTmpFile(NAME_T* filename, const void* data, DWORD dataSize, int FileStartFlg)
{
    int iResult;
    wchar_t wfilename[NAME_SZ];
    iResult = AnsiToUtf16Stat((const char*)filename, wfilename, NAME_SZ);
    if (iResult) return 1;
    return FtpWriteFileW(TMP_DIR, wfilename, data, dataSize, FileStartFlg);
}

int FtpCollectFile(NAME_T* filename, const void* data, DWORD dataSize, int FileStartFlg)
{
    if (!strcmp(filename, "")) {
        return 1;
    }
    if (FileStartFlg || strncmp(ActiveName, filename, NAME_SZ)) {
        strncpy(ActiveName, filename, NAME_SZ);
    }
    return FtpSaveTmpFile(ActiveName, data, dataSize, FileStartFlg);
}

int FtpMoveReceivedFileW(wchar_t* wfilename)
{
    wchar_t sourceFile[MAX_PATH];
    if (!PathCombineW(sourceFile, TMP_DIR, wfilename)) {
        Log(L"[-] PathCombineW failed");
        return 1;
    }

    wchar_t destFile[MAX_PATH];
    if (!PathCombineW(destFile, LIB_DIR, wfilename)) {
        Log(L"[-] PathCombineW failed");
        return 1;
    }

    if (!MoveFileExW(sourceFile, destFile, MOVEFILE_REPLACE_EXISTING)) {
        Log(L"[-] MoveFileExW failed: %lu\n", GetLastError());
        return 1;
    }
    return 0;
}

int FtpMoveReceivedFile(NAME_T* filename)
{
    int iResult;
    wchar_t wfilename[NAME_SZ];
    iResult = AnsiToUtf16Stat((const char*)filename, wfilename, NAME_SZ);
    if (iResult) return 1;
    return FtpMoveReceivedFileW(wfilename);
}

int FtpQueryUpdate(void)
{
    unsigned char pkt[HEADER_SZ] = {0};
    unsigned char type = QUERY_UPDATE;
    memmove(&pkt[0], &type, TYPE_SZ);
    //memmove(&pkt[NAME_OFFS], "qwe\n", 3);
    return SendPkt(FtpSocket, pkt, HEADER_SZ);
}

int FtpAck(void)
{
    unsigned char bytes[HEADER_SZ] = {0};
    unsigned char type = ACK;
    memmove(&bytes[TYPE_OFFS], &type, TYPE_SZ);
    memmove(&bytes[NAME_OFFS], ActiveName, strlen(ActiveName));
    return SendPkt(FtpSocket, bytes, HEADER_SZ);
}

int FtpCloseMsg(void)
{
    unsigned char bytes[HEADER_SZ] = {0};
    unsigned char type = CLOSE;
    memmove(&bytes[TYPE_OFFS], &type, TYPE_SZ);
    memmove(&bytes[NAME_OFFS], ActiveName, strlen(ActiveName));
    return SendPkt(FtpSocket, bytes, HEADER_SZ);
}

int FtpListen(void)
{
    int unknown_pkts_cnt = 0;

    // Log(L"[->] DATA pkts send (log)\n");
    // if (FtpSendFileW(LOG_FILE, 1)) {
    //     Log(L"[-] FtpSendFileW failed to send %ls\n", LOG_FILE);
    //     error_occured = 1;
    // }
    // else {
    //     error_occured = 0;
    // }
    // // Log(L"[->] CLOSE pkt send\n");
    // if (FtpCloseMsg()) {
    //     Log(L"[-] FtpClose failed\n");
    //     error_occured = 1;
    // }
    // return 0;

    Log(L"[->] QUERY_UPDATE pkt send\n");
    if (FtpQueryUpdate()) {
        return 1;
    }

    int stored = 0;
    char recvbuf[BUF_LEN] = {0};
    
    while (FtpLoopCond) {
        
        PKT_T pkt = { 0 };
        if (RecvPkt(FtpSocket, recvbuf, &stored)) {
            FtpLoopCond = 0;
            break;
        }
        PktView(recvbuf, &pkt);
        // Log(L"pkt.header.data_len: %llu\n", pkt.header.data_len);

        switch (pkt.header.type)
        {
        case DATA: {
            Log(L"[<-] DATA pkt received\n");
            if (FLAG_FILE_START&pkt.header.flags) {
                Log(L"[*] FLAG_FILE_START\n");
            }
            Log(L"[*] pkt.header.name: %hs\n", pkt.header.name);
            if (FtpCollectFile(pkt.header.name,
                            pkt.data,
                            pkt.header.data_len,
                            FLAG_FILE_START&pkt.header.flags))
            {
                Log(L"[-] FtpCollectFile failed\n");
                error_occured = 1;
            }

            if (FLAG_FILE_END&pkt.header.flags) {
                Log(L"[*] FLAG_FILE_END\n");
                if (FtpMoveReceivedFile(pkt.header.name)) {
                    Log(L"[-] FtpMoveReceivedFile failed\n");
                    error_occured = 1;
                }
            }
            break;
        }
        case QUERY_LOG: {
            Log(L"[<-] QUERY_LOG pkt received\n");
            Log(L"[->] DATA pkts send (log)\n");
            if (FtpSendFileW(LOG_FILE, 1)) {
                Log(L"[-] FtpSendFileW failed to send %ls\n", LOG_FILE);
                error_occured = 1;
            }
            else {
                error_occured = 0;
            }
            break;
        }
        case QUERY_UPDATE: {
            Log(L"[<-] QUERY_UPDATE pkt received\n");
            Log(L"[->] DATA pkt send (log)\n");
            if (FtpSendFileW(LOG_FILE, 1)) {
                Log(L"[-] FtpSendFileW failed to send %ls\n", LOG_FILE);
                error_occured = 1;
            }
            else {
                error_occured = 0;
            }
            if (!talktosrv) {
                Log(L"[->] CLOSE pkt send\n");
                if (FtpCloseMsg()) {
                    Log(L"[-] FtpClose failed\n");
                    error_occured = 1;
                }
                FtpLoopCond = 0;
            }
            break;
        }
        case CLOSE: {
            Log(L"[<-] CLOSE pkt received\n");
            FtpLoopCond = 0;
            break;
        }
        default: {
            unknown_pkts_cnt++;
            Log(L"[<-] UNKNOWN pkt received: %d\n", unknown_pkts_cnt);
            for (int k=0; k < HEADER_SZ + 10; k++) {
                Log(L"%02X ", recvbuf[k]);
                if ((k + 1) % 8 == 0) Log(L"\n");
            }
            Log(L"\n");
            if (unknown_pkts_cnt == 5) {
                Log(L"[-] received 5 unknown pkts\n");
                FtpLoopCond = 0;
            }
            break;
        }
        }
        ClearPktFromBuf(&pkt, recvbuf, &stored);

        // if (error_occured) {
        //     Log(L"[->] DATA pkt send (log)\n");
        //     if (FtpSendFileW(LOG_FILE, 1)) {
        //         Log(L"[-] FtpSendFileW failed to send %ls\n", LOG_FILE);
        //         error_occured = 1;
        //     }
        //     else {
        //         error_occured = 0;
        //     }
        // }
    }

    return 0;
}