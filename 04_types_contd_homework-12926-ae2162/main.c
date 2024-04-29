#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#define PERROR_IF(cond, msg) if (cond) { perror(msg); exit(1); }

int utf8_encode(char *out, uint32_t utf);

static size_t codepage_to_utf8(const char *in, char *out, const char *codepage) {
    size_t utf8_bytes = 0;
    /*cp1251_to_utf8_table: contains utf-8 codes of the symbols in sequence they found in cp1251 */
    static const int cp1251_to_utf8_table[128] = {
            0x402, 0x403, 0x20a1, 0x453, 0x201e, 0x2026, 0x2020, 0x2021, 0x20ac,
            0x2030, 0x409, 0x2039, 0x40a, 0x40c, 0x40b, 0x40f, 0x452, 0x2018,
            0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,  0, 0x2122,
            0x459, 0x203a, 0x45a, 0x45c, 0x45b, 0x45f, 0xa0, 0x40e, 0x45e,
            0x408, 0xa4, 0x490, 0xa6, 0xa7, 0x401, 0xa9, 0x404, 0xab,
            0xac, 0xad, 0xae, 0x407, 0xb0, 0xb1, 0x406, 0x456, 0x491,
            0xb5, 0xb6, 0xb7, 0x451, 0x2116, 0x454, 0xbb, 0x458, 0x405,
            0x455, 0x457, 0x410, 0x411, 0x412, 0x413, 0x414, 0x415, 0x416,
            0x417, 0x418, 0x419, 0x41a, 0x41b, 0x41c, 0x41d, 0x41e, 0x41f,
            0x420, 0x421, 0x422, 0x423, 0x424, 0x425, 0x426, 0x427, 0x428,
            0x429, 0x42a, 0x42b, 0x42c, 0x42d, 0x42e,0x42f, 0x430, 0x431,
            0x432, 0x433, 0x434, 0x435, 0x436, 0x437, 0x438, 0x439,
            0x43a, 0x43b, 0x43c, 0x43d, 0x43e, 0x43f, 0x440, 0x441,
            0x442, 0x443, 0x444, 0x445, 0x446, 0x447, 0x448, 0x449,
            0x44a, 0x44b, 0x44c, 0x44d, 0x44e, 0x44f
    };

    static const int koi8r_to_utf_table[128] = {
            0x2500, 0x2502, 0x250c, 0x2510, 0x2514, 0x2518, 0x251c, 0x2524, 0x252c,
            0x2534, 0x253c, 0x2580, 0x2584, 0x2588, 0x258c, 0x2590, 0x2591,
            0x2592, 0x2593, 0x2320, 0x25a0, 0x2219, 0x221a, 0x2248, 0x2264,
            0x2265, 0xa0, 0x2321, 0xb0, 0xb2, 0xb7, 0xf7, 0x2550, 0x2551,
            0x2552, 0x451, 0x2553, 0x2554, 0x2555, 0x2556, 0x2557, 0x2558,
            0x2559, 0x255a, 0x255b, 0x255c, 0x255d, 0x255e, 0x255f, 0x2560,
            0x2561, 0x401, 0x2562, 0x2563, 0x2564, 0x2565, 0x2566, 0x2567,
            0x2568, 0x2569, 0x256a, 0x256b, 0x256c, 0xa9, 0x44e, 0x430,
            0x431, 0x446, 0x434, 0x435, 0x444, 0x433, 0x445, 0x438, 0x439,
            0x43a, 0x43b, 0x43c, 0x43d, 0x43e, 0x43f, 0x44f, 0x440, 0x441,
            0x442, 0x443, 0x436, 0x432, 0x44c, 0x44b, 0x437, 0x448, 0x44d,
            0x449, 0x447, 0x44a, 0x42e, 0x410, 0x411, 0x426, 0x414, 0x415,
            0x424, 0x413, 0x425, 0x418, 0x419, 0x41a, 0x41b, 0x41c,
            0x41d, 0x41e, 0x41f, 0x42f, 0x420, 0x421, 0x422, 0x423,
            0x416, 0x412, 0x42c, 0x42b, 0x417, 0x428, 0x42d, 0x429,
            0x427, 0x42a
    };

    static const int iso_8859_5_to_utf_table[128] = {
            0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
            0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
            0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d,
            0x9e, 0x9f, 0xa0, 0x401, 0x402, 0x403, 0x404, 0x405, 0x406,
            0x407, 0x408, 0x409, 0x40a, 0x40b, 0x40c, 0xad, 0x40e, 0x40f,
            0x410, 0x411, 0x412, 0x413, 0x414, 0x415, 0x416, 0x417, 0x418,
            0x419, 0x41a, 0x41b, 0x41c, 0x41d, 0x41e, 0x41f, 0x420, 0x421,
            0x422, 0x423, 0x424, 0x425, 0x426, 0x427, 0x428, 0x429, 0x42a,
            0x42b, 0x42c, 0x42d, 0x42e, 0x42f, 0x430, 0x431, 0x432, 0x433,
            0x434, 0x435, 0x436, 0x437, 0x438, 0x439, 0x43a, 0x43b, 0x43c,
            0x43d, 0x43e, 0x43f, 0x440, 0x441, 0x442, 0x443, 0x444, 0x445,
            0x446, 0x447, 0x448, 0x449, 0x44a, 0x44b, 0x44c, 0x44d,
            0x44e, 0x44f, 0x2116, 0x451, 0x452, 0x453, 0x454, 0x455,
            0x456, 0x457, 0x458, 0x459, 0x45a, 0x45b, 0x45c, 0xa7,
            0x45e, 0x45f

    };

    const static int *code_table;
    if (strcmp(codepage, "cp1251") == 0)
        code_table = cp1251_to_utf8_table;
    else if (strcmp(codepage, "koi8-r") == 0)
        code_table = koi8r_to_utf_table;
    else if (strcmp(codepage, "iso-8859-5") == 0)
        code_table = iso_8859_5_to_utf_table;
    else {
        perror("Unsupported codepage");
        exit(1);
    }

    while (*in)
        if (*in & 0x80) {
            int codepoint = code_table[(int)(0x7f & *in++)];
            char utf8_byte_array[5];
            int len = utf8_encode(utf8_byte_array, codepoint);
            utf8_bytes += len;
            char *ptr = utf8_byte_array;
            while(len--)
                *out++ = *ptr++;
        }
        else {
            *out++ = *in++;
            utf8_bytes++;
        }
    *out = 0;
    return utf8_bytes;
}

