#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <limits.h>
#include <signal.h>
#include <getopt.h>
#include <stddef.h>

#define MAX_CONN 1024
#define RECV_BUF_SIZE 4096
#define SEND_BUF_SIZE 8192
#define MAX_HEADERS 8192
#define MAX_KEVENTS 64

enum conn_state {
    STATE_READING,
    STATE_SENDING_HEADER,
    STATE_SENDING_FILE,
    STATE_CLOSING
};

struct connection {
    int fd;
    enum conn_state state;
    char recv_buf[RECV_BUF_SIZE];
    size_t recv_len;
    int file_fd;
    off_t file_offset;
    size_t file_size;
    char send_buf[SEND_BUF_SIZE];
    size_t send_len;
    size_t send_sent;
};

static struct {
    const char *root_dir;
    const char *listen_addr;
    int kq;
    int listen_fd;
    struct connection conns[MAX_CONN];
    size_t nconns;
} server;

static void cleanup(void) {
    if (server.listen_fd != -1) {
        close(server.listen_fd);
    }
    for (size_t i = 0; i < server.nconns; i++) {
        if (server.conns[i].fd != -1) {
            close(server.conns[i].fd);
        }
        if (server.conns[i].file_fd != -1) {
            close(server.conns[i].file_fd);
        }
    }
    if (server.kq != -1) {
        close(server.kq);
    }
}

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/**
 * Установка неблокирующего режима на сокете
 * @param fd
 */
static int set_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return -1;
    return 0;
}

/**
 * Регистрация интересов
 * @param fd
 * @param filter
 * @param udata
 */
static int add_kqueue_event(int fd, int filter, void *udata) {
    struct kevent ev = {0};
    EV_SET(&ev, fd, filter, EV_ADD | EV_ENABLE, 0, 0, udata);
    if (kevent(server.kq, &ev, 1, NULL, 0, NULL) == -1)
        return -1;
    return 0;
}

static int mod_kqueue_event(int fd, int filter, void *udata) {
    struct kevent ev = {0};
    EV_SET(&ev, fd, filter, EV_ADD | EV_ENABLE, 0, 0, udata);
    if (kevent(server.kq, &ev, 1, NULL, 0, NULL) == -1)
        return -1;
    return 0;
}

static int del_kqueue_event(int fd, int filter) {
    struct kevent ev = {0};
    EV_SET(&ev, fd, filter, EV_DELETE, 0, 0, NULL);
    if (kevent(server.kq, &ev, 1, NULL, 0, NULL) == -1)
        return -1;
    return 0;
}

/**
 * Закрывает соединение с клиентом
 * @param conn
 */
static void close_connection(struct connection *conn) {
    del_kqueue_event(conn->fd, EVFILT_READ);
    del_kqueue_event(conn->fd, EVFILT_WRITE);
    close(conn->fd);
    if (conn->file_fd != -1) {
        close(conn->file_fd);
        conn->file_fd = -1;
    }
    conn->fd = -1;
    conn->state = STATE_CLOSING;
    /* server.nconns уменьшится в основном цикле */
}

static const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    return "application/octet-stream";
}

static void url_decode(char *dst, const char *src, size_t dst_size) {
    char *d = dst;
    const char *ptr = src;
    while (*ptr && d - dst < (ptrdiff_t)dst_size - 1) {
        if (*ptr == '%' && ptr[1] != '\0' && ptr[2] != '\0') {
            char hex[3] = {0};
            strlcpy(hex, ptr + 1, sizeof(hex));
            char *end = NULL;
            errno = 0;
            long val = strtol(hex, &end, 16);
            if (errno == 0 && end == hex + 2 && val >= 0 && val <= 255) {
                *d = (char)val;
                d++;
                ptr += 3;
                continue;
            }
        } else if (*ptr == '+') {
            *d = ' ';
            d++;
            ptr++;
            continue;
        }
        *d = *ptr;
        d++;
        ptr++;
    }
    *d = '\0';
}

static bool is_safe_path(const char *path) {
    if (strstr(path, "..") != NULL) return false;
    return true;
}

