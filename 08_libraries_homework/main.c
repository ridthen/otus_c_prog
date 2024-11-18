#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include "json_object.h"
#include "json_tokener.h"
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>


struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static int strict_mode = 0;
static const char *fname = NULL;

static void showmem(void)
{
#ifdef HAVE_GETRUSAGE
	struct rusage rusage;
	memset(&rusage, 0, sizeof(rusage));
	getrusage(RUSAGE_SELF, &rusage);
	fprintf(stderr, "maxrss: %ld KB\n", rusage.ru_maxrss);
#endif
}

static int parseit(const struct MemoryStruct *memory_struct, int (*callback)(json_object *))
{
	json_object *obj;
	int depth = JSON_TOKENER_DEFAULT_DEPTH;
	json_tokener *tok;
	tok = json_tokener_new_ex(depth);
	if (!tok)
	{
		fprintf(stderr, "unable to allocate json_tokener: %s\n", strerror(errno));
		return 1;
	}
	if (strict_mode)
	{
		json_tokener_set_flags(tok, JSON_TOKENER_STRICT
	#ifdef JSON_TOKENER_ALLOW_TRAILING_CHARS
			 | JSON_TOKENER_ALLOW_TRAILING_CHARS
	#endif
		);
	}
	obj = json_tokener_parse_ex(tok, memory_struct->memory, memory_struct->size);
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

static int showobj(const json_object *root)
{
	if (root == NULL)
	{
		fprintf(stderr, "%s: Failed to parse\n", fname);
		return 1;
	}

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


int main(void)
{
  CURL *curl_handle;
  CURLcode res;

  struct MemoryStruct chunk;

  chunk.memory = malloc(1);  /* grown as needed by the realloc above */
  chunk.size = 0;    /* no data at this point */

  curl_global_init(CURL_GLOBAL_ALL);

  /* init the curl session */
  curl_handle = curl_easy_init();

  /* specify URL to get */
  curl_easy_setopt(curl_handle, CURLOPT_URL, "https://wttr.in/Moscow?format=j1");

  /* send all data to this function  */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

  /* some servers do not like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

  /* get it! */
  res = curl_easy_perform(curl_handle);

  /* check for errors */
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  }
  else {
    /*
     * Now, our chunk.memory points to a memory block that is chunk.size
     * bytes big and contains the remote file.
     *
     * Do something nice with it!
     */

    // printf("%lu bytes retrieved\n", (unsigned long)chunk.size);

  	parseit(&chunk, showobj);
  }

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);

  free(chunk.memory);

  /* we are done with libcurl, so clean it up */
  curl_global_cleanup();

  return 0;
}