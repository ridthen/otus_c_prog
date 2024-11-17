#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Реализация хэш-таблицы с открытой адресацией со строками в качестве ключей и целыми числами в качестве значений.
 * Приложение подсчитывающее частоту слов в заданном файле.
 */

#define PERROR_IF(cond, msg) if (cond) { perror(msg); exit(1); }

/**
 * Функция-обертка для malloc с проверкой возвращаемого указателя на NULL
 * @param size Размер выделяемой памяти, байт
 * @return void указатель на выделенную область памяти
 */
static void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    PERROR_IF(ptr == NULL, "malloc");
    return ptr;
}

/**
 * Функция-обертка для realloc с проверкой возвращаемого значения на NULL
 * @param ptr void указатель для расширяемой области памяти
 * @param size новый размер выделяемой памяти, байт
 * @return void указатель на выделенную область памяти
 */
static void *xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    PERROR_IF(ptr == NULL, "realloc");
    return ptr;
}

/**
 * Функция-обертка для calloc с проверкой возвращаемого значения на NULL
 * @param nmemb количество элементов
 * @param size размер памяти для одного элемента
 * @return void указатель на выделенную область памяти
 */
static void *xcalloc(size_t nmemb, size_t size)
{
    void *ptr = calloc(nmemb, size);
    PERROR_IF(ptr == NULL, "realloc");
    return ptr;
}

/**
 * Считывает данные из файла
 * @param filename имя файла, который необходимо прочитать
 * @param file_sz указатель на переменную, в которую будет записан размер прочитанного файла
 * @return указатель на считанные из файла данные
 */
static char *read_file(const char *filename, size_t *file_sz)
{
    FILE *f;
    char *buf;
    size_t buf_cap;

    f = fopen(filename, "rb");
    PERROR_IF(f == NULL, "fopen");

    buf_cap = 4096;
    buf = xmalloc(buf_cap);

    *file_sz = 0;
    while (feof(f) == 0) {
        if (buf_cap - *file_sz == 0) {
            buf_cap *= 2;
            buf = xrealloc(buf, buf_cap);
        }

        *file_sz += fread(&buf[*file_sz], 1, buf_cap - *file_sz, f);
        PERROR_IF(ferror(f), "fread");
    }

    PERROR_IF(fclose(f) != 0, "fclose");
    return buf;
}

/**
 * Печатает сообщение о способе запуска программы
 * @param argv0 название файла вызываемой программы
 */
static void print_usage(const char *argv0)
{
    printf("Counts the frequency of words in a file\n\n");
    printf("Usage:\n\n");
    printf("  %s <input_file>\n", argv0);
    printf("\n");
}

static const float LOAD_FACTOR = 0.5f;
static const size_t INITIAL_SIZE = 100;
static const float MULTIPLIER = 2.0f;

/**
 * Тип данных ключа элемента карты
 */
typedef char* K;

/**
 * Тип данных значения элемента карты
 */
typedef size_t* V;

/**
 * Пара <ключ значение> элемента карты
 */
typedef struct Entry {
    K key;
    V value;
} Entry;

/**
 * Удаляет объект Entry и высвобождает память
 * @param e объект структуры Entry
 */
void freeEntry(Entry **e) {
    free((*e)->key);
    free((*e)->value);
    free(*e);
    // *e = NULL
}

// Сравнение ключей
#define CMP_EQ(a, b) (strcmp((a), (b)) == 0)
// Нахождение хешкода
#define HASHCODE(a) (hashcode(a))
// Удаление пары
#define FREE_ENTRY(a) (freeEntry(a))

/**
 * Нахождение хеш-кода строки
 * @param str строка
 * @return числовое значение хеш-кода
 */
unsigned long long hashcode(unsigned char *str) {
    unsigned long long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) ^ c;
    }
    return hash;
}

/**
 * Структура карты с открытой адресацией
 */
