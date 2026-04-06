#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Уровни журналирования по RFC 5424
enum logger_level {
    LOG_ERROR   = 3,
    LOG_WARNING = 4,
    LOG_INFO    = 6,
    LOG_DEBUG   = 7
};

/**
 * Устанавливает префикс для переменных окружения
 * @param prefix Указатель на строку префикса
 */
void logger_set_env_prefix(const char *prefix);

/**
 * Инициализация библиотеки logger
 * Читает переменные окружения с учётом префикса из logger_set_env_prefix.
 */
void logger_init(void);

/**
 * Устанавливает текущий уровнь важности (переопределяет значение из окружения)
 * @param level Номер уровня логирования из enum logger_level
 */
void logger_set_level(int level);

/**
 * Основная функция журналирования. Вызывается через макрос.
 * @param level Уровень логирования
 * @param file Указатель на строку имени файла откуда вызывается (макрос __FILE__)
 * @param line Номер строки вызова (макрос __LINE__)
 * @param func Указатель на функцию, которая вернёт строку с ошибкой
 * @param fmt Указатель на строку с форматом дополнительных аргументов
 * @param ... Произвольные дополнительные аргументы
 */
void logger_log(int level, const char *file, int line, const char *func,
                const char *fmt, ...);

/**
 * Завершение работы: останавливает асинхронные потоки и закрывает файлы
 */
void logger_shutdown(void);

#define log_error(...)   logger_log(LOG_ERROR,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_warning(...) logger_log(LOG_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_info(...)    logger_log(LOG_INFO,    __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_debug(...)   logger_log(LOG_DEBUG,   __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
