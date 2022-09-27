#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>

#include <libzdb/file.h>
#include <libzdb/list.h>
#include <libzdb/reconstruct.h>

#define FILE_OFFSET "file_offset="
#define COL         "col="
#define VDEVIDX     "vdevidx="
#define DEV         "dev="

struct libzdb_backing_device {
    char *name;
    int fd;
};

static libzdb_backing_t *libzdb_backing_create(size_t count) {
    libzdb_backing_t *backing = calloc(1, sizeof(libzdb_backing_t));
    backing->count = count;
    backing->devs = calloc(count, sizeof(libzdb_backing_device_t));
    for(size_t i = 0; i < count; i++) {
        backing->devs[i].fd = -1;
    }
    return backing;
}

/* returns a pointer owned by backing */
static char *libzdb_backing_open(libzdb_backing_t *backing,
                          uint64_t vdevidx, const char *devname) {
    if (!backing) {
        return NULL;
    }

    if (vdevidx >= backing->count) {
        fprintf(stderr, "Error: Given vdev index is larger than the maximum vdev index: %" PRIu64 " > %zu\n", vdevidx, backing->count);
        return NULL;
    }

    libzdb_backing_device_t *dev = &backing->devs[vdevidx];
    if (dev->fd >= 0) {
        return dev->name;
    }

    free(dev->name);
    dev->name = NULL;

    size_t len = strlen(devname);
    dev->name = malloc(len + 1);
    memcpy(dev->name, devname, len);
    dev->name[len] = '\0';

    dev->fd = open(dev->name, O_RDWR);
    if (dev->fd < 0) {
        fprintf(stderr, "Error: Could not open backing device \"%s\"\n", dev->name);
        free(dev->name);
        dev->name = NULL;
        return NULL;
    }

    return dev->name;
}

void libzdb_backing_cleanup(libzdb_backing_t *backing) {
    if (!backing) {
        return;
    }

    for(size_t i = 0; i < backing->count; i++) {
        free(backing->devs[i].name);
        close(backing->devs[i].fd);
    }

    free(backing->devs);
    free(backing);
}

int libzdb_from_stream(FILE *input, size_t backing_count,
                       libzdb_file_t **file, libzdb_backing_t **backing) {
    // file size: %zu (%zu blocks)
    uint64_t size = 0;
    uint64_t blocks = 0;
    if (fscanf(input, "file size: %" PRIu64 " (%" PRIu64 " blocks)\n",
               &size, &blocks) != 2) {
        fprintf(stderr, "Error: Could not parse first line of input\n");
        return 1;
    }

    libzdb_backing_t *b = libzdb_backing_create(backing_count);
    libzdb_file_t *f = libzdb_file_create(size, blocks);

    // read stanzas
    char *line = NULL;
    size_t line_alloc = 0;
    ssize_t line_len = 0;
    libzdb_dva_t *dva = NULL;
    int rc = 0;
    while (((line_len = getline(&line, &line_len, input)) > -1) &&
           (rc == 0)) {
        // stanza start
        // file_offset=%zu vdev=%s io_offset=%zu record_size=%zu
        if (memcmp(line, FILE_OFFSET, sizeof(FILE_OFFSET) - 1) == 0) {
            uint64_t file_offset = 0;
            uint64_t vdev = 0;
            uint64_t io_offset = 0;
            uint64_t record_size = 0;

            static const char format[] = FILE_OFFSET "%" PRIu64 " vdev=%" PRIu64 " io_offset=%" PRIu64 " record_size=%" PRIu64 "\n";
            if (sscanf(line, format, &file_offset, &vdev, &io_offset, &record_size) != 4) {
                fprintf(stderr, "Error: Could not parse start of stanza: \"%s\"\n", line);
                rc = 1;
                break;
            }

            dva = malloc(sizeof(libzdb_dva_t));
            dva->file_offset = file_offset;
            dva->vdev = vdev;
            dva->io_offset = io_offset;
            dva->size = record_size;
            libzdb_list_init(&dva->records);
            libzdb_list_pushback(&f->dvas, dva);
        }
        // stanza body
        // col=%zu vdevidx=%zu dev=%s offset=%zu size=%zu
        else if (memcmp(line, COL, sizeof(COL) - 1) == 0) {
            if (!dva) {
                rc = 1;
                break;
            }

            uint64_t col = 0;
            uint64_t vdevidx = 0;
            char devname[4096];
            uint64_t offset = 0;
            uint64_t size = 0;

            static const char format[] = COL "%" PRIu64 " vdevidx=%" PRIu64 " dev=%s offset=%" PRIu64 " size=%" PRIu64;
            if (sscanf(line, format, &col, &vdevidx, devname, &offset, &size) != 5) {
                fprintf(stderr, "Error: Could not parse line: \"%s\"\n", line);
                rc = 1;
                break;
            }

            // save name of backing device and reuse string
            char *dev = libzdb_backing_open(b, vdevidx, devname);
            if (!dev) {
                rc = 1;
                break;
            }

            libzdb_record_t *rec = malloc(sizeof(libzdb_record_t));
            rec->type = RAIDZ;
            rec->col = col;
            rec->vdevidx = vdevidx;
            rec->dev = dev;
            rec->offset = offset;
            rec->size = size;
            libzdb_list_pushback(&dva->records, rec);
        }
        // stanza body
        // vdevidx=%zu dev=%s offset=%zu size=%zu
        else if (memcmp(line, VDEVIDX, sizeof(VDEVIDX) - 1) == 0) {
            if (!dva) {
                rc = 1;
                free(line);
                break;
            }

            uint64_t vdevidx = 0;
            char devname[4096];
            uint64_t offset = 0;
            uint64_t size = 0;

            static const char format[] = VDEVIDX "%" PRIu64 " dev=%s offset=%" PRIu64 " size=%" PRIu64;
            if (sscanf(line, format, &vdevidx, devname, &offset, &size) != 4) {
                fprintf(stderr, "Error: Could not parse line: \"%s\"\n", line);
                rc = 1;
                free(line);
                break;
            }

            // save name of backing device and reuse string
            char *dev = libzdb_backing_open(b, vdevidx, devname);
            if (!dev) {
                rc = 1;
                break;
            }

            libzdb_record_t *rec = malloc(sizeof(libzdb_record_t));
            rec->type = STRIPE | MIRROR;
            rec->col = 0; // doesn't matter
            rec->vdevidx = vdevidx;
            rec->dev = dev;
            rec->offset = offset;
            rec->size = size;
            libzdb_list_pushback(&dva->records, rec);
        }
        else {
            fprintf(stderr, "Error: Got unexpected data in first column of \"%s\"\n", line);
            rc = 1;
            break;
        }

        free(line);
        line = NULL;
    }

    free(line);

    if (rc != 0) {
        libzdb_file_free(f);
        libzdb_backing_cleanup(b);
        return rc;
    }

    *file = f;
    *backing = b;

    return 0;
}

