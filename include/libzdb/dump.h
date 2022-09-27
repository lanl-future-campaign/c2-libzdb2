#ifndef LIBZDB_DUMP_H
#define LIBZDB_DUMP_H

#include <libzdb/file.h>

/* libzdb context */
typedef struct libzdb libzdb_t;

/* look up data records (can be called multiple times for the same dataset) */
/* the output is valid as long as the context is valid */
libzdb_file_t *libzdb_get_dvas(libzdb_t *ctx, char *path);

#endif
