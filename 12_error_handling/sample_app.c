#include "logger.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <_stdlib.h>

#define NUM_THREADS 4

void* thread_function(void* arg) {
    int id = *(int*)arg;
    if (id == 0)
    {
        errno = EAGAIN;
    }
    for (int i = 0; i < 3; ++i) {
        log_debug("Thread %d: debug итерация %d", id, i);
        log_info("Thread %d: сообщение info", id);
        log_warning("Thread %d: предупреждение", id);
        // errno 35: Resource temporarily unavailable должно появиться только в thread 0
        log_error("Thread %d: симуляция ошибки (errno %d: %s)", id, errno, strerror(errno));
        usleep(100000);  // 100 мс
    }
    return NULL;
}

// Демонстрация вложенных вызовов для стека
void deep_function(int depth) {
    if (depth <= 0) {
        log_debug("Ошибка на уровне 0 (errno: %s)", strerror(EPERM));
        return;
    }
    log_debug("Уровень рекурсии %d", depth);
    deep_function(depth - 1);
    log_debug("Уровень рекурсии %d", depth);
}

int main(void) {
    logger_set_env_prefix("MYAPP_");
    setenv("MYAPP_LOGGING_ENABLED", "1", 1);
    setenv("MYAPP_LOG_LEVEL", "debug", 1);
    setenv("MYAPP_DEBUG_LOG_FILE", "debug.log", 1);
    // setenv("MYAPP_LOG_FILE", "myapp.log", 1);
    // setenv("MYAPP_DEBUG_LOG_FILE", "debug.log", 1);
    // log_debug("Это debug сообщение запишется в debug.log");
    // log_info("Это info сообщение запишется в myapp.log");

    logger_init();

    log_info("Приложение запущено. Логирование включено");

    log_info("Тест стека для рекурсивной функции");
    deep_function(2);

    log_info("Многопоточный тест");
    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; ++i) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, thread_function, &ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    log_info("Проверка динамического изменения уровня логирования");
    logger_set_level(LOG_WARNING);
    log_debug("Это debug сообщение не должно появиться..");
    log_info("Это info сообщение не должно появиться..");
    log_warning("Это warning сообщение появиться..");
    log_error("Это error сообщение появиться..");


    log_info("Тест завершен");
    logger_shutdown();
    return 0;
}