/* ZFS record with the file's offset */
typedef struct lizdb_output {
    libzdb_record_t *record;   /* reference to item in dva */
    uint64_t file_offset;      /* dva offset + previous column sizes */
} libzdb_output_t;

/*
 * pass this function into the dva_func of libzdb_file_walk and an
 * array of lists of records as the argument
 *
 * this function will place all records located on the same backing
 * device into the same list
 */
static void libzdb_file_add_records(libzdb_dva_t *dva, void *dva_arg) {
    /* list of records */
    libzdb_list_t *recs = (libzdb_list_t *) dva_arg;

    size_t file_offset = dva->file_offset;
    for(libzdb_node_t *node = libzdb_list_head(&dva->records); node;
        node = libzdb_list_next(node)) {
        libzdb_record_t *rec = libzdb_list_get(node);

        libzdb_output_t *output = malloc(sizeof(libzdb_output_t));
        output->record = rec;
        output->file_offset = file_offset;

        libzdb_list_pushback(recs, output);

        /* accumulate record offsets (assumes records were processed in order) */
        file_offset += rec->size;
    }
}

/* process all records on the same backing device */
static int libzdb_read_write_records(libzdb_output_t *output,
                                      libzdb_backing_t *backing,
                                      int out) {
    /* if (!records || !backing) { */
    /*     return; */
    /* } */

    int in = backing->devs[output->record->vdevidx].fd;
    if (in < 0) {
        return 1;
    }

    if (out < 0) {
        return 1;
    }

    if (lseek(out, output->file_offset, SEEK_SET) == (off_t) -1) {
        const int err = errno;
        fprintf(stderr, "lseek: %s (%d)\n", strerror(err), err);
        return 1;
    }

    off_t offset = output->record->offset;
    if (sendfile(out, in, &offset, output->record->size) < 0) {
        const int err = errno;
        fprintf(stderr, "sendfile(%d -> %d): %s (%d)\n", in, out, strerror(err), err);
        return 1;
    }

    return 0;
}

/*
 * reconstruct a file using the information in libzdb_file_t
 *
 * If the libzdb_file_t was constructed by zdb, the records will
 * reference data from ZFS. Pass in NULL. The structure will be
 * generated and destroyed internally.
 *
 * If it was constructed from text, a libzdb_backing_t will be generated
 * and should be passed into this function.
 */
int libzdb_reconstruct(libzdb_file_t *file, libzdb_backing_t *backing,
                       size_t backing_count, const char *output_filename) {
    if (!file || !output_filename) {
        return 1;
    }

    int outfd = -1;
    int rc = 0;

    libzdb_list_t recs;
    libzdb_list_init(&recs);

    /* compute file offsets of records */
    libzdb_file_walk(file,
                     NULL, NULL,
                     libzdb_file_add_records, &recs,
                     NULL, NULL);

    /* cache backing device opens */
    int backing_exists = !!backing;
    if (!backing_exists) {
        backing = libzdb_backing_create(backing_count);
        for(libzdb_node_t *node = libzdb_list_head(&recs);
            node; node = libzdb_list_next(node)) {
            libzdb_output_t *output = libzdb_list_get(node);
            if (!libzdb_backing_open(backing,
                                     output->record->vdevidx,
                                     output->record->dev)) {
                libzdb_backing_cleanup(backing);
                backing = NULL;
                break;
            }
        }

        if (!backing) {
            rc = 1;
            goto cleanup;
        }
    }

    /* open reconstruction target */
    outfd = open(output_filename, O_RDWR | O_CREAT | O_TRUNC);
    if (outfd < 0) {
        const int err = errno;
        fprintf(stderr, "Error: Could not open output file: %s (%d)\n", strerror(errno), errno);
        rc = 1;
        goto cleanup;
    }

    /* resize output file to given size */
    if (ftruncate(outfd, file->size) < 0) {
        const int err = errno;
        fprintf(stderr, "Warning: Could not resize file: %s (%d)\n", strerror(errno), errno);
        rc = 1;
        goto cleanup;
    }

    /* reconstruct */
    for(libzdb_node_t *node = libzdb_list_head(&recs);
        node; node = libzdb_list_next(node)) {
        libzdb_output_t *output = libzdb_list_get(node);
        if (libzdb_read_write_records(output, backing, outfd) != 0) {
            rc = 1;
            break;
        }
    }

  cleanup:
    close(outfd);

    libzdb_list_fin(&recs, free);

    if (!backing_exists) {
        libzdb_backing_cleanup(backing);
    }

    return rc;
}
