#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_LINE_LEN 8192
#define HASH_SIZE 1024
#define TOP_N 10

typedef struct UrlEntry {
    char *key;
    long long bytes;
    struct UrlEntry *next;
} UrlEntry;

typedef struct {
    UrlEntry **buckets;
    size_t size;
    pthread_mutex_t mutex;
} UrlMap;

typedef struct RefEntry {
    char *key;
    int count;
    struct RefEntry *next;
} RefEntry;

typedef struct {
    RefEntry **buckets;
    size_t size;
    pthread_mutex_t mutex;
} RefMap;

typedef struct {
    char **files;
    int count;
    int next;
    int finished;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} FileQueue;

static UrlMap url_map;
static RefMap ref_map;
static long long total_bytes = 0;
static pthread_mutex_t total_bytes_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned long hash(const char *str) {
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;
    return h;
}

static void url_map_init(UrlMap *map, size_t size) {
    map->buckets = calloc(size, sizeof(UrlEntry*));
    if (!map->buckets) {
        perror("url_map_init: calloc");
        exit(EXIT_FAILURE);
    }
    map->size = size;
    pthread_mutex_init(&map->mutex, NULL);
}

static void ref_map_init(RefMap *map, size_t size) {
    map->buckets = calloc(size, sizeof(RefEntry*));
    if (!map->buckets) {
        perror("ref_map_init: calloc");
        exit(EXIT_FAILURE);
    }
    map->size = size;
    pthread_mutex_init(&map->mutex, NULL);
}

static void url_map_add(UrlMap *map, const char *key, long long bytes) {
    if (!key || bytes == 0) return;
    unsigned long h = hash(key) % map->size;
    pthread_mutex_lock(&map->mutex);
    UrlEntry *e = map->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            e->bytes += bytes;
            pthread_mutex_unlock(&map->mutex);
            return;
        }
        e = e->next;
    }
    UrlEntry *new_e = malloc(sizeof(UrlEntry));
    if (!new_e) {
        perror("url_map_add: malloc UrlEntry");
        pthread_mutex_unlock(&map->mutex);
        exit(EXIT_FAILURE);
    }
    new_e->key = malloc(strlen(key) + 1);
    if (!new_e->key) {
        perror("url_map_add: malloc key");
        free(new_e);
        pthread_mutex_unlock(&map->mutex);
        exit(EXIT_FAILURE);
    }
    strcpy(new_e->key, key);
    new_e->bytes = bytes;
    new_e->next = map->buckets[h];
    map->buckets[h] = new_e;
    pthread_mutex_unlock(&map->mutex);
}

static void ref_map_inc(RefMap *map, const char *key) {
    if (!key) return;
    unsigned long h = hash(key) % map->size;
    pthread_mutex_lock(&map->mutex);
    RefEntry *e = map->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            e->count++;
            pthread_mutex_unlock(&map->mutex);
            return;
        }
        e = e->next;
    }
    RefEntry *new_e = malloc(sizeof(RefEntry));
    if (!new_e) {
        perror("ref_map_inc: malloc RefEntry");
        pthread_mutex_unlock(&map->mutex);
        exit(EXIT_FAILURE);
    }
    new_e->key = malloc(strlen(key) + 1);
    if (!new_e->key) {
        perror("ref_map_inc: malloc key");
        free(new_e);
        pthread_mutex_unlock(&map->mutex);
        exit(EXIT_FAILURE);
    }
    strcpy(new_e->key, key);
    new_e->count = 1;
    new_e->next = map->buckets[h];
    map->buckets[h] = new_e;
    pthread_mutex_unlock(&map->mutex);
}

