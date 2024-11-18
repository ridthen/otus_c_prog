#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <curl/curl.h>
#include "json_object.h"
#include "json_tokener.h"
#include <stddef.h>
#include <unistd.h>

#define NUMARGS(...)  (sizeof((void* []){0, ##__VA_ARGS__})/sizeof(void*)-1)
#define PERROR_IF(cond, msg, ...) perror_if(cond, msg, __LINE__, NUMARGS(__VA_ARGS__), ##__VA_ARGS__)

/**
 * Проверяет успешность завершения функции в коде, если завершение не успешно, печатает сообщение об ошибке,
 * номер строки, откуда макрос PERROR_IF был вызван, освобождает память, если были переданы указатели.
 *
 * @param cond выполняет все дальнейшие действия при истиности условия
 * @param msg произвольное имя вызвавшей фукнции
 * @param linenum номер строки кода в которой был вызван вспомогательный макрос PERROR_IF (из макроса)
 * @param numargs количество указателей, которые необходимо освободить (берутся из макроса)
 * @param ... адреса указателей или NULL
 */
void perror_if(const int cond, const char* msg, const size_t linenum, int numargs, ...) {
    if (cond) {
        va_list ap;
        fprintf(stderr, "line %lu: ", linenum);
        perror(msg);
        // printf("Number of pointers freed: %d\n", numargs);
        va_start(ap, numargs);
        while (numargs--)
            free(va_arg(ap, void*));
        va_end(ap);
        exit(1);
    }
}
/**
 * Функция-обертка для malloc с проверкой возвращаемого указателя на NULL
 * @param size Размер выделяемой памяти, байт
 * @return void указатель на выделенную область памяти
 */
static void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    PERROR_IF(ptr == NULL, "malloc", ptr);
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
    PERROR_IF(ptr == NULL, "realloc", ptr);
    return ptr;
}

typedef struct MemoryStruct {
  char *memory;
  size_t size;
} MemoryStruct;

/**
 * Выделяет память для записи данных, полученных Curl
 * @param contents
 * @param size
 * @param nmemb
 * @param userp
 * @return
 */
static size_t WriteMemoryCallback(const void *contents, const size_t size, const size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  MemoryStruct *mem = userp;
  char *ptr = xrealloc(mem->memory, mem->size + realsize + 1);
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
  return realsize;
}

static int strict_mode = 0;

/**
 * Парсит объект JSON в переданной структуре памяти
 * @param memory_struct структура памяти MemoryStruct в которой находится объект JSON
 * @param callback функция которой передается извлеченный объект JSON
 * @return в случае успешного завершения возвращается 0, иначе 1
 */
static int parseit(const MemoryStruct *memory_struct, int (*callback)(json_object *))
{
	int depth = JSON_TOKENER_DEFAULT_DEPTH;
	json_tokener *tok = json_tokener_new_ex(depth);

	char err_msg[200];
	strcpy(err_msg, "unable to allocate json_tokener: ");
	PERROR_IF(tok == NULL, strcat(err_msg, strerror(errno)), memory_struct->memory);

	if (strict_mode) {
		json_tokener_set_flags(tok, JSON_TOKENER_STRICT
	#ifdef JSON_TOKENER_ALLOW_TRAILING_CHARS
			 | JSON_TOKENER_ALLOW_TRAILING_CHARS
	#endif
		);
	}
	json_object *obj = json_tokener_parse_ex(tok, memory_struct->memory, memory_struct->size);
	if (obj != NULL) {
		int cb_ret = callback(obj);
		json_object_put(obj);
		if (cb_ret != 0) {
			json_tokener_free(tok);
			return 1;
		}
	}
	json_tokener_free(tok);
	return 0;
}

/**
 * Извлекает нужные поля из объекта JSON и печатает их в консоль
 * @param root объект JSON
 * @return 0 в случае успешности завершения, 1 в случае указателя NULL на входе
 */
static int showobj(json_object *root) {
	if (root == NULL) {
		fprintf(stderr, "Failed to parse input data\n");
		return 1;
	}

	json_object *request = json_object_object_get(root, "request");
	json_object *request_0 = json_object_array_get_idx(request, 0);
	json_object *query = json_object_object_get(request_0, "query");
	printf("Weather in %s\n", json_object_get_string(query));


	json_object *current_condition = json_object_object_get(root, "current_condition");
	json_object *current_condition_0 = json_object_array_get_idx(current_condition, 0);
	json_object *weatherDesc = json_object_object_get(current_condition_0, "weatherDesc");
	json_object *weatherDesc_0 = json_object_array_get_idx(weatherDesc, 0);
	json_object *weatherDesc_value = json_object_object_get(weatherDesc_0, "value");
	printf("Weather description: %s\n", json_object_get_string(weatherDesc_value));
	json_object *winddir16Point = json_object_object_get(current_condition_0, "winddir16Point");
	printf("Wind direction: %s\n", json_object_get_string(winddir16Point));
	json_object *windspeedKmph = json_object_object_get(current_condition_0, "windspeedKmph");
	printf("Wind speed: %s km/h\n", json_object_get_string(windspeedKmph));
	json_object *temp_C = json_object_object_get(current_condition_0, "temp_C");
	printf("Temperature: %s deg.C\n", json_object_get_string(temp_C));

	return 0;
}

/**
 * Печатает сообщение о способе запуска программы
 * @param argv0 название файла вызываемой программы
 */
static void print_usage(const char *argv0)
{
	printf("Weather nowcast for given location\n\n");
	printf("Usage:\n\n");
	printf("  %s Location\n", argv0);
	printf("\n");
}


int main(int argc, char **argv) {

	if (argc == 2) {
		char* location = argv[1];
		char url[200];
		strcpy(url, "https://wttr.in/");
		strcat(url, location);
		strcat(url, "?format=j1");

		MemoryStruct chunk;
		chunk.memory = xmalloc(1);  /* grown as needed by the realloc above */
		chunk.size = 0; /* no data at this point */

		curl_global_init(CURL_GLOBAL_ALL);
		/* init the curl session */
		CURL *curl_handle = curl_easy_init();
		/* specify URL to get */
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		/* send all data to this function  */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		/* we pass our 'chunk' struct to the callback function */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
		/* some servers do not like requests that are made without a user-agent
		   field, so we provide one */
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		/* get it! */
		CURLcode res = curl_easy_perform(curl_handle);
		/* check for errors */
		char err_msg[200];
		strcpy(err_msg, "curl_easy_perform() failed: ");
		PERROR_IF(res != CURLE_OK, strcat(err_msg, curl_easy_strerror(res)), chunk.memory);
		PERROR_IF(chunk.size <= 1, "no network data received", chunk.memory);

		// printf("Weather in %s:\n", location);
		parseit(&chunk, showobj);


		/* cleanup curl stuff */
		curl_easy_cleanup(curl_handle);

		free(chunk.memory);

		/* we are done with libcurl, so clean it up */
		curl_global_cleanup();

	} else {
		print_usage(argv[0]);
		return 1;
	}
	return 0;
}