typedef struct Hashmap {
    Entry **data;       // массив корзин
    size_t size;        // количество элементов в карте
    size_t arr_size;    // размер массива корзин
    size_t limit;       // целочисленное значение количества элементов карты при превышении которого происходит пересборка
    float loadFactor;   // процентное заполнение, при превышении которого происходит пересборка
    float multiplier;   // во сколько раз увеличится размер карты при пересборке
} Hashmap;


Hashmap* rehashUp(Hashmap **_map_, Entry* e);

/**
 * Создает экземпляр карты с открытой адресацией
 * @param limit целочисленное значение количества элементов карты при превышении которого происходит пересборка
 * @param loadFactor процентное заполнение, при превышении которого происходит пересборка
 * @param multiplier во сколько раз увеличится размер карты при пересборке
 * @return Hashmap*
 */
Hashmap* createHashmap(size_t limit, float loadFactor, float multiplier) {
    Hashmap *tmp = (Hashmap*) xmalloc(sizeof(Hashmap));
    tmp->arr_size = (limit >= INITIAL_SIZE) ? limit : INITIAL_SIZE;
    tmp->loadFactor = (loadFactor >= LOAD_FACTOR && loadFactor <= 1.0f) ? loadFactor : LOAD_FACTOR;
    tmp->limit = (int)(tmp->loadFactor * tmp->arr_size);
    tmp->size = 0;
    tmp->multiplier = (multiplier >= MULTIPLIER) ? multiplier : MULTIPLIER;

    tmp->data = (Entry**) xcalloc(tmp->arr_size, sizeof(Entry*));
    return tmp;
}

/**
 * Помещяет элемент e в карту _map_. Вычисляет хеш-код ключа и ищет индекс, куда поместить элемент.
 * Увеличивает счетчик количества элементов карты. Если количество элементов карты выше лимита,
 * вызывает пересборку карты.
 * @param _map_ указатель на карту
 * @param e указатель на экземпляр элемента Entry
 */
void raw_put(Hashmap **_map_, Entry *e) {
    // вспомогательный указатель
    Hashmap *map = *_map_;
    // находим хеш и положение в массиве
    unsigned long long hash = HASHCODE(e->key);
    size_t index =  (hash % map->arr_size);
    if (map->size <= map->limit) {
        if (map->data[index] == NULL) {
            map->data[index] = e;
        } else {
            while (map->data[index]) {
                index++;
                if (index >= map->arr_size) {
                    index = 0;
                }
            }
            map->data[index] = e;
        }
        (*_map_)->size++;
    } else {
        *_map_ = rehashUp(_map_, e);
    }
}

/**
 * Помещает новый элемент в карту. Создает новый объект Entry и вызывает raw_put для его размещения в карте.
 * @param map указатель на объект Hashmap
 * @param key ключ нового элемента
 * @param value значение нового элемента
 */
void put(Hashmap **map, K key, V value) {
    Entry *e = (Entry*) xmalloc(sizeof(Entry));
    e->key = key;
    e->value = value;
    raw_put(map, e);
}

/**
 * Создает новую карту, если размер предыдущей выше установленного лимита. Удаляет прежнюю карту.
 * Вставляет новый переданный элемент в созданную карту.
 * @param _map_ Карта, размер которой необходимо увеличить
 * @param e Элемент Entry, который необходимо добавить в новую увеличенную карту
 * @return Новая карта Hashmap
 */
Hashmap* rehashUp(Hashmap **_map_, Entry* e) {
    Hashmap *newMap = createHashmap((size_t)(*_map_)->arr_size * (*_map_)->multiplier,
                                    (*_map_)->loadFactor,
                                    (*_map_)->multiplier);
    size_t i, size;
    Hashmap *map = (*_map_);
    size = (*_map_)->arr_size;
    for (i = 0; i < size; i++) {
        // не создаем новых пар, вставляем прежние
        if (map->data[i]) {
            raw_put(&newMap, map->data[i]);
        }
    }
    free(map->data);
    free(*_map_);
    raw_put(&newMap, e);
    *_map_ = newMap;
    return newMap;
}

