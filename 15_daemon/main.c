#include "config.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/errno.h>


static char *pid_path_global = NULL;

static int check_pid_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char buf[32];
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);

    char *endptr;
    errno = 0;
    long pid_long = strtol(buf, &endptr, 10);
    if (errno != 0 || endptr == buf || (*endptr != '\n' && *endptr != '\0') || pid_long <= 0 || pid_long > INT_MAX) {
        fprintf(stderr, "PID-файл %s содержит некорректное значение\n", path);
        return -1;
    }

    pid_t pid = (pid_t)pid_long;
    if (kill(pid, 0) == 0) {
        fprintf(stderr, "Процесс с PID %d уже запущен (PID-файл %s)\n", pid, path);
        return -1;
    }

    if (errno == ESRCH) {
        fprintf(stderr, "PID-файл %s существует, но процесс не запущен. Удаляем PID-файл.\n", path);
        unlink(path);
        return 0;
    }

    fprintf(stderr, "Ошибка проверки процесса %d: %s (файл %s)\n", pid, strerror(errno), path);
    return -1;
}


static void create_pid_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("Не удалось создать PID-файл");
        exit(EXIT_FAILURE);
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);
}


static void remove_pid_file(void) {
    if (pid_path_global) unlink(pid_path_global);
}


static void print_usage(const char *progname) {
    fprintf(stderr, "Использование: %s [-d] [-c config_file]\n", progname);
    fprintf(stderr, "  -d                Демонизировать\n");
    fprintf(stderr, "  -c config_file    Путь к файлу конфигурации (по умолчанию: ./config.yaml)\n");
}


static void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        goto except;
    }
    if (pid > 0) _exit(EXIT_SUCCESS);

    if (setsid() < 0) {
        perror("setsid");
        goto except;
    }

    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) {
        perror("fork2");
        goto except;
    }
    if (pid > 0) _exit(EXIT_SUCCESS);

    if (chdir("/") < 0) {
        perror("chdir");
        goto except;
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDWR);
    dup(0); // туда же
    dup(0);
    return;

except:
    _exit(EXIT_FAILURE);
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

    if (check_pid_file(cfg->pid_path) != 0) {
        free_config(cfg);
        exit(EXIT_FAILURE);
    }

    pid_path_global = strdup(cfg->pid_path);


    if (daemonize_flag) {
        daemonize();
        openlog("filesizer", LOG_PID, LOG_DAEMON);
        syslog(LOG_INFO, "Демон запущен, socket=%s, file=%s", cfg->socket_path, cfg->file_path);
    } else {
        printf("Сервер запущен. Сокет: %s, Отслеживаемый файл: %s\n", cfg->socket_path, cfg->file_path);
    }

    create_pid_file(cfg->pid_path);
    atexit(remove_pid_file);

    int ret = run_server(cfg->socket_path, cfg->file_path);

    if (daemonize_flag) {
        syslog(LOG_INFO, "Демон остановлен");
        closelog();
    } else {
        printf("Сервер остановлен.\n");
    }

    free_config(cfg);
    exit(ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
