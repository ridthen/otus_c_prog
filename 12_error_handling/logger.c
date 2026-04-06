#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <execinfo.h>
#include <unistd.h>

#define MAX_ENV_PREFIX 64
static char env_prefix[MAX_ENV_PREFIX] = "";

typedef struct log_message {
    char *text;                 // полное отформатированное сообщение (включая время, уровень, место)
    struct log_message *next;
} log_message_t;

typedef struct async_logger {
    FILE *file;                 // файл для записи (если используется отдельный поток)
    pthread_t thread;           // поток-писатель
    pthread_mutex_t mutex;      // мьютекс для очереди
    pthread_cond_t cond;        // условная переменная
    log_message_t *queue_head;  // голова очереди
    log_message_t *queue_tail;  // хвост очереди
    int running;                // флаг работы потока
    int level;                  // уровень, которому принадлежит этот асинхронный логгер
} async_logger_t;

static int logging_enabled = 0;
static int current_level = LOG_ERROR;
static FILE *common_file = NULL;
static pthread_mutex_t common_mutex = PTHREAD_MUTEX_INITIALIZER;

static async_logger_t async_loggers[8];

static int initialized = 0;

/**
 * Формирует полное имя переменной окружения с префиксом
 * Например, добавляет "MYAPP_" к именам всех переменным окружения, которые будет считывать библиотека
 * @param suffix Указатель на строку префикса для имени переменной
 * @return
 */
static char* make_env_name(const char *suffix) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s%s", env_prefix, suffix);
    return buffer;
}


/**
 * Возвращает значение env переменной с учётом префикса
 * @param suffix Указатель на строку основной части имени
 * @return Значение переменной
 */
static const char* get_env_with_prefix(const char *suffix) {
    char *name = make_env_name(suffix);
    return getenv(name);
}


/**
 * Проверяет, что заданный уровень логирования существует
 * @param level Уровень логирования
 * @return Существует ли уровень 0/1
 */
static int is_valid_level(const int level) {
    switch (level) {
    case LOG_ERROR:
    case LOG_WARNING:
    case LOG_INFO:
    case LOG_DEBUG:
        return 1;
    default:
        return 0;
    }
}


/**
 * Преобразует строку из переменной LOG_LEVEL в уровень
 * @param str Указатель на строку из LOG_LEVEL
 * @return Уровень важности сообщения по logger_level
 */
static int level_from_string(const char *str) {
    if (str == NULL) return LOG_ERROR;
    if (strcasecmp(str, "error") == 0)   return LOG_ERROR;
    if (strcasecmp(str, "warning") == 0) return LOG_WARNING;
    if (strcasecmp(str, "info") == 0)    return LOG_INFO;
    if (strcasecmp(str, "debug") == 0)   return LOG_DEBUG;
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (*endptr == '\0' && is_valid_level((int)val))
        return (int)val;
    return LOG_ERROR;
}


/**
 * Проверяет env переменную LOGGING_ENABLED на то, включено ли логирование
 * @return 0/1 включено или нет
 */
static int check_logging_enabled(void) {
    const char *env = get_env_with_prefix("LOGGING_ENABLED");
    if (env == NULL) return 0;
    if (strcasecmp(env, "1") == 0 || strcasecmp(env, "true") == 0)
        return 1;
    return 0;
}


/**
 * Форматирование сообщения
 * @param level Уровень логирования
 * @param file Указатель на строку имени файла откуда вызывается (макрос __FILE__)
 * @param line Номер строки вызова (макрос __LINE__)
 * @param func Указатель на функцию, которая вернёт строку с ошибкой
 * @param fmt Указатель на строку с форматом дополнительных аргументов
 * @param args Произвольные дополнительные аргументы
 * @return Указатель на строку сообщения
 */
static char* format_log_message(int level, const char *file, int line, const char *func,
                                const char *fmt, va_list args) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    const char *level_str;
    switch (level) {
        case LOG_ERROR:   level_str = "ERROR"; break;
        case LOG_WARNING: level_str = "WARNING"; break;
        case LOG_INFO:    level_str = "INFO"; break;
        case LOG_DEBUG:   level_str = "DEBUG"; break;
        default:          level_str = "UNKNOWN"; break;
    }

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    char *user_msg = malloc(needed + 1);
    if (user_msg) {
        vsnprintf(user_msg, needed + 1, fmt, args);
    } else {
        user_msg = strdup("<out of memory>");
    }

    char *result;
    asprintf(&result, "%s [%s] %s:%d %s(): %s", time_buf, level_str, file, line, func, user_msg);
    free(user_msg);
    return result;
}