static void build_http_status(struct connection *conn, int code,
    const char *status, const char *content_type, size_t content_len) {
    conn->send_len = (size_t)snprintf(conn->send_buf, sizeof(conn->send_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, status, content_type, content_len);
    conn->send_sent = 0;
    conn->state = STATE_SENDING_HEADER;
}

static void send_error(struct connection *conn, int code, const char *status) {
    const char *body = status;
    size_t body_len = strlen(body);
    build_http_status(conn, code, status, "text/plain", body_len);

    if (conn->send_len + body_len < sizeof(conn->send_buf)) {
        memcpy(conn->send_buf + conn->send_len, body, body_len);
        conn->send_len += body_len;
    }
    if (mod_kqueue_event(conn->fd, EVFILT_WRITE, conn) == -1) {
        /* Не удалось зарегистрировать событие — закрываем соединение */
        close_connection(conn);
    }
}

static void handle_request(struct connection *conn) {
    char method[16], path[1024], version[16];
    memset(method, 0, sizeof(method));
    memset(path, 0, sizeof(path));
    memset(version, 0, sizeof(version));
    if (sscanf(conn->recv_buf, "%15s %1023s %15s",
        method, path, version) != 3) {
        send_error(conn, 400, "Bad Request");
        return;
    }
    if (strcmp(method, "GET") != 0) {
        send_error(conn, 405, "Method Not Allowed");
        return;
    }

    char decoded_path[1024];
    memset(decoded_path, 0, sizeof(decoded_path));
    url_decode(decoded_path, path, sizeof(decoded_path));

    if (!is_safe_path(decoded_path)) {
        send_error(conn, 403, "Forbidden");
        return;
    }

    char full_path[PATH_MAX];
    memset(full_path, 0, sizeof(full_path));
    int written = snprintf(full_path, sizeof(full_path), "%s/%s",
        server.root_dir, decoded_path);
    if (written < 0 || (size_t)written >= sizeof(full_path)) {
        send_error(conn, 414, "URI Too Long");
        return;
    }

    struct stat st = {0};
    if (stat(full_path, &st) == -1) {
        if (errno == ENOENT)
            send_error(conn, 404, "Not Found");
        else if (errno == EACCES)
            send_error(conn, 403, "Forbidden");
        else
            send_error(conn, 500, "Internal Server Error");
        return;
    }

    if (!S_ISREG(st.st_mode)) {
        send_error(conn, 404, "Not Found");
        return;
    }

    if (access(full_path, R_OK) == -1) {
        send_error(conn, 403, "Forbidden");
        return;
    }

    int file_fd = open(full_path, O_RDONLY);
    if (file_fd == -1) {
        send_error(conn, 500, "Internal Server Error");
        return;
    }

    conn->file_fd = file_fd;
    conn->file_offset = 0;
    conn->file_size = st.st_size;
    build_http_status(conn, 200, "OK", get_mime_type(full_path),
        st.st_size);
    if (mod_kqueue_event(conn->fd, EVFILT_WRITE, conn) == -1) {
        close_connection(conn);
    }
}

static void handle_read(struct connection *conn) {
    if (conn->fd == -1)
        return;

    /* всё, что клиент имеет прислать */
    ssize_t n = recv(conn->fd, conn->recv_buf + conn->recv_len,
                     sizeof(conn->recv_buf) - conn->recv_len - 1, 0);
    /* если ни чего нет или клиент отвалился */
    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
            close_connection(conn);
        return;
    }
    conn->recv_len += n;
    conn->recv_buf[conn->recv_len] = '\0';

    if (strstr(conn->recv_buf, "\r\n\r\n") != NULL) {
        del_kqueue_event(conn->fd, EVFILT_READ);
        handle_request(conn);
    } else if (conn->recv_len >= sizeof(conn->recv_buf) - 1) {
        send_error(conn, 413, "Payload Too Large");
    }
}

static void handle_write(struct connection *conn) {
    if (conn->fd == -1)
        return;

    if (conn->state == STATE_SENDING_HEADER) {
        ssize_t sent = send(conn->fd, conn->send_buf + conn->send_sent,
                            conn->send_len - conn->send_sent, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            if (errno != EPIPE && errno != ECONNRESET)
                perror("send headers");
            close_connection(conn);
            return;
        }
        conn->send_sent += sent;
        if (conn->send_sent < conn->send_len)
            return;

        if (conn->file_fd != -1) {
            conn->state = STATE_SENDING_FILE;
            conn->send_len = 0;
            conn->send_sent = 0;
        } else {
            close_connection(conn);
            return;
        }
    }

    if (conn->state == STATE_SENDING_FILE) {
        if (conn->send_sent < conn->send_len) {
            ssize_t sent = send(conn->fd, conn->send_buf + conn->send_sent,
                                conn->send_len - conn->send_sent, 0);
            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return;
                if (errno != EPIPE && errno != ECONNRESET)
                    perror("send file chunk");
                close_connection(conn);
                return;
            }
            conn->send_sent += sent;
            if (conn->send_sent < conn->send_len)
                return;
        }

        /* следующий блок файла */
        char buf[SEND_BUF_SIZE];
        ssize_t nr = pread(conn->file_fd, buf, sizeof(buf),
            conn->file_offset);
        if (nr < 0) {
            perror("pread");
            close_connection(conn);
            return;
        }
        if (nr == 0) {
            /* Конец файла */
            close_connection(conn);
            return;
        }

        memcpy(conn->send_buf, buf, nr);
        conn->send_len = nr;
        conn->send_sent = 0;
        conn->file_offset += nr;

        ssize_t sent = send(conn->fd, conn->send_buf, conn->send_len, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            if (errno != EPIPE && errno != ECONNRESET)
                perror("send file chunk");
            close_connection(conn);
            return;
        }
        conn->send_sent = sent;
    }
}