static void url_map_free(UrlMap *map) {
    for (size_t i = 0; i < map->size; i++) {
        UrlEntry *e = map->buckets[i];
        while (e) {
            UrlEntry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(map->buckets);
    pthread_mutex_destroy(&map->mutex);
}

static void ref_map_free(RefMap *map) {
    for (size_t i = 0; i < map->size; i++) {
        RefEntry *e = map->buckets[i];
        while (e) {
            RefEntry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(map->buckets);
    pthread_mutex_destroy(&map->mutex);
}

/**
 * Проверка имени файла
 * @param name
 * @return
 */
static int is_log_file(const char *name) {
    const char *logpos = strstr(name, ".log");
    if (!logpos) return 0;
    const char *after = logpos + 4;
    if (*after == '\0') return 1;
    if (*after == '.') {
        const char *p = after + 1;
        if (*p == '\0') return 0;
        while (*p) {
            if (!isdigit(*p)) return 0;
            p++;
        }
        return 1;
    }
    return 0;
}

/**
 * Пропуск пробелов
 * @param p
 * @return
 */
static const char *skip_spaces(const char *p) {
    while (*p == ' ') p++;
    return p;
}

/**
 * Парсинг строки лога в формате Combined Log Format
 * @param line
 * @param url
 * @param bytes
 * @param referer
 * @return
 */
static int parse_log_line(const char *line, char **url, long long *bytes, char **referer) {
    const char *p = line;

    // Пропуск IP
    p = strchr(p, ' ');
    if (!p) return 0;
    p++;

    // Пропуск идентификатора
    p = strchr(p, ' ');
    if (!p) return 0;
    p++;

    // Пропуск userid
    p = strchr(p, '[');
    if (!p) return 0;
    p = strchr(p, '"');
    if (!p) return 0;
    p++;

    // Метод
    const char *method_end = strchr(p, ' ');
    if (!method_end) return 0;

    // URI
    const char *url_start = method_end + 1;
    const char *url_end = strchr(url_start, ' ');
    if (!url_end) return 0;
    size_t url_len = url_end - url_start;

    // Версия протокола
    p = strchr(url_end, '"');
    if (!p) return 0;
    p++;

    // Код ответа
    p = skip_spaces(p);
    if (!isdigit(*p)) return 0;
    p = strchr(p, ' ');
    if (!p) return 0;
    p++; // перед размером ответа

    // Размер ответа
    p = skip_spaces(p);
    if (*p == '-') {
        *bytes = 0;
    } else {
        *bytes = atoll(p);
    }

    // Referer
    p = strchr(p, '"');
    if (!p) return 0;
    p++; // внутри кавычек Referer
    const char *ref_start = p;
    const char *ref_end = strchr(p, '"');
    if (!ref_end) return 0;
    size_t ref_len = ref_end - ref_start;

    *url = malloc(url_len + 1);
    if (!*url) {
        perror("parse_log_line: malloc url");
        return 0;
    }
    memcpy(*url, url_start, url_len);
    (*url)[url_len] = '\0';

    *referer = malloc(ref_len + 1);
    if (!*referer) {
        perror("parse_log_line: malloc referer");
        free(*url);
        *url = NULL;
        return 0;
    }
    memcpy(*referer, ref_start, ref_len);
    (*referer)[ref_len] = '\0';

    return 1;
}

/**
 * Декодирование URI в UTF-8
 * @param src
 * @return
 */
static char *url_decode(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *dst = malloc(len + 1);
    if (!dst) {
        perror("url_decode: malloc");
        return NULL;
    }
    char *out = dst;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '%' && i + 2 < len && isxdigit(src[i+1]) && isxdigit(src[i+2])) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            *out++ = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            *out++ = ' ';
        } else {
            *out++ = src[i];
        }
    }
    *out = '\0';
    return dst;
}

/**
 * Список файлов логов в директории
 * @param dir_path
 * @param count
 * @return
 */
static char **list_files(const char *dir_path, int *count) {
    DIR *d = opendir(dir_path);
    if (!d) {
        perror("list_files: opendir");
        return NULL;
    }
    struct dirent *ent;
    char **files = NULL;
    int n = 0, capacity = 0;
    while ((ent = readdir(d)) != NULL) {
        if (!is_log_file(ent->d_name))
            continue;
        size_t plen = strlen(dir_path) + strlen(ent->d_name) + 2;
        char *fullpath = malloc(plen);
        if (!fullpath) {
            perror("list_files: malloc fullpath");
            continue;
        }
        snprintf(fullpath, plen, "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (n >= capacity) {
                capacity = capacity ? capacity * 2 : 16;
                char **tmp = realloc(files, capacity * sizeof(char*));
                if (!tmp) {
                    perror("list_files: realloc files");
                    free(fullpath);
                    for (int i = 0; i < n; i++) {
                        if (files && files[i]) free(files[i]);
                    }
                    free(files);
                    closedir(d);
                    return NULL;
                }
                files = tmp;
            }
            if (files)
                files[n++] = fullpath;
        } else {
            free(fullpath);
        }
    }
    closedir(d);
    *count = n;
    return files;
}

/**
 * Поток воркера
 * @param arg
 * @return
 */
static void *worker(void *arg) {
    FileQueue *q = (FileQueue*)arg;
    while (1) {
        pthread_mutex_lock(&q->mutex);
        while (q->next >= q->count && !q->finished) {
            pthread_cond_wait(&q->cond, &q->mutex);
        }
        if (q->finished && q->next >= q->count) {
            pthread_mutex_unlock(&q->mutex);
            break;
        }
        int idx = q->next++;
        if (idx == q->count - 1) {
            q->finished = 1;
            pthread_cond_broadcast(&q->cond);
        }
        pthread_mutex_unlock(&q->mutex);

        char *filename = q->files[idx];
        FILE *f = fopen(filename, "r");
        if (!f) {
            perror("worker: fopen");
            continue;
        }
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), f)) {
            char *url = NULL;
            long long bytes = 0;
            char *referer = NULL;
            if (parse_log_line(line, &url, &bytes, &referer)) {
                if (bytes > 0) {
                    pthread_mutex_lock(&total_bytes_mutex);
                    total_bytes += bytes;
                    pthread_mutex_unlock(&total_bytes_mutex);
                    url_map_add(&url_map, url, bytes);
                }
                ref_map_inc(&ref_map, referer);
                free(url);
                free(referer);
            }
        }
        fclose(f);
    }
    return NULL;
}

