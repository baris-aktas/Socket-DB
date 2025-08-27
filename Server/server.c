#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <strings.h>

#define PORT 8080
#define BACKLOG 32
#define BUFFER_SIZE 65536
#define MAX_THREADS 256
#define FILENAME_MAXLEN 256
#define DATA_DIR "data"
#define LOGFILE "server.log"

static int ServerFD = -1;
static volatile int ServerStatus = 1;

pthread_mutex_t LogMutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ThreadTableMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    pthread_t ThreadID;
    int ClientFD;
    int Active;
} ClientThread;

static ClientThread ThreadTable[MAX_THREADS];


static void WriteLog(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    pthread_mutex_lock(&LogMutex);
    FILE *lf = fopen(LOGFILE, "a");
    if (lf) {
        time_t t = time(NULL);
        struct tm tm;
        localtime_r(&t, &tm);
        char timestr[64];
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);

        fprintf(lf, "[%s] ", timestr);
        vfprintf(lf, fmt, ap);
        fprintf(lf, "\n");
        fclose(lf);
    }
    pthread_mutex_unlock(&LogMutex);

    va_end(ap);
}

static int DataDirectory(void) {
    struct stat st;
    if (stat(DATA_DIR, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        else return -1;
    }
    return mkdir(DATA_DIR, 0755);
}

static int RecvAll(int fd, void *buf, size_t n) {
    char *p = (char *)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, p + got, n - got, 0);
        if (r == 0) return 0;
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        got += (size_t)r;
    }
    return 1;
}

static uint64_t htonll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)htonl((uint32_t)(v & 0xFFFFFFFFULL))) << 32) | htonl((uint32_t)(v >> 32));
#else
    return v;
#endif
}
static uint64_t ntohll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)ntohl((uint32_t)(v & 0xFFFFFFFFULL))) << 32) | ntohl((uint32_t)(v >> 32));
#else
    return v;
#endif
}

static void SanitizeFilename(char *name) {
    char *base = name;
    for (char *p = name; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    if (base != name) memmove(name, base, strlen(base) + 1);
    name[strcspn(name, "\r\n")] = '\0';
    for (char *p = name; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-')) {
            *p = '_';
        }
    }
}

static void HandleListCommand(int clientFD) {
    DIR *d = opendir(DATA_DIR);
    if (!d) {
        send(clientFD, "NO_TABLES\nEND\n", 14, 0);
        return;
    }
    struct dirent *ent;
    int found = 0;
    char line[512];
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type == DT_REG) {
            snprintf(line, sizeof(line), "%s\n", ent->d_name);
            send(clientFD, line, strlen(line), 0);
            found = 1;
        }
    }
    closedir(d);
    if (!found) {
        send(clientFD, "NO_TABLES\n", 10, 0);
    }
    send(clientFD, "END\n", 4, 0);
}

static void HandleGetCommand(int clientFD, const char *filename) {
    char path[512];
    snprintf(path, sizeof(path), DATA_DIR "/%s", filename);
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        send(clientFD, "ERROR: File not found\n", 23, 0);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char header[64];
    snprintf(header, sizeof(header), "SIZE %ld\n", filesize);
    send(clientFD, header, strlen(header), 0);

    char buf[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        send(clientFD, buf, n, 0);
    }
    fclose(fp);
    send(clientFD, "END\n", 4, 0);
}

static int GetThreadSlot(void) {
    for (int i = 0; i < MAX_THREADS; ++i)
        if (!ThreadTable[i].Active) return i;
    return -1;
}
static void CleanThreadSlot(int idx) {
    if (idx < 0 || idx >= MAX_THREADS) return;
    pthread_mutex_lock(&ThreadTableMutex);
    ThreadTable[idx].Active = 0;
    ThreadTable[idx].ClientFD = -1;
    pthread_mutex_unlock(&ThreadTableMutex);
}