static void accept_connection(void) {
    struct sockaddr_in addr = {0};
    socklen_t addrlen = sizeof(addr);
    int client_fd = accept(server.listen_fd, (struct sockaddr *)&addr,
        &addrlen);
    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
        return;
    }

    if (server.nconns >= MAX_CONN) {
        close(client_fd);
        return;
    }

    /* неблокирующий режим на клиенте */
    if (set_nonblocking(client_fd) == -1) {
        close(client_fd);
        return;
    }

    struct connection *conn = &server.conns[server.nconns];
    server.nconns++;
    memset(conn, 0, sizeof(*conn));
    conn->fd = client_fd;
    conn->state = STATE_READING;
    conn->file_fd = -1;

    if (add_kqueue_event(client_fd, EVFILT_READ, conn) == -1) {
        close(client_fd);
        server.nconns--;
    }
}

static int parse_args(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "d:l:")) != -1) {
        switch (opt) {
            case 'd':
                server.root_dir = optarg;
                break;
            case 'l':
                server.listen_addr = optarg;
                break;
            default:
                fprintf(stderr, "Использование: %s -d <директория>"
                                " -l <адрес:порт>\n", argv[0]);
                goto error;
        }
    }
    if (!server.root_dir || !server.listen_addr) {
        fprintf(stderr, "Отсутствуют необходимые аргументы\n");
        goto error;
    }
    return 0;
error:
    return -1;
}

static int setup_listen_socket(void) {
    char addr_str[64] = {0};
    strlcpy(addr_str, server.listen_addr, sizeof(addr_str));

    char *colon = strrchr(addr_str, ':');
    if (!colon) {
        fprintf(stderr, "Неверный формат адреса. Ожидается адрес:порт\n");
        goto error;
    }
    *colon = '\0';
    const char* port_str = colon + 1;

    char *end;
    long port = strtol(port_str, &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "Неверный порт\n");
        goto error;
    }

    /* Создание сокета сервера */
    server.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.listen_fd == -1)
        goto error;

    int optval = 1;
    if (setsockopt(server.listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
        sizeof(optval)) == -1)
        goto error;

    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, addr_str, &sa.sin_addr) != 1)
        goto error;

    if (bind(server.listen_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
        goto error;
    if (listen(server.listen_fd, SOMAXCONN) == -1)
        goto error;

    if (set_nonblocking(server.listen_fd) == -1)
        goto error;

    return 0;

    error:
        if (server.listen_fd != -1) {
            close(server.listen_fd);
            server.listen_fd = -1;
        }
    return -1;
}

void free_conns(void) {
    size_t j = 0;
    for (size_t i = 0; i < server.nconns; i++) {
        if (server.conns[i].fd != -1) {
            if (i != j)
                server.conns[j] = server.conns[i];
            j++;
        }
    }
    server.nconns = j;
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    atexit(cleanup);

    server.kq = -1;
    server.listen_fd = -1;
    for (size_t i = 0; i < MAX_CONN; i++) {
        server.conns[i].fd = -1;
        server.conns[i].file_fd = -1;
    }

    if (parse_args(argc, argv) == -1)
        exit(EXIT_FAILURE);

    if (setup_listen_socket() == -1)
        die("setup_listen_socket");

    server.kq = kqueue();
    if (server.kq == -1)
        die("kqueue");

    if (add_kqueue_event(server.listen_fd, EVFILT_READ, NULL) == -1)
        die("add_kqueue_event");

    struct kevent events[MAX_KEVENTS] = {0};
    while (true) {
        /* ожидание и обработка готовых событий */
        int nev = kevent(server.kq, NULL, 0, events,
            MAX_KEVENTS, NULL);
        if (nev == -1) {
            if (errno == EINTR) continue;
            die("kevent");
        }

        for (int i = 0; i < nev; i++) {
            struct kevent *ev = &events[i];
            if (ev->ident == (uintptr_t)server.listen_fd) {
                accept_connection(); /* регистрация новых событий */
            } else {
                struct connection *conn = ev->udata;
                if (conn == NULL) continue;
                if (ev->filter == EVFILT_READ) {
                    handle_read(conn);
                } else if (ev->filter == EVFILT_WRITE) {
                    handle_write(conn);
                }
            }
        }

        /* удаление закрытых соединений */
        free_conns();
    }

    exit(EXIT_SUCCESS);
}
