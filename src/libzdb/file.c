#include <stdlib.h>

#include <libzdb/file.h>

libzdb_file_t *libzdb_file_create(uint64_t size, uint64_t blocks) {
    libzdb_file_t *file = calloc(1, sizeof(libzdb_file_t));
    file->size = size;
    file->blocks = blocks;
    libzdb_list_init(&file->dvas);
    return file;
}

void libzdb_file_walk(libzdb_file_t *file,
                      void (*file_func)(libzdb_file_t *file, void *file_arg), void *file_arg,
                      void (*dva_func)(libzdb_dva_t *dva, void *dva_arg), void *dva_arg,
                      void (*rec_func)(libzdb_record_t *rec, void *rec_arg), void *rec_arg) {
    if (!file) {
        return;
    }

    /* header */
    if (file_func) {
        file_func(file, file_arg);
    }

    for(libzdb_node_t *dva_node = libzdb_list_head(&file->dvas); dva_node;
        dva_node = libzdb_list_next(dva_node)) {
        libzdb_dva_t *dva = libzdb_list_get(dva_node);

        /* stanza header */
        if (dva_func) {
            dva_func(dva, dva_arg);
        }

        for(libzdb_node_t *rec_node = libzdb_list_head(&dva->records); rec_node;
            rec_node = libzdb_list_next(rec_node)) {
            libzdb_record_t *rec = libzdb_list_get(rec_node);

            /* stanza body */
            if (rec_func) {
                rec_func(rec, rec_arg);
            }
        }
    }
}

static void print_header(libzdb_file_t *file, void *file_arg) {
    fprintf(file_arg, "file size: %zu (%zu blocks)\n", file->size, file->blocks);
}

static void print_stanza(libzdb_dva_t *dva, void *dva_arg) {
    fprintf(dva_arg, "file_offset=%ld vdev=%ld io_offset=%ld record_size=%ld\n",
            dva->file_offset, dva->vdev, dva->io_offset, dva->size);
}

static void print_body(libzdb_record_t *rec, void *rec_arg) {
    switch(rec->type) {
        case RAIDZ:
            fprintf(rec_arg, "col=%02lu ",
                    rec->col);
        case STRIPE:
        case MIRROR:
            fprintf(rec_arg, "vdevidx=%02lu dev=%s offset=%lu size=%lu\n",
                    rec->vdevidx,
                    rec->dev,
                    rec->offset,
                    rec->size);
            break;
        default:
            break;
    }
}

void libzdb_file_print(libzdb_file_t *file, FILE *out) {
    libzdb_file_walk(file,
                     print_header, out,
                     print_stanza, out,
                     print_body, out);
}

void libzdb_file_free(libzdb_file_t *file) {
    if (!file) {
        return;
    }

    for(libzdb_node_t *dva_node = libzdb_list_head(&file->dvas); dva_node;
        dva_node = libzdb_list_next(dva_node)) {
        libzdb_dva_t *dva = libzdb_list_get(dva_node);
        libzdb_list_fin(&dva->records, free);
    }

    libzdb_list_fin(&file->dvas, free);
    free(file);
}
