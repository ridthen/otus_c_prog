#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUF_SIZE 8192
#define HOST "telehack.com"
#define PORT "23"

static void send_all(int fd, const char *buf, size_t len) {
    ssize_t n;
    while (len > 0) {
        n = write(fd, buf, len);
        if (n <= 0) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        buf += n;
        len -= (size_t)n;
    }
}

static void print_usage(const char *prog) {
    static const char msg[] =
        "Использование: %s [-f шрифт] текст\n"
        "  -f шрифт   установка figlet-шрифта (по умолчанию: standard)\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    char buf[256];
    memset(buf, 0, sizeof(buf));
    int len = snprintf(buf, sizeof(buf), msg, prog);
    write(STDERR_FILENO, buf, (size_t)len);
}

int main(int argc, char *argv[]) {
    const char *font = "standard";
    int opt;
    while ((opt = getopt(argc, argv, "f:")) != -1) {
        switch (opt) {
            case 'f':
                font = optarg;
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    char text[BUF_SIZE / 2];
    memset(text, 0, sizeof(text));
    size_t text_len = 0;
    for (int i = optind; i < argc && text_len < sizeof(text) - 2; ++i) {
        if (i > optind) {
            text[text_len++] = ' ';
        }
        size_t arg_len = strlen(argv[i]);
        if (text_len + arg_len >= sizeof(text)) {
            arg_len = sizeof(text) - text_len - 1;
        }
        memcpy(text + text_len, argv[i], arg_len);
        text_len += arg_len;
    }
    text[text_len] = '\0';

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res;
    int gai_err = getaddrinfo(HOST, PORT, &hints, &res);
    if (gai_err != 0) {
        char errbuf[256];
        snprintf(errbuf, sizeof(errbuf), "getaddrinfo: %s",
            gai_strerror(gai_err));
        perror(errbuf);
        return EXIT_FAILURE;
    }

    int sock = -1;
    struct addrinfo *rp = {0};
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1)
            continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);
    if (sock == -1) {
        perror("connect");
        return EXIT_FAILURE;
    }

    /* Отправка ввода */
    send_all(sock, "\r\n", 2);

    char buf[BUF_SIZE];
    memset(buf, 0, sizeof(buf));
    ssize_t n = 0;
    int found_prompt = 0;

    /* Промпт - точка в пустой строке */
    while (!found_prompt && (n = read(sock, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        if (strstr(buf, ".\r\n") != NULL)
            found_prompt = 1;
    }
    if (n <= 0) {
        perror("read");
        close(sock);
        return EXIT_FAILURE;
    }

    /* Формирование команды figlet */
    char cmd[BUF_SIZE];
    memset(cmd, 0, sizeof(cmd));
    int cmd_len = snprintf(cmd, sizeof(cmd), "figlet /%s %s\r\n", font, text);
    if (cmd_len < 0 || (size_t)cmd_len >= sizeof(cmd)) {
        static const char err[] = "Длина текстовой строки более допустимой\n";
        write(STDERR_FILENO, err, sizeof(err) - 1);
        close(sock);
        return EXIT_FAILURE;
    }
    send_all(sock, cmd, (size_t)cmd_len);

    /* Чтение вывода с аски арт до нового промпта */
    found_prompt = 0;
    while (!found_prompt && (n = read(sock, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        char *prompt_pos = strstr(buf, "\r\n.");
        if (prompt_pos != NULL) {
            found_prompt = 1;
            /* Сам новый промпт не нужен */
            size_t prompt_index = (size_t)(prompt_pos - buf);
            buf[prompt_index] = '\n';
            buf[prompt_index+1] = '\0';
            write(STDOUT_FILENO, buf, prompt_index+1);
        } else {
            write(STDOUT_FILENO, buf, (size_t)n);
        }
    }
    if (n < 0) {
        perror("read");
        close(sock);
        return EXIT_FAILURE;
    }

    close(sock);
    return EXIT_SUCCESS;
}