/**
 * Удаляет карту Hashmap и высвобождает память
 * @param _map_ Объект Hashmap
 */
void destroyHashmap(Hashmap **_map_) {
    Hashmap *map = *_map_;
    size_t i, size;

    size = map->arr_size;
    for (i = 0; i < size; i++) {
        if (map->data[i]) {
            FREE_ENTRY(&(map->data[i]));
        }
    }
    if (map->size > 0) {
        free(map->data);
    }
    free(*_map_);
    *_map_ = NULL;
}

/**
 * Получение элемента карты по ключу.
 *
 * @brief get
 * @param map Объект Hashmap
 * @param key Ключ для поиска в карте
 * @return Возвращает значение для заданного ключа или NULL, если элемент не найден.
 */
V get(Hashmap *map, K key) {
    unsigned long long hash = HASHCODE(key);
    size_t index = (hash % map->arr_size);
    size_t init_index = index;
    V retVal = NULL;
    if (map->data[index] != NULL) {
        if (CMP_EQ(map->data[index]->key, key)) {
            retVal = map->data[index]->value;
        } else {
            char found = 1;
            while (map->data[index] == NULL ||
                !CMP_EQ(map->data[index]->key, key)) {
                index++;
                if (index >= map->arr_size) {
                    index = 0;
                }
                if (index == init_index) {
                    found = 0;
                    break;
                }
            }
            if (found) {
                retVal = map->data[index]->value;
            } else {
                retVal = NULL;
            }
        }
    }
    return retVal;
}

/**
 * Удаляет элемент с ключем key и возвращает объект удаленного элемента Entry или NULL, если элемент не найден.
 * @param map Объект Hashmap
 * @param key Ключ
 * @return 
 */
Entry *xremove(Hashmap *map, K key) {
    unsigned long long hash = HASHCODE(key);
    size_t index = (hash % map->arr_size);
    size_t init_index = index;
    Entry *retVal = NULL;
    if (map->data[index] != NULL && CMP_EQ(map->data[index]->key, key)) {
        retVal = map->data[index];
        map->data[index] = NULL;
        map->size--;
    } else {
        char found = 1;
        while (map->data[index] == NULL || !CMP_EQ(map->data[index]->key, key)) {
            index++;
            if (index >= map->arr_size) {
                index = 0;
            }
            if (index == init_index) {
                found = 0;
                break;
            }
        }
        if (found) {
            retVal = map->data[index];
            map->data[index] = NULL;
            map->size--;
        } else {
            retVal = NULL;
        }
    }
    return retVal;
}

/**
 * Обходит все элементы карты и применят функцию f к каждому элементу
 * @param map Объект Hashmap
 * @param f Функция, применяемая к каждому элементу карты
 * @param data опциональные данные для функции, применяемой к элементу
 */
void mapIterate(Hashmap *map, void(*f)(Entry*, void*), void* data) {
    size_t size, i;
    size = map->arr_size;
    for (i = 0; i < size; i++) {
        if (map->data[i]) {
            f(map->data[i], data);
        }
    }
}

/**
 * Выводит в консоль ключ и значение элемента карты.
 * @param e Объект Entry
 * @param data опциональные данные для совместимости с mapIterate(). Могут быть NULL.
 */
void printEntry(Entry *e, void* data) {
    printf("%5lu %s\n", *e->value, e->key);
}

/**
 * Создает в памяти копию строки для дальнейшего использования в качестве ключа элемента карты
 * @param str строка
 * @return Возвращает указатель на созданную копию строки
 */
char* initAndCopy(const char *str) {
    char *word = (char*) xmalloc(strlen(str) + 1);
    strcpy(word, str);
    return word;
}

