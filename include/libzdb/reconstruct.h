#include <stdio.h>

#include <libzdb/file.h>

/* cache backing device names and file descriptors */
/* stored in array where index is vdevidx */
typedef struct libzdb_backing_device libzdb_backing_device_t;
typedef struct libzdb_backing {
    size_t count;
    libzdb_backing_device_t *devs;
} libzdb_backing_t;

int libzdb_from_stream(FILE *input, size_t backing_count,
                       libzdb_file_t **file, libzdb_backing_t **backing);

void libzdb_backing_cleanup(libzdb_backing_t *backing);

int libzdb_reconstruct(libzdb_file_t *file, libzdb_backing_t *backing,
                       size_t backing_count, const char *output_filename);
