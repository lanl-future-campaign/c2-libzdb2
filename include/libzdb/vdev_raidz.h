#ifndef LIBZDB_VDEV_RAIDZ
#define LIBZDB_VDEV_RAIDZ

#include <sys/zio.h> /* ZFS */

#include <libzdb/file.h>

void libzdb_vdev_raidz_map_alloc(zio_t *zio, uint64_t ashift, uint64_t dcols,
                                 uint64_t nparity, char **backing, uint64_t actual_size,
                                 libzdb_dva_t *dva);

#endif
