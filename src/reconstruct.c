#include <stdio.h>

#include <libzdb/libzdb.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Syntax: %s backing_count output_file\n", argv[0]);
        return 1;
    }

    size_t backing_count = 0;
    if (sscanf(argv[1], "%zu", &backing_count) != 1) {
        fprintf(stderr, "Error: Bad backing count %s\n", argv[1]);
        return 1;
    }

    libzdb_file_t *file = NULL;
    libzdb_backing_t *backing = NULL;
    if (libzdb_from_stream(stdin, backing_count, &file, &backing) != 0) {
        return 1;
    }

    int rc = libzdb_reconstruct(file, backing, backing->count, argv[2]);

    libzdb_file_free(file);
    libzdb_backing_cleanup(backing);

    return rc;
}