static void *ClientHandler(void *arg) {
    int slot = *(int *)arg;
    free(arg);

    pthread_mutex_lock(&ThreadTableMutex);
    int client_fd = ThreadTable[slot].ClientFD;
    pthread_mutex_unlock(&ThreadTableMutex);

    char peekbuf[5] = {0};
    ssize_t r = recv(client_fd, peekbuf, 4, MSG_PEEK);
    if (r <= 0) {
        close(client_fd);
        CleanThreadSlot(slot);
        return NULL;
    }

    if (strncmp(peekbuf, "LIST", 4) == 0 || strncmp(peekbuf, "GET ", 4) == 0) {
        char cmdline[512];
        r = recv(client_fd, cmdline, sizeof(cmdline) - 1, 0);
        if (r > 0) {
            cmdline[r] = '\0';
            if (strncmp(cmdline, "LIST", 4) == 0) {
                HandleListCommand(client_fd);
            } else if (strncmp(cmdline, "GET ", 4) == 0) {
                char *fname = cmdline + 4;
                fname[strcspn(fname, "\r\n")] = '\0';
                SanitizeFilename(fname);
                HandleGetCommand(client_fd, fname);
            }
        }
        close(client_fd);
        CleanThreadSlot(slot);
        return NULL;
    }

    uint32_t name_len_net;
    int st = RecvAll(client_fd, &name_len_net, sizeof(name_len_net));
    if (st != 1) { close(client_fd); CleanThreadSlot(slot); return NULL; }
    uint32_t name_len = ntohl(name_len_net);
    if (name_len == 0 || name_len >= FILENAME_MAXLEN) { close(client_fd); CleanThreadSlot(slot); return NULL; }

    char filename[FILENAME_MAXLEN];
    memset(filename, 0, sizeof(filename));
    st = RecvAll(client_fd, filename, name_len);
    if (st != 1) { close(client_fd); CleanThreadSlot(slot); return NULL; }
    filename[name_len] = '\0';
    SanitizeFilename(filename);

    uint64_t fsize_net;
    st = RecvAll(client_fd, &fsize_net, sizeof(fsize_net));
    if (st != 1) { close(client_fd); CleanThreadSlot(slot); return NULL; }
    uint64_t fsize = ntohll(fsize_net);

    char path[1024];
    snprintf(path, sizeof(path), DATA_DIR "/%s", filename);
    FILE *fp = fopen(path, "wb");
    if (!fp) { close(client_fd); CleanThreadSlot(slot); return NULL; }

    char *buf = (char *)malloc(BUFFER_SIZE);
    uint64_t remaining = fsize;
    while (remaining > 0) {
        size_t to_read = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : (size_t)remaining;
        ssize_t rr = recv(client_fd, buf, to_read, 0);
        if (rr <= 0) break;
        fwrite(buf, 1, rr, fp);
        remaining -= rr;
    }
    free(buf);
    fclose(fp);

    close(client_fd);
    CleanThreadSlot(slot);
    return NULL;
}

static void ListFiles(void) {
    DIR *d = opendir(DATA_DIR);
    if (!d) {
        printf("No data directory or cannot open '%s'\n", DATA_DIR);
        return;
    }
    printf("Files in %s:\n", DATA_DIR);
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type == DT_REG) {
            printf(" - %s\n", ent->d_name);
        }
    }
    closedir(d);
}

static void PrintLogs(int n) {
    FILE *lf = fopen(LOGFILE, "r");
    if (!lf) {
        printf("No log file.\n");
        return;
    }
    fseek(lf, 0, SEEK_END);
    long sz = ftell(lf);
    fseek(lf, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(lf); return; }
    fread(buf, 1, (size_t)sz, lf);
    buf[sz] = '\0';
    fclose(lf);

    int lines = 0;
    for (long i = sz - 1; i >= 0 && lines < n; --i) {
        if (buf[i] == '\n') lines++;
    }
    char *start = buf;
    if (lines >= n) {
        long i;
        for (i = sz - 1, lines = 0; i >= 0 && lines < n; --i) {
            if (buf[i] == '\n') lines++;
        }
        start = buf + i + 2;
        if (start >= buf + sz) start = buf;
    }
    printf("=== last %d log lines ===\n%s\n", n, start);
    free(buf);
}

