#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*
 * Реализация хэш-таблицы с открытой адресацией со строками в качестве ключей и целыми числами в качестве значений.
 * Приложение подсчитывающее частоту слов в заданном файле.
 * 
 */

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
void destroyHashmap(Hashmap **_map_);

#define NUMARGS(...)  (sizeof((void* []){0, ##__VA_ARGS__})/sizeof(void*)-1)
#define PERROR_IF(cond, msg, map, ...) perror_if(cond, msg, map, __LINE__, NUMARGS(__VA_ARGS__), ##__VA_ARGS__)

/**
 * Проверяет успешность завершения функции в коде, если не завершение не успешно, печатает сообщение об ошибке,
 * номер строки, откуда макрос PERROR_IF был вызван, освобождает память, если были переданы указатели.
 *
 * @param cond выполняет все дальнейшие действия при истиности условия
 * @param msg произвольное имя вызвавшей фукнции
 * @param map структура Hashmap, память которой необходимо освободить
 * @param linenum номер строки кода в которой был вызван вспомогательный макрос PERROR_IF (из макроса)
 * @param numargs количество указателей, которые необходимо освободить (берутся из макроса)
 * @param ... адреса указателей или NULL
 */
void perror_if(const int cond, const char* msg, Hashmap *map, const size_t linenum, int numargs, ...) {
    if (cond) {
        va_list ap;
        fprintf(stderr, "line %lu: ", linenum);
        perror(msg);
        destroyHashmap(&map);
        // printf("Number of pointers freed: %d\n", numargs);
        va_start(ap, numargs);
        while (numargs--)
            free(va_arg(ap, void*));
        va_end(ap);
        exit(1);
    }
}

/**
 * Считывает данные из файла
 * @param filename имя файла, который необходимо прочитать
 * @param file_sz указатель на переменную, в которую будет записан размер прочитанного файла
 * @param map Структура Hashmap, удаляемая в случае необходимости exit(1) из программы
 * @return указатель на считанные из файла данные
 */
static char *read_file(const char *filename, size_t *file_sz, Hashmap *map)
{
    char *buf;

    FILE *f = fopen(filename, "rb");
    PERROR_IF(f == NULL, "fopen", map, NULL);

    size_t buf_cap = 4096;
    buf = malloc(buf_cap);
    PERROR_IF(buf == NULL, "malloc", map, NULL);

    *file_sz = 0;
    while (feof(f) == 0) {
        if (buf_cap - *file_sz == 0) {
            buf_cap *= 2;
            buf = realloc(buf, buf_cap);
            PERROR_IF(buf == NULL, "malloc", map, NULL);
        }

        *file_sz += fread(&buf[*file_sz], 1, buf_cap - *file_sz, f);
        PERROR_IF(ferror(f), "fread", map, buf);
    }

    PERROR_IF(fclose(f) != 0, "fclose", map, buf);
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
unsigned long long hashcode(const char *str) {
    unsigned long long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) ^ c;
    }
    return hash;
}



/**
 * Создает экземпляр карты с открытой адресацией
 * @param limit целочисленное значение количества элементов карты при превышении которого происходит пересборка
 * @param loadFactor процентное заполнение, при превышении которого происходит пересборка
 * @param multiplier во сколько раз увеличится размер карты при пересборке
 * @return Hashmap*
 */
Hashmap* createHashmap(size_t limit, float loadFactor, float multiplier) {
    Hashmap *tmp = (Hashmap*) malloc(sizeof(Hashmap));
    PERROR_IF(tmp == NULL, "malloc", NULL, NULL);
    tmp->arr_size = (limit >= INITIAL_SIZE) ? limit : INITIAL_SIZE;
    tmp->loadFactor = (loadFactor >= LOAD_FACTOR && loadFactor <= 1.0f) ? loadFactor : LOAD_FACTOR;
    tmp->limit = (int)(tmp->loadFactor * tmp->arr_size);
    tmp->size = 0;
    tmp->multiplier = (multiplier >= MULTIPLIER) ? multiplier : MULTIPLIER;

    tmp->data = (Entry**) calloc(tmp->arr_size, sizeof(Entry*));
    PERROR_IF(tmp->data == NULL, "calloc", tmp, NULL);
    return tmp;
}

/**
 * Удаляет карту Hashmap и высвобождает память
 * @param _map_ Объект Hashmap
 */
void destroyHashmap(Hashmap **_map_) {
    if (_map_ == NULL) return;
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
    Entry *e = malloc(sizeof(Entry));
    PERROR_IF(e == NULL, "malloc", *map, key, value);
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
            int found = 1;
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

#define IN 1     /* внутри слова */
#define OUT 0    /* снаружи слова */
#define MAX_WORD_LEN 128

/**
 * Считает частоту встречаемости слов в передаваемом input_file.
 * @param input_file указатель на имя файла, в котором необходимо посчитать частоту слов.
 * @return карту Hashmap со словами в качестве ключей и их частотой в качестве значений
 */
Hashmap* countWords(const char* input_file) {
    Hashmap *map = createHashmap(2, 0.5f, 2.0f);
    size_t input_file_size;
    size_t word_len = 0;
    char c;
    char *input_data = read_file(input_file, &input_file_size, map);
    char *input_data_init_ptr = input_data;
    char *word = (char*) malloc(sizeof(char)*MAX_WORD_LEN);
    char *word_init_ptr = word;
    PERROR_IF(word == NULL, "malloc", map, input_data);
    char state = OUT;

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
                word = word_init_ptr;
                // word -= word_len+1; // указатель в начало слова
                char *key = malloc(strlen(word)*sizeof(char) + sizeof(char));
                PERROR_IF(key == NULL, "malloc", map, word, input_data_init_ptr);
                strcpy(key, word);
                size_t *value = malloc(sizeof(size_t));
                PERROR_IF(value == NULL, "malloc", map, word, key, input_data_init_ptr);

                size_t *old_value;
                if ((old_value = get(map, key)) != NULL) {
                    *value = *old_value+1;
                    Entry* e = xremove(map, key);
                    FREE_ENTRY(&e);
                    put(&map, key, value);
                } else {
                    *value = 1;
                    put(&map, key, value);
                }
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

    free(word_init_ptr);
    free(input_data_init_ptr);
    return map;
}

/**
 * Выводит в stdout элементы Hashmap, отсортированные по значению в порядке убывания
 * @param map карта Hashmap
 */
void printDescending(Hashmap *map) {
    K* keysDesc = malloc(sizeof(K) * map->size);
    PERROR_IF(keysDesc == NULL, "malloc", map, NULL);
    size_t maxValue = 0;
    for (size_t i = 0; i < map->size; i++) {
        for (size_t j = 0; j < map->arr_size; j++) {
            if (map->data[j] != NULL && *map->data[j]->value >= maxValue) {
                for (size_t k = 0; k < i; k++)
                    if (map->data[j]->key == keysDesc[k]) goto next; // пропускаем итерацию j, если указатель
                                                                     // на ключ уже был отобран ранее
                keysDesc[i] = map->data[j]->key;
                maxValue = *map->data[j]->value;
                next:
                ;
            }
        }
        printf("%5lu %s\n", maxValue, keysDesc[i]);
        maxValue = 0;
    }
    free(keysDesc);
}

int main(int argc, char **argv) {
    if (argc == 2) {
        Hashmap *map = countWords(argv[1]);
        printDescending(map);
        destroyHashmap(&map);
    } else {
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}