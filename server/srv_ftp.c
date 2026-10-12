#include "srv_ftp.h"
#include "srv_tp.h"
#include "srv_log.h"
#include "pkt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PATH 260
#define BUF_LEN 512
#define MAX_DATA_SZ (BUF_LEN - HEADER_SZ)
#define DOWNLOAD_DIR "download/" // where server stores downloaded files
#define FILE_DIR     "storage/"  // where is is better to store files 
#define LIST_FILE    "list.list" // must be in the same dir as server locates
#define MAX_LINE_LEN (MAX_PATH)

int FtpInit(void)
{
    if (SrvInit()){
        Log("[-] SrvInit failed\n");
        return 1;
    }

    return 0;
}

int FtpClose(void)
{
    if (SrvClose()) {
        Log("[-] SrvClose failed\n");
        return 1;
    }

    return 0;
}

int GetFullPath(char* path, char* dirname, char* filename)
{
    int dirname_sz = strlen(dirname);
    if (dirname_sz >= MAX_PATH) return 1;
    memmove(path, dirname, dirname_sz);
    int filename_sz = strlen(filename);
    int idx = dirname_sz + filename_sz;
    if (idx >= MAX_PATH) return 1;
    memmove(&path[dirname_sz], filename, filename_sz);
    path[idx] = 0;
    return 0;
}

int FtpCollectFile(char* dir, char* filename, void* data, unsigned long dataSize, int FileStartFlg)
{
    char fullPath[MAX_PATH];

    // Log("dir: %s\nfilename: %s\n", dir, filename);
    
    if (GetFullPath(fullPath, dir, filename)) {
        Log("[-] GetFullPath failed\n");
        return 1;
    }

    // Log("fullpath: %s\n", fullPath);

    char* mode = FileStartFlg ? "wb" : "ab";
    FILE* file = fopen(fullPath, mode);
    if (file == NULL) {
        Log("[-] fopen failed %s\n", fullPath);
        return 1;
    }

    size_t written = fwrite(data, 1, dataSize, file);
    if (written != dataSize) {
        Log("[-] fwrite failed: written %zu from %lu bytes\n", written, dataSize);
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}

int FtpSendFile(char* filename, int last_file_flg)
{
    char* dstfilename = filename;
    int filename_sz = strlen(filename);
    for (int k = filename_sz-1 ; k >= 0 ; k--) {
        if (filename[k] == '/' || filename[k] == '\\') {
            dstfilename = filename + k + 1;
            if (dstfilename >= filename + filename_sz) {
                Log("[-] file ends on slash");
                return 1;
            }
        }
    }

    if (strlen(dstfilename) >= NAME_SZ) {
        Log("[-] strlen(dstfilename) >= NAME_SZ\n");
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
        Log("[-] fopen failed\n");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    file_sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_sz == (unsigned long)-1) {
        Log("[-] ftell failed\n");
        fclose(fp);
        return 1;
    }

    // Log("[*] file_sz: %lu\n", file_sz);

    int flag_file_start = 1;
    while (file_idx < file_sz) {
        data_len = fread(readbuf, 1, MAX_DATA_SZ, fp);
        Log("[*] datalen: %lu\n", data_len);
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

        // flags = flags | FLAG_FILE_END;

        memmove(&pheader->flags, &flags, FLAGS_SZ);
        memmove(&pheader->data_len, &data_len, DATA_LEN_SZ);
        
        PktConstr(sendbuf, &pkt);
        if (SrvSendPkt(sendbuf, HEADER_SZ+data_len)) {
            Log("[-] SrvSendPkt failed\n");
            return 1;
        }
    }
    
        
    fclose(fp);
    return 0;
}

static size_t trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[--len] = '\0';
        if (len > 0 && str[len-1] == '\r')
            str[--len] = '\0';
    }
    return len;  // new len
}

int FtpSend(void) {
    char line[MAX_LINE_LEN + 1];
    int total_non_empty = 0;
    FILE *fp;

    fp = fopen(LIST_FILE, "r");
    if (fp == NULL) {
        Log("fopen (first pass)");
        return 1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        if (line[0] != '\0') { 
            total_non_empty++;
        }
    }
    fclose(fp);

    if (total_non_empty == 0) {
        return 1;
    }

    fp = fopen(LIST_FILE, "r");
    if (fp == NULL) {
        Log("fopen (second pass)");
        return 1;
    }

    // Log("[*] total_non_empty: %d\n", total_non_empty);
    int processed = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t len = trim_newline(line);
        if (len == 0) {
            continue; 
        }

        // Log("[*] processed: %d\n", processed);
        int last;
        if (processed == (total_non_empty - 1)) last = 1; else last = 0;
        if (FtpSendFile(line, last)) {
            Log("[-] FtpSend failed %s\n", line);
        }
        else Log("[+] FtpSend: %s\n", line);

        processed++;
    }

    fclose(fp);
    return 0;
}
int FtpQueryUpdate(void)
{
    unsigned char pkt[HEADER_SZ] = {0};
    unsigned char type = QUERY_UPDATE;
    memmove(&pkt[0], &type, TYPE_SZ);
    return SrvSendPkt(pkt, HEADER_SZ);
}

int FtpListen(void)
{
    int unknown_pkts_cnt = 0;

    if (SrvAccept()) {
        Log("[-] SrvAccept failed\n");
        return 1;
    }

    // if (FtpQueryUpdate()) {
    //     Log("[-] FtpQueryUpdate failed\n");
    // }

    int QueryUpdateSend = 0;
    int FtpLoopCond;
    int stored = 0;
    char recvbuf[BUF_LEN] = {0};
    while (FtpLoopCond) {
        PKT_T pkt;
        if (SrvRecvPkt(recvbuf, BUF_LEN, &stored)) {
            FtpLoopCond = 0;
            break;
        }
        PktView(recvbuf, &pkt);
        switch (pkt.header.type)
        {
        case QUERY_UPDATE: {
            Log("[<-] QUERY_UPDATE pkt received\n");
            if (FtpSend()) {
                Log("[-] FtpSend failed\n");
            }
            if (!QueryUpdateSend) {
                Log("[->] QUERY_UPDATE pkt send\n");
                if (FtpQueryUpdate()) {
                    Log("[-] FtpQueryUpdate failed\n");
                }
                else {
                    QueryUpdateSend = 1;
                }
            }
            break;
        }
        case DATA: {
            Log("[<-] DATA pkt received\n");
            if (FLAG_FILE_START&pkt.header.flags) {
                Log("[*] FLAG_FILE_START\n");
            }
            FtpCollectFile(DOWNLOAD_DIR,
                            pkt.header.name,
                            pkt.data,
                            pkt.header.data_len,
                            FLAG_FILE_START&pkt.header.flags);

            if (FLAG_FILE_END&pkt.header.flags) {
                Log("[*] FLAG_FILE_END\n");
            }
            break;
        }
        case CLOSE: {
            Log("[<-] CLOSE pkt received\n");
            FtpLoopCond = 0;
            break;
        }
        default: {
            unknown_pkts_cnt++;
            Log("[<-] UNKNOWN pkt received: %d\n", unknown_pkts_cnt);
            if (unknown_pkts_cnt == 5) {
                Log("[-] received 5 unknown pkts\n");
                FtpLoopCond = 0;
            }
            break;
        }
        }

        ClearPktFromBuf(&pkt, recvbuf, &stored);

    }

    return 0;
}