static void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    PERROR_IF(ptr == NULL, "malloc");
    return ptr;
}

static void *xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    PERROR_IF(ptr == NULL, "realloc");
    return ptr;
}

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

static void write_file(const char *filename, const char *data, size_t n)
{
    FILE *f;

    f = fopen(filename, "wb");
    PERROR_IF(f == NULL, "fopen");
    PERROR_IF(fwrite(data, 1, n, f) != n, "fwrite");
    PERROR_IF(fclose(f) != 0, "fclose");
}

static void print_usage(const char *argv0)
{
    printf("Converts cp1251|koi8-r|iso-8859-5 text to utf-8\n\n");
    printf("Usage:\n\n");
    printf("  %s <input_file> <codepage: cp1251|koi8-r|iso-8859-5> <output_file>\n", argv0);
    printf("\n");
}

static void convert(const char* input_file, const char* output_file, const char* codepage)
{
    char *input_data;
    char *output_data;
    size_t input_file_size;
    size_t utf8_bytes = 0;

    printf ("Converting %s from %s to UTF-8\n", input_file, codepage);

    input_data = read_file(input_file, &input_file_size);
    output_data = xmalloc(input_file_size*2);

    utf8_bytes = codepage_to_utf8(input_data, output_data, codepage);
    write_file(output_file, output_data, utf8_bytes);

    free(input_data);
    free(output_data);
}

int utf8_encode(char *out, uint32_t utf)
{
    if (utf <= 0x7F) {
        // Plain ASCII
        out[0] = (char) utf;
        out[1] = 0;
        return 1;
    }
    else if (utf <= 0x07FF) {
        // 2-byte unicode
        out[0] = (char) (((utf >> 6) & 0x1F) | 0xC0);
        out[1] = (char) (((utf >> 0) & 0x3F) | 0x80);
        out[2] = 0;
        return 2;
    }
    else if (utf <= 0xFFFF) {
        // 3-byte unicode
        out[0] = (char) (((utf >> 12) & 0x0F) | 0xE0);
        out[1] = (char) (((utf >>  6) & 0x3F) | 0x80);
        out[2] = (char) (((utf >>  0) & 0x3F) | 0x80);
        out[3] = 0;
        return 3;
    }
    else if (utf <= 0x10FFFF) {
        // 4-byte unicode
        out[0] = (char) (((utf >> 18) & 0x07) | 0xF0);
        out[1] = (char) (((utf >> 12) & 0x3F) | 0x80);
        out[2] = (char) (((utf >>  6) & 0x3F) | 0x80);
        out[3] = (char) (((utf >>  0) & 0x3F) | 0x80);
        out[4] = 0;
        return 4;
    }
    else {
        // error - use replacement character
        out[0] = (char) 0xEF;
        out[1] = (char) 0xBF;
        out[2] = (char) 0xBD;
        out[3] = 0;
        return 0;
    }
}

int main(int argc, char **argv)
{
    if (argc == 4 && (strcmp(argv[2], "cp1251") == 0 || strcmp(argv[2], "koi8-r") ==0
                      || strcmp(argv[2], "iso-8859-5") == 0)) {
        convert(argv[1], argv[3], argv[2]);
    } else {
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
