#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <libzdb/libzdb.h>

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Syntax: %s zpool filename backing_count output\n", argv[0]);
        return 1;
    }

    size_t backing_count = 0;
    if (sscanf(argv[3], "%zu", &backing_count) != 1) {
        fprintf(stderr, "Error: Bad drive count: %s\n", argv[3]);
        return 1;
    }

    /* access physical blocks from ZFS */
    libzdb_init();
    libzdb_t *ctx = libzdb_ds_init(argv[1]);
    if (!ctx) {
        fprintf(stderr, "Error: Bad dataset name: %s\n", argv[1]);
        return 1;
    }

    /* get records from zpool */
    libzdb_file_t *file = libzdb_get_dvas(ctx, argv[2]);
    if (!file) {
        fprintf(stderr, "Error: Bad file name: %s\n", argv[2]);
        libzdb_fini();
        return 1;
    }

    /* reconstruct the file using libzdb_file_t */
    int rc = libzdb_reconstruct(file, NULL, backing_count, argv[4]);

    /* clean up */
    libzdb_file_free(file);
    libzdb_zpool_fini(ctx);
    libzdb_fini();

    return rc;
}