/**
 * Сортировка вывода для топ url
 * @param a
 * @param b
 * @return
 */
static int cmp_url(const void *a, const void *b) {
    const UrlEntry *ea = *(const UrlEntry**)a;
    const UrlEntry *eb = *(const UrlEntry**)b;
    if (eb->bytes > ea->bytes) return 1;
    if (eb->bytes < ea->bytes) return -1;
    return 0;
}

static int cmp_ref(const void *a, const void *b) {
    const RefEntry *ea = *(const RefEntry**)a;
    const RefEntry *eb = *(const RefEntry**)b;
    return eb->count - ea->count;
}

/**
 * Сбор всех url в массив
 * @param out_count
 * @return
 */
static UrlEntry **collect_urls(int *out_count) {
    UrlEntry **arr = NULL;
    int cnt = 0;
    pthread_mutex_lock(&url_map.mutex);
    for (size_t i = 0; i < url_map.size; i++) {
        UrlEntry *e = url_map.buckets[i];
        while (e) {
            UrlEntry **tmp = realloc(arr, (cnt + 1) * sizeof(UrlEntry*));
            if (!tmp) {
                perror("collect_urls: realloc");
                free(arr);
                pthread_mutex_unlock(&url_map.mutex);
                return NULL;
            }
            arr = tmp;
            arr[cnt++] = e;
            e = e->next;
        }
    }
    pthread_mutex_unlock(&url_map.mutex);
    *out_count = cnt;
    return arr;
}

/**
 * Сбор всех рефералов в массив
 * @param out_count
 * @return
 */
static RefEntry **collect_refs(int *out_count) {
    RefEntry **arr = NULL;
    int cnt = 0;
    pthread_mutex_lock(&ref_map.mutex);
    for (size_t i = 0; i < ref_map.size; i++) {
        RefEntry *e = ref_map.buckets[i];
        while (e) {
            RefEntry **tmp = realloc(arr, (cnt + 1) * sizeof(RefEntry*));
            if (!tmp) {
                perror("collect_refs: realloc");
                free(arr);
                pthread_mutex_unlock(&ref_map.mutex);
                return NULL;
            }
            arr = tmp;
            arr[cnt++] = e;
            e = e->next;
        }
    }
    pthread_mutex_unlock(&ref_map.mutex);
    *out_count = cnt;
    return arr;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <директория логов> <число тредов>\n", argv[0]);
        return 1;
    }
    const char *dir_path = argv[1];
    int num_threads = atoi(argv[2]);
    if (num_threads <= 0) num_threads = 1;

    int file_count = 0;
    char **files = list_files(dir_path, &file_count);
    if (!files) {
        fprintf(stderr, "Невозможно прочитать директорию или в ней нет *.log-файлов\n");
        file_count = 0;
    }

    url_map_init(&url_map, HASH_SIZE);
    ref_map_init(&ref_map, HASH_SIZE);

    FileQueue queue = {
        .files = files,
        .count = file_count,
        .next = 0,
        .finished = 0
    };
    pthread_mutex_init(&queue.mutex, NULL);
    pthread_cond_init(&queue.cond, NULL);

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    if (!threads) {
        perror("main: malloc threads");
        return 1;
    }
    for (int i = 0; i < num_threads; i++)
        pthread_create(&threads[i], NULL, worker, &queue);

    if (file_count == 0) {
        pthread_mutex_lock(&queue.mutex);
        queue.finished = 1;
        pthread_cond_broadcast(&queue.cond);
        pthread_mutex_unlock(&queue.mutex);
    }

    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);
    free(threads);

    printf("Всего байт: %lld\n", total_bytes);

    int url_cnt = 0;
    UrlEntry **urls = collect_urls(&url_cnt);
    if (urls) {
        qsort(urls, url_cnt, sizeof(UrlEntry*), cmp_url);
        printf("\nТоп %d URI по трафику:\n", TOP_N);
        for (int i = 0; i < TOP_N && i < url_cnt; i++) {
            char *decoded = url_decode(urls[i]->key);
            printf("%lld %s\n", urls[i]->bytes, decoded ? decoded : urls[i]->key);
            if (decoded)
                free(decoded);
        }
        free(urls);
    } else {
        printf("\nТоп URI: нет\n");
    }

    int ref_cnt = 0;
    RefEntry **refs = collect_refs(&ref_cnt);
    if (refs) {
        qsort(refs, ref_cnt, sizeof(RefEntry*), cmp_ref);
        printf("\nТоп %d рефералов:\n", TOP_N);
        for (int i = 0; i < TOP_N && i < ref_cnt; i++) {
            printf("%d %s\n", refs[i]->count, refs[i]->key);
        }
        free(refs);
    } else {
        printf("\nТоп рефералов: нет\n");
    }

    if (files) {
        for (int i = 0; i < file_count; i++) free(files[i]);
        free(files);
    }
    pthread_mutex_destroy(&queue.mutex);
    pthread_cond_destroy(&queue.cond);
    url_map_free(&url_map);
    ref_map_free(&ref_map);

    return 0;
}
