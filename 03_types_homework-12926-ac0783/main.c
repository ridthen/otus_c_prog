#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

/* Read a 64-bit value from p in little-endian byte order. */
static inline uint64_t read64le(const uint8_t *p)
{
    /* The one true way, see
     * https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html */
    return ((uint64_t)p[0] << 0)  |
           ((uint64_t)p[1] << 8)  |
           ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

static inline uint32_t read32le(const uint8_t *p)
{
    return ((uint32_t)p[0] << 0)  |
           ((uint32_t)p[1] << 8)  |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint16_t read16le(const uint8_t *p)
{
    return (uint16_t)(
            ((uint16_t)p[0] << 0) |
            ((uint16_t)p[1] << 8));
}

/* End of Central Directory Record. */
struct eocdr {
    uint16_t disk_nbr;        /* Number of this disk. */
    uint16_t cd_start_disk;   /* Nbr. of disk with start of the CD. */
    uint16_t disk_cd_entries; /* Nbr. of CD entries on this disk. */
    uint16_t cd_entries;      /* Nbr. of Central Directory entries. */
    uint32_t cd_size;         /* Central Directory size in bytes. */
    uint32_t cd_offset;       /* Central Directory file offset. */
    uint16_t comment_len;     /* Archive comment length. */
    const uint8_t *comment;   /* Archive comment. */
};

/* Read 16/32 bits little-endian and bump p forward afterwards. */
#define READ16(p) ((p) += 2, read16le((p) - 2))
#define READ32(p) ((p) += 4, read32le((p) - 4))

/* Size of the End of Central Directory Record, not including comment. */
#define EOCDR_BASE_SZ 22
#define EOCDR_SIGNATURE 0x06054b50  /* "PK\5\6" little-endian. */

#define EXT_ATTR_DIR (1U << 4)
#define EXT_ATTR_ARC (1U << 5)

/* Central File Header (Central Directory Entry) */
struct cfh {
    uint16_t made_by_ver;    /* Version made by. */
    uint16_t extract_ver;    /* Version needed to extract. */
    uint16_t gp_flag;        /* General purpose bit flag. */
    uint16_t method;         /* Compression method. */
    uint16_t mod_time;       /* Modification time. */
    uint16_t mod_date;       /* Modification date. */
    uint32_t crc32;          /* CRC-32 checksum. */
    uint32_t comp_size;      /* Compressed size. */
    uint32_t uncomp_size;    /* Uncompressed size. */
    uint16_t name_len;       /* Filename length. */
    uint16_t extra_len;      /* Extra data length. */
    uint16_t comment_len;    /* Comment length. */
    uint16_t disk_nbr_start; /* Disk nbr. where file begins. */
    uint16_t int_attrs;      /* Internal file attributes. */
    uint32_t ext_attrs;      /* External file attributes. */
    uint32_t lfh_offset;     /* Local File Header offset. */
    const uint8_t *name;     /* Filename. */
    const uint8_t *extra;    /* Extra data. */
    const uint8_t *comment;  /* File comment. */
};

/* Size of a Central File Header, not including name, extra, and comment. */
#define CFH_BASE_SZ 46
#define CFH_SIGNATURE 0x02014b50 /* "PK\1\2" little-endian. */

static bool read_cfh(struct cfh *cfh, const uint8_t *src, size_t src_len,
                     size_t offset)
{
    const uint8_t *p;
    uint32_t signature;

    if (offset > src_len || src_len - offset < CFH_BASE_SZ) {
        return false;
    }

    p = &src[offset];
    signature = READ32(p);
    if (signature != CFH_SIGNATURE) {
        return false;
    }

    cfh->made_by_ver = READ16(p);
    cfh->extract_ver = READ16(p);
    cfh->gp_flag = READ16(p);
    cfh->method = READ16(p);
    cfh->mod_time = READ16(p);
    cfh->mod_date = READ16(p);
    cfh->crc32 = READ32(p);
    cfh->comp_size = READ32(p);
    cfh->uncomp_size = READ32(p);
    cfh->name_len = READ16(p);
    cfh->extra_len = READ16(p);
    cfh->comment_len = READ16(p);
    cfh->disk_nbr_start = READ16(p);
    cfh->int_attrs = READ16(p);
    cfh->ext_attrs = READ32(p);
    cfh->lfh_offset = READ32(p);
    cfh->name = p;
    cfh->extra = cfh->name + cfh->name_len;
    cfh->comment = cfh->extra + cfh->extra_len;
    assert(p == &src[offset + CFH_BASE_SZ] && "All fields read.");

    if (src_len - offset - CFH_BASE_SZ <
        cfh->name_len + cfh->extra_len + cfh->comment_len) {
        return false;
    }

    return true;
}

/* Convert DOS date and time to time_t. */
static time_t dos2ctime(uint16_t dos_date, uint16_t dos_time)
{
    struct tm tm = {0};

    tm.tm_sec = (dos_time & 0x1f) * 2;  /* Bits 0--4:  Secs divided by 2. */
    tm.tm_min = (dos_time >> 5) & 0x3f; /* Bits 5--10: Minute. */
    tm.tm_hour = (dos_time >> 11);      /* Bits 11-15: Hour (0--23). */

    tm.tm_mday = (dos_date & 0x1f);          /* Bits 0--4: Day (1--31). */
    tm.tm_mon = ((dos_date >> 5) & 0xf) - 1; /* Bits 5--8: Month (1--12). */
    tm.tm_year = (dos_date >> 9) + 80;       /* Bits 9--15: Year-1980. */

    tm.tm_isdst = -1;

    return mktime(&tm);
}

/* Convert time_t to DOS date and time. */
static void ctime2dos(time_t t, uint16_t *dos_date, uint16_t *dos_time)
{
    struct tm *tm = localtime(&t);

    *dos_time = 0;
    *dos_time |= tm->tm_sec / 2;    /* Bits 0--4:  Second divided by two. */
    *dos_time |= tm->tm_min << 5;   /* Bits 5--10: Minute. */
    *dos_time |= tm->tm_hour << 11; /* Bits 11-15: Hour. */

    *dos_date = 0;
    *dos_date |= tm->tm_mday;             /* Bits 0--4:  Day (1--31). */
    *dos_date |= (tm->tm_mon + 1) << 5;   /* Bits 5--8:  Month (1--12). */
    *dos_date |= (tm->tm_year - 80) << 9; /* Bits 9--15: Year from 1980. */
}

static bool find_eocdr(struct eocdr *r, const uint8_t *src, size_t src_len)
{
    size_t comment_len;
    const uint8_t *p;
    uint32_t signature;

    for (comment_len = 0; comment_len <= UINT16_MAX; comment_len++) {
        if (src_len < EOCDR_BASE_SZ + comment_len) {
            break;
        }

        p = &src[src_len - EOCDR_BASE_SZ - comment_len];
        signature = READ32(p);

        if (signature == EOCDR_SIGNATURE) {
            r->disk_nbr = READ16(p);
            r->cd_start_disk = READ16(p);
            r->disk_cd_entries = READ16(p);
            r->cd_entries = READ16(p);
            r->cd_size = READ32(p);
            r->cd_offset = READ32(p);
            r->comment_len = READ16(p);
            r->comment = p;
            assert(p == &src[src_len - comment_len] &&
                   "All fields read.");

            if (r->comment_len == comment_len) {
                printf("ZIP archive is found\n");
                return true;
            }
        }
    }

    return false;
}


/* Local File Header. */
struct lfh {
    uint16_t extract_ver;
    uint16_t gp_flag;
    uint16_t method;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint16_t name_len;
    uint16_t extra_len;
    const uint8_t *name;
    const uint8_t *extra;
};

/* Size of a Local File Header, not including name and extra. */
#define LFH_BASE_SZ 30
#define LFH_SIGNATURE 0x04034b50 /* "PK\3\4" little-endian. */

static bool read_lfh(struct lfh *lfh, const uint8_t *src, size_t src_len,
                     size_t offset)
{
    const uint8_t *p;
    uint32_t signature;

    if (offset > src_len || src_len - offset < LFH_BASE_SZ) {
        return false;
    }

    p = &src[offset];
    signature = READ32(p);
    if (signature != LFH_SIGNATURE) {
        return false;
    }

    lfh->extract_ver = READ16(p);
    lfh->gp_flag = READ16(p);
    lfh->method = READ16(p);
    lfh->mod_time = READ16(p);
    lfh->mod_date = READ16(p);
    lfh->crc32 = READ32(p);
    lfh->comp_size = READ32(p);
    lfh->uncomp_size = READ32(p);
    lfh->name_len = READ16(p);
    lfh->extra_len = READ16(p);
    lfh->name = p;
    lfh->extra = lfh->name + lfh->name_len;
    assert(p == &src[offset + LFH_BASE_SZ] && "All fields read.");

    if (src_len - offset - LFH_BASE_SZ < lfh->name_len + lfh->extra_len) {
        return false;
    }

    return true;
}

typedef size_t zipiter_t; /* Zip archive member iterator. */

typedef struct zip_t zip_t;
struct zip_t {
    uint16_t num_members;    /* Number of members. */
    const uint8_t *comment;  /* Zip file comment (not terminated). */
    uint16_t comment_len;    /* Zip file comment length. */
    zipiter_t members_begin; /* Iterator to the first member. */
    zipiter_t members_end;   /* Iterator to the end of members. */

    const uint8_t *src;
    size_t src_len;
};


typedef enum { ZIP_STORED = 0, ZIP_DEFLATED = 8 } method_t;

typedef struct zipmemb_t zipmemb_t;
struct zipmemb_t {
    const uint8_t *name;      /* Member name (not null terminated). */
    uint16_t name_len;        /* Member name length. */
    time_t mtime;             /* Modification time. */
    uint32_t comp_size;       /* Compressed size. */
    const uint8_t *comp_data; /* Compressed data. */
    method_t method;          /* Compression method. */
    uint32_t uncomp_size;     /* Uncompressed size. */
    uint32_t crc32;           /* CRC-32 checksum. */
    const uint8_t *comment;   /* Comment (not null terminated). */
    uint16_t comment_len;     /* Comment length. */
    bool is_dir;              /* Whether this is a directory. */
    zipiter_t next;           /* Iterator to the next member. */
};

/* Initialize zip based on the source data. Returns true on success, or false
   if the data could not be parsed as a valid Zip file. */
bool zip_read(zip_t *zip, const uint8_t *src, size_t src_len)
{
    struct eocdr eocdr;
    struct cfh cfh;
    struct lfh lfh;
    size_t i, offset;
    const uint8_t *comp_data;

    zip->src = src;
    zip->src_len = src_len;

    if (!find_eocdr(&eocdr, src, src_len)) {
        return false;
    }

    if (eocdr.disk_nbr != 0 || eocdr.cd_start_disk != 0 ||
        eocdr.disk_cd_entries != eocdr.cd_entries) {
        return false; /* Cannot handle multi-volume archives. */
    }

    zip->num_members = eocdr.cd_entries;
    zip->comment = eocdr.comment;
    zip->comment_len = eocdr.comment_len;

    offset = eocdr.cd_offset;
    zip->members_begin = offset;

    /* Read the member info and do a few checks. */
    for (i = 0; i < eocdr.cd_entries; i++) {
        if (!read_cfh(&cfh, src, src_len, offset)) {
            return false;
        }

        if (cfh.gp_flag & 1) {
            return false; /* The member is encrypted. */
        }
        if (cfh.method != ZIP_STORED && cfh.method != ZIP_DEFLATED) {
            return false; /* Unsupported compression method. */
        }
        if (cfh.method == ZIP_STORED &&
            cfh.uncomp_size != cfh.comp_size) {
            return false;
        }
        if (cfh.disk_nbr_start != 0) {
            return false; /* Cannot handle multi-volume archives. */
        }
        if (memchr(cfh.name, '\0', cfh.name_len) != NULL) {
            return false; /* Bad filename. */
        }

        if (!read_lfh(&lfh, src, src_len, cfh.lfh_offset)) {
            return false;
        }

        comp_data = lfh.extra + lfh.extra_len;
        if (cfh.comp_size > src_len - (size_t)(comp_data - src)) {
            return false; /* Member data does not fit in src. */
        }

        offset += CFH_BASE_SZ + cfh.name_len + cfh.extra_len +
                  cfh.comment_len;
    }

    zip->members_end = offset;

    return true;
}

/* Get the Zip archive member through iterator it. */
zipmemb_t zip_member(const zip_t *zip, zipiter_t it)
{
    struct cfh cfh;
    struct lfh lfh;
    bool ok;
    zipmemb_t m;

    assert(it >= zip->members_begin && it < zip->members_end);

    ok = read_cfh(&cfh, zip->src, zip->src_len, it);
    assert(ok);

    ok = read_lfh(&lfh, zip->src, zip->src_len, cfh.lfh_offset);
    assert(ok);

    m.name = cfh.name;
    m.name_len = cfh.name_len;
    m.mtime = dos2ctime(cfh.mod_date, cfh.mod_time);
    m.comp_size = cfh.comp_size;
    m.comp_data = lfh.extra + lfh.extra_len;
    m.method = cfh.method;
    m.uncomp_size = cfh.uncomp_size;
    m.crc32 = cfh.crc32;
    m.comment = cfh.comment;
    m.comment_len = cfh.comment_len;
    m.is_dir = (cfh.ext_attrs & EXT_ATTR_DIR) != 0;

    m.next = it + CFH_BASE_SZ +
             cfh.name_len + cfh.extra_len + cfh.comment_len;

    assert(m.next <= zip->members_end);

    return m;
}

#define PERROR_IF(cond, msg) if (cond) { perror(msg); exit(1); }

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

static uint8_t *read_file(const char *filename, size_t *file_sz)
{
    FILE *f;
    uint8_t *buf;
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

static void list_zip(const char *filename)
{
    uint8_t *zip_data;
    size_t zip_sz;
    zip_t z;
    zipiter_t it;
    zipmemb_t m;

    printf("Listing ZIP archive: %s\n\n", filename);

    zip_data = read_file(filename, &zip_sz);

    if (!zip_read(&z, zip_data, zip_sz)) {
        printf("Failed to parse ZIP file!\n");
        exit(1);
    }

    if (z.comment_len != 0) {
        printf("%.*s\n\n", (int)z.comment_len, z.comment);
    }

    for (it = z.members_begin; it != z.members_end; it = m.next) {
        m = zip_member(&z, it);
        printf("%.*s\n", (int)m.name_len, m.name);
    }

    printf("\n");

    free(zip_data);
}

static void print_usage(const char *argv0)
{
    printf("Usage:\n\n");
    printf("  %s list <zipfile>\n", argv0);
    printf("\n");
}

int main(int argc, char **argv) {

    if (argc == 3 && strcmp(argv[1], "list") == 0) {
        list_zip(argv[2]);
    } else {
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
