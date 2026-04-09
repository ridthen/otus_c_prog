#include "server.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

static volatile sig_atomic_t keep_running = 1;
static char *g_socket_path = NULL;

static void signal_handler(int sig __attribute__((unused))) {
    keep_running = 0;
}

static void cleanup_socket(void) {
    if (g_socket_path) {
        unlink(g_socket_path);
    }
}

int run_server(const char *socket_path, const char *file_path) {
    g_socket_path = strdup(socket_path);
    if (!g_socket_path) {
        perror("strdup");
        goto except;
    }

    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        goto except;
    }

    unlink(socket_path);

    struct sockaddr_un addr = {0};
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        goto except;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        unlink(socket_path);
        goto except;
    }

    atexit(cleanup_socket);

    while (keep_running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            if (keep_running) perror("accept");
            continue;
        }

        struct stat st = {0};
        char response[256];
        if (stat(file_path, &st) == 0) {
            snprintf(response, sizeof(response), "%lld\n", st.st_size);
        } else {
            snprintf(response, sizeof(response), "error: %s\n", strerror(errno));
        }

        send(client_fd, response, strlen(response), 0);
        close(client_fd);
    }

    close(server_fd);
    unlink(socket_path);
    free(g_socket_path);
    return 0;

except:
    if (g_socket_path)
        free(g_socket_path);
    return -1;
}