/**
 * Получение строки стека вызовов
 * @return Указатель на строку со стеком вызовов
 */
static char* get_stack_trace_string(void) {
    void *buffer[100];
    int nptrs = backtrace(buffer, sizeof(buffer) / sizeof(buffer[0]));
    char **symbols = backtrace_symbols(buffer, nptrs);
    if (!symbols) return strdup("Ошибка получения стека вызовов\n");

    size_t total_len = 0;
    for (int i = 0; i < nptrs; ++i)
        total_len += strlen(symbols[i]) + 2;
    char *trace = malloc(total_len + 20);
    if (trace) {
        char *p = trace;
        p += sprintf(p, "\nСтек вызовов:\n");
        for (int i = 0; i < nptrs; ++i)
            p += sprintf(p, "  %s\n", symbols[i]);
    }
    free(symbols);
    return trace ? trace : strdup("Стек вызовов недоступен\n");
}


/**
 * Асинхронный поток-писатель
 * @param arg
 * @return
 */
static void* async_writer_thread(void *arg) {
    async_logger_t *logger = (async_logger_t*)arg;
    pthread_mutex_lock(&logger->mutex);
    while (logger->running) {
        if (logger->queue_head == NULL) {
            pthread_cond_wait(&logger->cond, &logger->mutex);
            continue;
        }
        log_message_t *msg = logger->queue_head;
        logger->queue_head = msg->next;
        if (logger->queue_head == NULL)
            logger->queue_tail = NULL;
        pthread_mutex_unlock(&logger->mutex);

        if (logger->file) {
            fprintf(logger->file, "%s", msg->text);
            if (logger->level == LOG_DEBUG) {
                char *stack = get_stack_trace_string();
                fprintf(logger->file, "%s", stack);
                free(stack);
            }
            fprintf(logger->file, "\n");
            fflush(logger->file);
        }
        free(msg->text);
        free(msg);

        pthread_mutex_lock(&logger->mutex);
    }

    while (logger->queue_head) {
        log_message_t *msg = logger->queue_head;
        logger->queue_head = msg->next;
        if (logger->file) {
            fprintf(logger->file, "%s", msg->text);
            if (logger->level == LOG_DEBUG) {
                char *stack = get_stack_trace_string();
                fprintf(logger->file, "%s", stack);
                free(stack);
            }
            fprintf(logger->file, "\n");
            fflush(logger->file);
        }
        free(msg->text);
        free(msg);
    }
    pthread_mutex_unlock(&logger->mutex);
    return NULL;
}


/**
 * Создает асинхронный логгер для уровня (индекс = уровень)
 * @param level Уровень журналирования
 * @param filename Имя файла лога
 * @return Код завершения
 */
static int create_async_logger(int level, const char *filename) {
    if (level < 0 || level >= 8) return -1;
    async_logger_t *logger = &async_loggers[level];
    logger->level = level;
    logger->running = 1;
    logger->queue_head = logger->queue_tail = NULL;
    pthread_mutex_init(&logger->mutex, NULL);
    pthread_cond_init(&logger->cond, NULL);

    logger->file = fopen(filename, "a");
    if (!logger->file) {
        fprintf(stderr, "logger: невозможно открыть '%s' для уровня %d: %s, используя общий файл журнала\n",
                filename, level, strerror(errno));
        return -1;
    }

    if (pthread_create(&logger->thread, NULL, async_writer_thread, logger) != 0) {
        fclose(logger->file);
        logger->file = NULL;
        return -1;
    }
    return 0;
}


/**
 * Закрывает асинхронный логгер
 * @param logger
 */
static void destroy_async_logger(async_logger_t *logger) {
    if (!logger->file) return;
    pthread_mutex_lock(&logger->mutex);
    logger->running = 0;
    pthread_cond_signal(&logger->cond);
    pthread_mutex_unlock(&logger->mutex);
    pthread_join(logger->thread, NULL);
    fclose(logger->file);
    pthread_mutex_destroy(&logger->mutex);
    pthread_cond_destroy(&logger->cond);
    logger->file = NULL;
}


/**
 * Инициализирует библиотеку логирования
 */
