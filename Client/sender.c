#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP    "YOUR_SERVER_IP"
#define SERVER_PORT  8080
#define BUFFER_SIZE  65536

static unsigned long long htonll(unsigned long long v) {
    unsigned long long hi = htonl((unsigned long) (v >> 32));
    unsigned long long lo = htonl((unsigned long) (v & 0xFFFFFFFFULL));
    return (lo << 32) | hi;
}

static bool has_tbl_ext_ci(const char *name) {
    size_t n = strlen(name);
    if (n < 4) return false;
    char c1 = tolower((unsigned char) name[n - 4]);
    char c2 = tolower((unsigned char) name[n - 3]);
    char c3 = tolower((unsigned char) name[n - 2]);
    char c4 = tolower((unsigned char) name[n - 1]);
    return (c1 == '.' && c2 == 't' && c3 == 'b' && c4 == 'l');
}

static bool get_file_size_win64(const char *path, unsigned long long *out) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER li;
    BOOL ok = GetFileSizeEx(h, &li);
    CloseHandle(h);
    if (!ok) return false;
    *out = (unsigned long long) li.QuadPart;
    return true;
}

void SendFileToServer(const char *tableName) {
    char wire_name[256];
    if (has_tbl_ext_ci(tableName)) {
        snprintf(wire_name, sizeof(wire_name), "%s", tableName);
    } else {
        snprintf(wire_name, sizeof(wire_name), "%s.tbl", tableName);
    }


    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "data/%s", wire_name);


    unsigned long long fsize = 0;
    if (!get_file_size_win64(fullpath, &fsize)) {
        printf("[ERROR] File not found or size failed: %s\n", fullpath);
        return;
    }


    FILE *fp = fopen(fullpath, "rb");
    if (!fp) {
        printf("[ERROR] Cannot open for read: %s\n", fullpath);
        return;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[ERROR] WSAStartup failed\n");
        fclose(fp);
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("[ERROR] Socket creation failed\n");
        fclose(fp);
        WSACleanup();
        return;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("[ERROR] Connection failed\n");
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        return;
    }

    printf("[INFO] Connected. Sending file: %s (%llu bytes)\n", wire_name, fsize);


    unsigned int name_len = (unsigned int) strlen(wire_name);
    unsigned int name_len_net = htonl(name_len);
    if (send(sock, (const char *) &name_len_net, sizeof(name_len_net), 0) != sizeof(name_len_net)) {
        printf("[ERROR] send(name_len) failed\n");
        goto cleanup;
    }


    if (send(sock, wire_name, (int) name_len, 0) != (int) name_len) {
        printf("[ERROR] send(filename) failed\n");
        goto cleanup;
    }


    unsigned long long fsize_net = htonll(fsize);
    if (send(sock, (const char *) &fsize_net, sizeof(fsize_net), 0) != sizeof(fsize_net)) {
        printf("[ERROR] send(file_size) failed\n");
        goto cleanup;
    }


    char *buf = (char *) malloc(BUFFER_SIZE);
    if (!buf) {
        printf("[ERROR] OOM\n");
        goto cleanup;
    }

    size_t rd;
    unsigned long long total = 0;
    while ((rd = fread(buf, 1, BUFFER_SIZE, fp)) > 0) {
        char *p = buf;
        size_t left = rd;
        while (left > 0) {
            int s = send(sock, p, (int) left, 0);
            if (s <= 0) {
                printf("[ERROR] send(body) failed\n");
                free(buf);
                goto cleanup;
            }
            p += s;
            left -= (size_t) s;
            total += (unsigned long long) s;
        }
    }
    free(buf);

    printf("[INFO] Sent successfully (%llu / %llu bytes)\n", total, fsize);

cleanup:
    fclose(fp);
    closesocket(sock);
    WSACleanup();
}
