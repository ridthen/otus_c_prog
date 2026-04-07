#ifndef SERVER_H
#define SERVER_H

/**
 * Запускает серверный цикл.
 * @param socket_path Путь к UNIX-сокету
 * @param file_path   Путь к файлу, размер которого отслеживается
 * @return 0 при успешной работе (после остановки), -1 при ошибке
 */
int run_server(const char *socket_path, const char *file_path);

#endif // SERVER_H

