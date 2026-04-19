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

int main(int argc, char *argv[])
{
    if (atexit(cleanup) != 0) {
        perror("atexit");
        return EXIT_FAILURE;
    }

    const char *host = "localhost";
    const char *user = "root";
    const char *pass = "my-secret-pw";
    const char *db = NULL;
    const char *table = NULL;
    const char *column = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "h:u:p:d:t:c:")) != -1) {
        switch (opt) {
        case 'h': host = optarg; break;
        case 'u': user = optarg; break;
        case 'p': pass = optarg; break;
        case 'd': db = optarg; break;
        case 't': table = optarg; break;
        case 'c': column = optarg; break;
        default:
            fputs("Использование: dbstat -d база -t таблица -c колонка "
                  "[-h хост] [-u пользователь] [-p пароль]\n", stderr);
            return EXIT_FAILURE;
        }
    }

    if (!db || !table || !column) {
        fputs("Ошибка: необходимо указать базу данных, таблицу и колонку\n",
            stderr);
        return EXIT_FAILURE;
    }

    if (!is_valid_identifier(db) || !is_valid_identifier(table)
        || !is_valid_identifier(column)) {
        fputs("Ошибка: имя базы, таблицы или колонки содержит"
              " недопустимые символы\n", stderr);
        return EXIT_FAILURE;
    }

    conn = mysql_init(NULL);
    if (!conn) {
        perror("mysql_init");
        return EXIT_FAILURE;
    }

    if (!mysql_real_connect(conn, host, user, pass, db, 0,
        NULL, 0)) {
        finish_with_error();
    }

    char query[512] = {0};
    int needed = snprintf(query, sizeof(query),
                         "SELECT `%s` FROM `%s`", column, table);
    if (needed < 0 || (size_t)needed >= sizeof(query)) {
        fputs("Ошибка: буфер запроса недостаточен\n", stderr);
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    size_t count = 0;
    size_t skipped = 0;
    double sum = 0.0;
    double sum_sq = 0.0;
    double min = 0.0;
    double max = 0.0;
    bool first = true;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        const char *val = row[0];
        if (!val) {
            ++skipped;
            continue;
        }

        char *endptr;
        errno = 0;
        double d = strtod(val, &endptr);
        if (errno == ERANGE) {
            ++skipped;
            continue;
        }
        if (endptr == val || *endptr != '\0') {
            ++skipped;
            continue;
        }

        if (first) {
            min = max = d;
            first = false;
        } else {
            if (d < min) min = d;
            if (d > max) max = d;
        }
        sum += d;
        sum_sq += d * d;
        ++count;
    }

    if (count == 0) {
        fputs("В указанной колонке не найдено числовых значений\n", stderr);
        exit(EXIT_FAILURE);
    }

    double mean = sum / count;
    double variance = (sum_sq / count) - (mean * mean);

    printf("Количество обработанных числовых значений: %zu\n", count);
    if (skipped > 0) {
        printf("Пропущено нечисловых (или NULL) значений: %zu\n", skipped);
    }
    printf("Сумма:    %g\n", sum);
    printf("Среднее:  %g\n", mean);
    printf("Минимум:  %g\n", min);
    printf("Максимум: %g\n", max);
    printf("Дисперсия: %g\n", variance);

    mysql_free_result(result);
    result = NULL;
    mysql_close(conn);
    conn = NULL;

    return EXIT_SUCCESS;
}
