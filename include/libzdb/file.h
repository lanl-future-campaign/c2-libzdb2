#ifndef LIBZDB_FILE_H
#define LIBZDB_FILE_H

#include <stdint.h>
#include <stdio.h>

#include <libzdb/list.h>
#include <libzdb/zpool.h>

/* single record */
typedef struct libzdb_record
{
    zpool_type_t type;
    size_t col;           /* only set if type is raidz */

    /* common to all zpool types */
    uint64_t vdevidx;
    char *dev;            /* never owned by this struct - always a reference */
    uint64_t offset;
    uint64_t size;
} libzdb_record_t;

/* single dva */
typedef struct libzdb_dva
{
    uint64_t file_offset;
    uint64_t vdev;
    uint64_t io_offset;
    uint64_t size;
    libzdb_list_t records; /* libzdb_record_t */
} libzdb_dva_t;

/* all dvas */
typedef struct libzdb_file
{
    size_t size;
    size_t blocks;
    libzdb_list_t dvas;    /* libzdb_dva_t */
} libzdb_file_t;

libzdb_file_t *libzdb_file_create(uint64_t size, uint64_t blocks);

/* generic function for walking a file dvas */
void libzdb_file_walk(libzdb_file_t *file,
                      void (*file_func)(libzdb_file_t *file, void *file_arg), void *file_arg,
                      void (*dva_func)(libzdb_dva_t *dva, void *dva_arg), void *dva_arg,
                      void (*rec_func)(libzdb_record_t *rec, void *rec_arg), void *rec_arg);

/* specialization of libzdb_file_walk */
void libzdb_file_print(libzdb_file_t *file, FILE *out);

void libzdb_file_free(libzdb_file_t *file);

#endif