static void Shutdown(void) {
    ServerStatus = 0;
    if (ServerFD != -1) close(ServerFD); // break accept()
    WriteLog("Shutdown initiated.");

    printf("Waiting for active client threads to finish...\n");
    for (int i = 0; i < MAX_THREADS; ++i) {
        pthread_mutex_lock(&ThreadTableMutex);
        int active = ThreadTable[i].Active;
        pthread_mutex_unlock(&ThreadTableMutex);
        if (active) {
            pthread_join(ThreadTable[i].ThreadID, NULL);
        }
    }
    printf("All client threads finished. Exiting.\n");
    WriteLog("Server gracefully shutdown.");
}

static void *AcceptLoop(void *arg) {
    (void)arg;
    while (ServerStatus) {
        struct sockaddr_in client_addr;
        socklen_t clientlen = sizeof(client_addr);
        int client_fd = accept(ServerFD, (struct sockaddr *)&client_addr, &clientlen);
        if (client_fd < 0) {
            if (!ServerStatus) break;
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
        WriteLog("Accepted connection from %s (fd=%d)", ipbuf, client_fd);

        int slot = GetThreadSlot();
        if (slot == -1) {
            WriteLog("Too many clients. Rejecting fd=%d", client_fd);
            close(client_fd);
            continue;
        }

        pthread_mutex_lock(&ThreadTableMutex);
        ThreadTable[slot].ClientFD = client_fd;
        ThreadTable[slot].Active = 1;
        pthread_mutex_unlock(&ThreadTableMutex);

        int *pslot = (int *)malloc(sizeof(int));
        *pslot = slot;
        if (pthread_create(&ThreadTable[slot].ThreadID, NULL, ClientHandler, pslot) != 0) {
            WriteLog("pthread_create failed for fd=%d", client_fd);
            close(client_fd);
            CleanThreadSlot(slot);
            free(pslot);
            continue;
        }
    }
    return NULL;
}

int main(void) {
    if (DataDirectory() != 0) {
        fprintf(stderr, "Failed to create data directory '%s'\n", DATA_DIR);
        return 1;
    }

    for (int i = 0; i < MAX_THREADS; ++i) {
        ThreadTable[i].Active = 0;
        ThreadTable[i].ClientFD = -1;
    }

    struct sockaddr_in addr;
    ServerFD = socket(AF_INET, SOCK_STREAM, 0);
    if (ServerFD < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(ServerFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(ServerFD, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(ServerFD);
        return 1;
    }

    if (listen(ServerFD, BACKLOG) < 0) {
        perror("listen");
        close(ServerFD);
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);
    WriteLog("Server started and listening on port %d", PORT);

    pthread_t accept_thread;
    if (pthread_create(&accept_thread, NULL, AcceptLoop, NULL) != 0) {
        perror("pthread_create");
        close(ServerFD);
        return 1;
    }


    char cmd[128];
    while (ServerStatus) {
        printf("\nserver> ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\r\n")] = '\0';

        if (strcasecmp(cmd, "LIST") == 0) {
            ListFiles();
        } else if (strncasecmp(cmd, "LOGS", 4) == 0) {
            int n = 20;
            char *p = strchr(cmd, ' ');
            if (p) n = atoi(p + 1);
            if (n <= 0) n = 20;
            PrintLogs(n);
        } else if (strcasecmp(cmd, "SHUTDOWN") == 0) {
            printf("Shutting down server...\n");
            Shutdown();
            break;
        } else if (strlen(cmd) == 0) {
            continue;
        } else {
            printf("Commands: LIST, LOGS [n], SHUTDOWN\n");
        }
    }

    pthread_join(accept_thread, NULL);
    return 0;
}
