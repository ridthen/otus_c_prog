#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <mysql.h>
#include <errno.h>

static MYSQL *conn = NULL;
static MYSQL_RES *result = NULL;

static void cleanup(void)
{
    if (result) {
        mysql_free_result(result);
        result = NULL;
    }
    if (conn) {
        mysql_close(conn);
        conn = NULL;
    }
}

static void finish_with_error(void)
{
    if (conn) {
        fputs(mysql_error(conn), stderr);
        fputc('\n', stderr);
    }
    exit(EXIT_FAILURE);
}

static bool is_valid_identifier(const char *s)
{
    if (!s || !*s) return false;
    for (size_t i = 0; s[i]; ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '$'))
            return false;
    }
    return true;
}

typedef struct {
    const char *host;
    const char *user;
    const char *pass;
    const char *db;
    const char *table;
    const char *column;
} arguments_t;

static int parse_arguments(int argc, char *argv[], arguments_t *args)
{
    args->host = "localhost";
    args->user = "root";
    args->pass = "my-secret-pw";
    args->db = NULL;
    args->table = NULL;
    args->column = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "h:u:p:d:t:c:")) != -1) {
        switch (opt) {
        case 'h': args->host = optarg; break;
        case 'u': args->user = optarg; break;
        case 'p': args->pass = optarg; break;
        case 'd': args->db = optarg; break;
        case 't': args->table = optarg; break;
        case 'c': args->column = optarg; break;
        default:
            fputs("Использование: dbstat -d база -t таблица -c колонка "
                  "[-h хост] [-u пользователь] [-p пароль]\n", stderr);
            return -1;
        }
    }

    if (!args->db || !args->table || !args->column) {
        fputs("Ошибка: необходимо указать базу данных, таблицу и колонку\n",
              stderr);
        return -1;
    }

    return 0;
}

static int validate_identifiers(const arguments_t *args)
{
    if (!is_valid_identifier(args->db) || !is_valid_identifier(args->table)
        || !is_valid_identifier(args->column)) {
        fputs("Ошибка: имя базы, таблицы или колонки содержит"
              " недопустимые символы\n", stderr);
        return -1;
    }
    return 0;
}

static int db_connect(const arguments_t *args)
{
    conn = mysql_init(NULL);
    if (!conn) {
        perror("mysql_init");
        return -1;
    }

    if (!mysql_real_connect(conn, args->host, args->user, args->pass,
                            args->db, 0, NULL, 0)) {
        finish_with_error();
    }
    return 0;
}

static int execute_query(const arguments_t *args)
{
    char query[512] = {0};
    int needed = snprintf(query, sizeof(query),
                         "SELECT `%s` FROM `%s`", args->column, args->table);
    if (needed < 0 || (size_t)needed >= sizeof(query)) {
        fputs("Ошибка: буфер запроса недостаточен\n", stderr);
        return -1;
    }

    if (mysql_query(conn, query)) {
        finish_with_error();
    }

    result = mysql_store_result(conn);
    if (!result) {
        finish_with_error();
    }

    unsigned int num_fields = mysql_num_fields(result);
    if (num_fields != 1) {
        fputs("Ошибка: ожидалась ровно одна колонка в результате\n", stderr);
        return -1;
    }

    return 0;
}

typedef struct {
    size_t count;
    size_t skipped;
    double sum;
    double sum_sq;
    double min;
    double max;
} stats_t;

static int process_rows(stats_t *stats)
{
    stats->count = 0;
    stats->skipped = 0;
    stats->sum = 0.0;
    stats->sum_sq = 0.0;
    bool first = true;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        const char *val = row[0];
        if (!val) {
            ++stats->skipped;
            continue;
        }

        char *endptr;
        errno = 0;
        double d = strtod(val, &endptr);
        if (errno == ERANGE) {
            ++stats->skipped;
            continue;
        }
        if (endptr == val || *endptr != '\0') {
            ++stats->skipped;
            continue;
        }

        if (first) {
            stats->min = stats->max = d;
            first = false;
        } else {
            if (d < stats->min) stats->min = d;
            if (d > stats->max) stats->max = d;
        }
        stats->sum += d;
        stats->sum_sq += d * d;
        ++stats->count;
    }

    if (stats->count == 0) {
        fputs("В указанной колонке не найдено числовых значений\n", stderr);
        return -1;
    }

    return 0;
}

static void print_statistics(const stats_t *stats)
{
    double mean = stats->sum / stats->count;
    double variance = (stats->sum_sq / stats->count) - (mean * mean);

    printf("Количество обработанных числовых значений: %zu\n", stats->count);
    if (stats->skipped > 0) {
        printf("Пропущено нечисловых (или NULL) значений: %zu\n", stats->skipped);
    }
    printf("Сумма:    %g\n", stats->sum);
    printf("Среднее:  %g\n", mean);
    printf("Минимум:  %g\n", stats->min);
    printf("Максимум: %g\n", stats->max);
    printf("Дисперсия: %g\n", variance);
}

int main(int argc, char *argv[])
{
    if (atexit(cleanup) != 0) {
        perror("atexit");
        goto error;
    }

    arguments_t args;
    if (parse_arguments(argc, argv, &args) != 0) {
        goto error;
    }

    if (validate_identifiers(&args) != 0) {
        goto error;
    }

    if (db_connect(&args) != 0) {
        goto error;
    }

    if (execute_query(&args) != 0) {
        goto error;
    }

    stats_t stats;
    if (process_rows(&stats) != 0) {
        goto error;
    }

    print_statistics(&stats);

    mysql_free_result(result);
    result = NULL;
    mysql_close(conn);
    conn = NULL;

    return EXIT_SUCCESS;
error:
    return EXIT_FAILURE;
}