/**
 * Создает в памяти копию переменной для дальнейшего использования в качестве значения элемента карты
 * @param count переменная с количеством слов
 * @return указатель на созданную копию
 */
size_t* initAndCopyInt(const size_t count) {
    size_t *value = (size_t*) xmalloc(1);
    *value = count;
    return value;
}

#define IN 1     /* внутри слова */
#define OUT 0    /* снаружи слова */
#define MAX_WORD_LEN 128

/**
 * Считает частоту встречаемости слов в передаваемом input_file.
 * @param input_file указатель на имя файла, в котором необходимо посчитать частоту слов.
 * @return карту Hashmap со словами в качестве ключей и их частотой в качестве значений
 */
static Hashmap* countWords(const char* input_file) {
    Hashmap *map = createHashmap(2, 0.5f, 2.0f);
    size_t input_file_size;
    size_t word_len = 0;
    char c;
    char *input_data = read_file(input_file, &input_file_size);
    char *input_data_init_ptr = input_data;
    char *word = (char*) xmalloc(MAX_WORD_LEN);
    char *word_init_ptr = word;
    char state = OUT;
    size_t count;

    while(input_file_size > 0) {
        c = *input_data++;
        input_file_size--;

        if (c == ' ' || c == '\n' || c == '\t' || c == '\000' || c == '[' || c == '{' || c == '('
            || c == ']' || c == '}' || c == ')' || c == '.' || c == ',' || c == '?' || c == '!'
            || c == ';' || c == '+' || c == '=' || c == '^' || c == '*' || c == '|' || c == '\\'
            || c == '|' || c == '/' || c == '<' || c == '>' || c == '/' || c == '\'' || c == '\"'
            || c == '`') {
            if (state == IN) {
                // end of the word
                state = OUT;
                *word = '\0';
                word = word_init_ptr; // restore initial pointer
                size_t *ptr;
                if ((ptr = get(map, word)) != NULL) {
                    count = *ptr;
                    count++;
                    xremove(map, word);
                    put(&map, initAndCopy(word), initAndCopyInt(count));
                } else {
                    count = 1;
                    put(&map, initAndCopy(word), initAndCopyInt(count));
                }
                count = 0;
            }
        } else if (state == OUT) {
            // new word
            state = IN;
            *word++ = c;
            word_len = 1;
        } else if (state == IN) {
            // along word
            word_len++;
            if (word_len <= MAX_WORD_LEN-1) {
                *word++ = c;
            }
        }
    }
    free(word);
    free(input_data_init_ptr);
    return map;
}

/**
 * Ищет в карте Hashmap элемент с максимальным значением
 * @param map карта Hashmap
 * @return элемент Entry c максимальным значением
 */
Entry* getMaxEntry(const Hashmap *map) {
    Entry *maxEntry = (Entry*) xmalloc(sizeof(Entry));
    *maxEntry->value = 0;
    size_t size, i;
    size = map->arr_size;
    for (i = 0; i < size; i++) {
        if (map->data[i] != NULL && *map->data[i]->value >= *maxEntry->value) {
            maxEntry->value = map->data[i]->value;
            maxEntry->key = map->data[i]->key;
        }
    }
    return maxEntry;
}

/**
 * Выводит в консоль элементы Hashmap, отсортированные с уменьшением частоты встречаемости.
 * Удаляет все элементы из карты по мере продвижения по ней и поиска максимального значения.
 * @param map карта Hashmap
 */
void printDescending(Hashmap *map) {
    Entry *maxEntry;
    printf("Total: %lu uniq words\n", map->size);
    while (map->size > 0) {
        maxEntry = getMaxEntry(map);
        printEntry(maxEntry, NULL);
        xremove(map, maxEntry->key);
    }
}

int main(int argc, char **argv) {
    if (argc == 2) {
        Hashmap *map = countWords(argv[1]);
        // mapIterate(map, printEntry, NULL);
        printDescending(map);
        destroyHashmap(&map);
    } else {
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}