static void do_init(void) {
    if (initialized) return;

    logging_enabled = check_logging_enabled();
    if (!logging_enabled) {
        initialized = 1;
        return;
    }

    // Общий файл журнала LOG_FILE
    const char *common_filename = get_env_with_prefix("LOG_FILE");
    if (common_filename && common_filename[0] != '\0') {
        common_file = fopen(common_filename, "a");
        if (!common_file) {
            fprintf(stderr, "logger: невозможно открыть LOG_FILE '%s': %s, будет использован stderr\n",
                    common_filename, strerror(errno));
            common_file = stderr;
        }
    } else {
        common_file = stderr;
    }

    // Уровень журналирования из переменной LOG_LEVEL
    const char *env_level = get_env_with_prefix("LOG_LEVEL");
    if (env_level && env_level[0] != '\0')
        current_level = level_from_string(env_level);

    // Асинхронные логгеры для допустимых уровней (3,4,6,7), если задан файл журнала для уровня
    const int valid_levels[] = {LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG};
    for (int i = 0; i < 4; ++i) {
        int lvl = valid_levels[i];
        const char *suffix = NULL;
        switch (lvl) {
            case LOG_ERROR:   suffix = "ERROR_LOG_FILE"; break;
            case LOG_WARNING: suffix = "WARNING_LOG_FILE"; break;
            case LOG_INFO:    suffix = "INFO_LOG_FILE"; break;
            case LOG_DEBUG:   suffix = "DEBUG_LOG_FILE"; break;
        }
        const char *filename = get_env_with_prefix(suffix);
        if (filename && filename[0] != '\0') {
            create_async_logger(lvl, filename);
        }
    }

    initialized = 1;
}

// Публичные функции

void logger_set_env_prefix(const char *prefix) {
    if (initialized) return;
    if (prefix) {
        strncpy(env_prefix, prefix, MAX_ENV_PREFIX - 1);
        env_prefix[MAX_ENV_PREFIX - 1] = '\0';
    } else {
        env_prefix[0] = '\0';
    }
}


void logger_init(void) {
    pthread_mutex_lock(&common_mutex);
    if (!initialized) {
        do_init();
    }
    pthread_mutex_unlock(&common_mutex);
}


void logger_set_level(int level) {
    if (level == LOG_ERROR || level == LOG_WARNING || level == LOG_INFO || level == LOG_DEBUG) {
        pthread_mutex_lock(&common_mutex);
        current_level = level;
        pthread_mutex_unlock(&common_mutex);
    }
}


void logger_shutdown(void) {
    if (!initialized) return;

    const int valid_levels[] = {LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG};
    for (int i = 0; i < 4; ++i) {
        int lvl = valid_levels[i];
        if (lvl >= 0 && lvl < 8 && async_loggers[lvl].file) {
            destroy_async_logger(&async_loggers[lvl]);
        }
    }

    if (common_file && common_file != stderr) {
        fclose(common_file);
        common_file = NULL;
    }
    initialized = 0;
}


void logger_log(int level, const char *file, int line, const char *func,
                const char *fmt, ...) {
    if (!initialized) {
        pthread_mutex_lock(&common_mutex);
        if (!initialized) do_init();
        pthread_mutex_unlock(&common_mutex);
    }

    if (!logging_enabled) return;
    if (level > current_level) return;

    va_list args;
    va_start(args, fmt);
    char *msg_text = format_log_message(level, file, line, func, fmt, args);
    va_end(args);
    if (!msg_text) return;

    // Если для уровня есть асинхронный логгер и файл открыт
    if (level >= 0 && level < 8 && async_loggers[level].file) {
        async_logger_t *logger = &async_loggers[level];
        log_message_t *msg = malloc(sizeof(log_message_t));
        if (msg) {
            msg->text = msg_text;
            msg->next = NULL;
            pthread_mutex_lock(&logger->mutex);
            if (logger->queue_tail) {
                logger->queue_tail->next = msg;
                logger->queue_tail = msg;
            } else {
                logger->queue_head = logger->queue_tail = msg;
            }
            pthread_cond_signal(&logger->cond);
            pthread_mutex_unlock(&logger->mutex);
        } else {
            free(msg_text);
        }
    } else {
        // Синхронная запись в общий файл
        pthread_mutex_lock(&common_mutex);
        if (common_file) {
            fprintf(common_file, "%s", msg_text);
            if (level == LOG_DEBUG) {
                char *stack = get_stack_trace_string();
                fprintf(common_file, "%s", stack);
                free(stack);
            }
            fprintf(common_file, "\n");
            fflush(common_file);
        }
        pthread_mutex_unlock(&common_mutex);
        free(msg_text);
    }
}
