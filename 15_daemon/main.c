#include "config.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>

static void print_usage(const char *progname) {
    fprintf(stderr, "Использование: %s [-d] [-c config_file]\n", progname);
    fprintf(stderr, "  -d                Демонизировать\n");
    fprintf(stderr, "  -c config_file    Путь к файлу конфигурации (по умолчанию: ./config.yaml)\n");
}

static void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) {
        perror("setsid");
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) {
        perror("fork2");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) exit(EXIT_SUCCESS);

    if (chdir("/") < 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDWR);
    dup(0); // туда же
    dup(0);
}

int main(int argc, char *argv[]) {
    int daemonize_flag = 0;
    const char *config_file = "config.yaml";
    int opt;

    while ((opt = getopt(argc, argv, "dch:")) != -1) {
        switch (opt) {
            case 'd':
                daemonize_flag = 1;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    struct config *cfg = load_config(config_file);
    if (!cfg) {
        fprintf(stderr, "Ошибка чтения конфигурационного файла %s\n", config_file);
        exit(EXIT_FAILURE);
    }

    if (daemonize_flag) {
        daemonize();
        openlog("filesizer", LOG_PID, LOG_DAEMON);
        syslog(LOG_INFO, "Демон запущен, socket=%s, file=%s", cfg->socket_path, cfg->file_path);
    } else {
        printf("Сервер запущен. Сокет: %s, Отслеживаемый файл: %s\n", cfg->socket_path, cfg->file_path);
    }

    int ret = run_server(cfg->socket_path, cfg->file_path);

    if (daemonize_flag) {
        syslog(LOG_INFO, "Демон остановлен");
        closelog();
    } else {
        printf("Сервер остановлен.\n");
    }

    free_config(cfg);
    return ret;
}
