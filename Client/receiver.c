#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include "functions.h"
#include "database.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP       "YOUR_SERVER_IP"
#define SERVER_PORT     8080
#define BUFFER_SIZE     65536
#define LOCAL_DATA_DIR  "data"


static void LocalDataDirectory(void) {
    CreateDirectoryA(LOCAL_DATA_DIR, NULL);
}

static int ServerConnection(SOCKET *out_sock) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[ERROR] WSAStartup failed\n");
        return -1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("[ERROR] Socket creation failed\n");
        WSACleanup();
        return -1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("[ERROR] Connection failed\n");
        closesocket(sock);
        WSACleanup();
        return -1;
    }

    *out_sock = sock;
    return 0;
}


static int LineRecv(SOCKET s, char *buf, int buflen) {
    int total = 0;
    while (total < buflen - 1) {
        char ch;
        int r = recv(s, &ch, 1, 0);
        if (r == 0) {
            // peer closed
            break;
        }
        if (r < 0) {
            return -1;
        }
        buf[total++] = ch;
        if (ch == '\n') break;
    }
    buf[total] = '\0';
    return total;
}

static void WireName(const char *name_in, char *out, size_t out_sz) {
    size_t n = strlen(name_in);
    if (n >= 4 && _stricmp(name_in + n - 4, ".tbl") == 0) {
        snprintf(out, out_sz, "%s", name_in);
    } else {
        snprintf(out, out_sz, "%s.tbl", name_in);
    }
}


void ListTablesFromServer(void) {
    SOCKET sock;
    if (ServerConnection(&sock) != 0) return;

    const char *cmd = "LIST\n";
    if (send(sock, cmd, (int) strlen(cmd), 0) < 0) {
        printf("[ERROR] Failed to send LIST command\n");
        closesocket(sock);
        WSACleanup();
        return;
    }

    char buf[BUFFER_SIZE + 1];
    int n;
    int recvAny = 0;

    while ((n = recv(sock, buf, BUFFER_SIZE, 0)) > 0) {
        buf[n] = '\0';
        char *endMarker = strstr(buf, "END\n");
        if (endMarker) {
            *endMarker = '\0';
            printf("%s\n", buf);
            recvAny = 1;
            break;
        }
        printf("%s", buf);
        recvAny = 1;
    }

    if (!recvAny) {
        printf("No tables received or error.\n");
    }

    closesocket(sock);
    WSACleanup();
}


Table *LoadTableFromServer(const char *filename) {
    if (!filename || !*filename) {
        printf("[ERROR] Invalid filename\n");
        return NULL;
    }

    char wire_name[256];
    WireName(filename, wire_name, sizeof(wire_name));

    SOCKET sock;
    if (ServerConnection(&sock) != 0) return NULL;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "GET %s\n", wire_name);
    if (send(sock, cmd, (int) strlen(cmd), 0) < 0) {
        printf("[ERROR] Failed to send GET command\n");
        closesocket(sock);
        WSACleanup();
        return NULL;
    }

    char header[128];
    int hl = LineRecv(sock, header, (int) sizeof(header));
    if (hl <= 0) {
        printf("[ERROR] Failed to receive file size header\n");
        closesocket(sock);
        WSACleanup();
        return NULL;
    }

    if (strncmp(header, "ERROR", 5) == 0) {
        header[strcspn(header, "\r\n")] = '\0';
        printf("[ERROR] Server: %s\n", header);
        closesocket(sock);
        WSACleanup();
        return NULL;
    }

    long long fsize = -1;
    if (sscanf(header, "SIZE %lld", &fsize) != 1 || fsize < 0) {
        header[strcspn(header, "\r\n")] = '\0';
        printf("[ERROR] Bad SIZE header: '%s'\n", header);
        closesocket(sock);
        WSACleanup();
        return NULL;
    }

    LocalDataDirectory();

    char localpath[512];
    snprintf(localpath, sizeof(localpath), "%s\\%s", LOCAL_DATA_DIR, wire_name);

    FILE *fp = fopen(localpath, "wb");
    if (!fp) {
        perror("[ERROR] fopen local path");
        closesocket(sock);
        WSACleanup();
        return NULL;
    }

    char buf[BUFFER_SIZE];
    long long remaining = fsize;

    while (remaining > 0) {
        int to_read = (int) ((remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining);
        int r = recv(sock, buf, to_read, 0);
        if (r <= 0) {
            printf("[ERROR] Download interrupted\n");
            fclose(fp);
            closesocket(sock);
            WSACleanup();
            return NULL;
        }
        size_t wrote = fwrite(buf, 1, (size_t) r, fp);
        if ((int) wrote != r) {
            printf("[ERROR] Write failed\n");
            fclose(fp);
            closesocket(sock);
            WSACleanup();
            return NULL;
        }
        remaining -= r;
    }

    char tail[4];
    int tgot = 0;
    while (tgot < 4) {
        int r = recv(sock, tail + tgot, 4 - tgot, 0);
        if (r <= 0) break;
        tgot += r;
        if (tgot >= 4) break;
    }

    if (tgot == 4 && !(tail[0] == 'E' && tail[1] == 'N' && tail[2] == 'D' && tail[3] == '\n')) {
    }

    fclose(fp);
    closesocket(sock);
    WSACleanup();

    printf("[INFO] Downloaded '%s' (%lld bytes)\n", wire_name, fsize);

    Table *tbl = LoadTableFromFile(localpath);
    if (!tbl) {
        printf("[ERROR] Failed to parse table after download: %s\n", localpath);
        return NULL;
    }
    return tbl;
}
