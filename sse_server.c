#include "sse_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <sys/un.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define UNIX_SOCKET_PATH "/tmp/sse_ipc.sock"

static int server_fd = -1;
static int client_sockets[MAX_CLIENTS];
static int client_count = 0;
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_sigint(int sig) {
    if (server_fd != -1) {
        close(server_fd);
    }
    unlink(UNIX_SOCKET_PATH);
    printf("\nServer stopped.\n");
    exit(0);
}

void* client_handler(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    // Giữ kết nối cho đến khi client ngắt
    while (1) {
        sleep(1);
    }
    return NULL;
}

void sse_send_message(const char* msg) {
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; ++i) {
        char event[BUFFER_SIZE];
        snprintf(event, sizeof(event), "data: %s\n\n", msg);
        if (send(client_sockets[i], event, strlen(event), 0) <= 0) {
            close(client_sockets[i]);
            for (int j = i; j < client_count - 1; ++j) {
                client_sockets[j] = client_sockets[j+1];
            }
            --client_count;
            --i;
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

void* accept_thread_func(void* arg) {
    int port = *(int*)arg;
    free(arg);
    int new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    signal(SIGINT, handle_sigint);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("SSE server listening on port %d...\n", port);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        char *headers = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/event-stream\r\n"
                         "Cache-Control: no-cache\r\n"
                         "Connection: keep-alive\r\n"
                         "Access-Control-Allow-Origin: *\r\n"
                         "\r\n";
        send(new_socket, headers, strlen(headers), 0);
        pthread_mutex_lock(&client_mutex);
        if (client_count < MAX_CLIENTS) {
            client_sockets[client_count++] = new_socket;
            int* pclient = malloc(sizeof(int));
            *pclient = new_socket;
            pthread_t tid;
            pthread_create(&tid, NULL, client_handler, pclient);
            pthread_detach(tid);
        } else {
            close(new_socket);
        }
        pthread_mutex_unlock(&client_mutex);
    }
    return NULL;
}

void* uds_ipc_thread_func(void* arg) {
    int uds_fd, client_fd;
    struct sockaddr_un addr;
    char buf[BUFFER_SIZE];

    unlink(UNIX_SOCKET_PATH); // Xóa socket cũ nếu có
    uds_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (uds_fd < 0) {
        perror("socket(AF_UNIX)");
        pthread_exit(NULL);
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (bind(uds_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind(UNIX)");
        close(uds_fd);
        pthread_exit(NULL);
    }
    if (listen(uds_fd, 5) < 0) {
        perror("listen(UNIX)");
        close(uds_fd);
        pthread_exit(NULL);
    }
    printf("SSE IPC UNIX socket listening at %s\n", UNIX_SOCKET_PATH);
    while (1) {
        client_fd = accept(uds_fd, NULL, NULL);
        if (client_fd < 0) continue;
        ssize_t n = read(client_fd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = '\0';
            // Xóa ký tự newline cuối nếu có
            size_t len = strlen(buf);
            if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
            sse_send_message(buf);
        }
        close(client_fd);
    }
    close(uds_fd);
    unlink(UNIX_SOCKET_PATH);
    return NULL;
}

void start_sse_server(int port) {
    signal(SIGPIPE, SIG_IGN); // Chặn SIGPIPE để không bị kill khi gửi tới socket đã đóng
    pthread_t accept_thread, uds_thread;
    int* pport = malloc(sizeof(int));
    *pport = port;
    pthread_create(&accept_thread, NULL, accept_thread_func, pport);
    pthread_detach(accept_thread);
    pthread_create(&uds_thread, NULL, uds_ipc_thread_func, NULL);
    pthread_detach(uds_thread);
}
