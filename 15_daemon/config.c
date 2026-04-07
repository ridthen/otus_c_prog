#include "config.h"
#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct config *load_config(const char *filename) {
    FILE *fh = fopen(filename, "rb");
    if (!fh) {
        perror("fopen config");
        return NULL;
    }

    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fh);

    struct config *cfg = calloc(1, sizeof(struct config));
    if (!cfg) {
        perror("calloc config");
        fclose(fh);
        yaml_parser_delete(&parser);
        return NULL;
    }

    int in_file_path = 0;
    int in_socket_path = 0;
    int done = 0;

    while (!done) {
        yaml_event_t event;
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "YAML parse error\n");
            free_config(cfg);
            cfg = NULL;
            break;
        }

        switch (event.type) {
            case YAML_SCALAR_EVENT:
                if (in_file_path) {
                    cfg->file_path = strdup((char *)event.data.scalar.value);
                    in_file_path = 0;
                } else if (in_socket_path) {
                    cfg->socket_path = strdup((char *)event.data.scalar.value);
                    in_socket_path = 0;
                } else if (strcmp((char *)event.data.scalar.value, "file_path") == 0) {
                    in_file_path = 1;
                } else if (strcmp((char *)event.data.scalar.value, "socket_path") == 0) {
                    in_socket_path = 1;
                }
                break;
            case YAML_STREAM_END_EVENT:
                done = 1;
                break;
            default:
                break;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);

    if (cfg && (!cfg->file_path || !cfg->socket_path)) {
        fprintf(stderr, "В конфигурации отсутствует file_path или socket_path\n");
        free_config(cfg);
        cfg = NULL;
    }
    return cfg;
}

void free_config(struct config *cfg) {
    if (cfg) {
        free(cfg->file_path);
        free(cfg->socket_path);
        free(cfg);
    }
}

