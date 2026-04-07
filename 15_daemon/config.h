#ifndef CONFIG_H
#define CONFIG_H

struct config {
    char *file_path;
    char *socket_path;
};

/**
 * Загружает конфигурацию из YAML-файла.
 * @param filename Указатель на строку с путём к файлу конфигурации
 * @return Структура config с выделенными строками, или NULL при ошибке
 */
struct config *load_config(const char *filename);

/**
 * Освобождает память, занятую конфигурацией.
 * @param cfg Указатель на структуру config
 */
void free_config(struct config *cfg);

#endif // CONFIG_H

