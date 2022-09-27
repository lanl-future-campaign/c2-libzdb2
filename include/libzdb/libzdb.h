#include <libzdb/dump.h>
#include <libzdb/reconstruct.h>

/* libzdb context */
typedef struct libzdb libzdb_t;

/* call these functions in this order */

/* global initialization */
void libzdb_init();

/* cache reusable dataset data */
libzdb_t *libzdb_ds_init(char *zpool_name);

/* do operations here */

/* clean up zpool vdevs cache */
void libzdb_zpool_fini(libzdb_t *ctx);

/* global teardown */
void libzdb_fini();
