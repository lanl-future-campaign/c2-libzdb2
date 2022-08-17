#ifndef C2_VDEV_RAIDZ
#define C2_VDEV_RAIDZ

#include <sys/zio.h>

void c2_vdev_raidz_map_alloc(zio_t *zio, uint64_t ashift, uint64_t dcols,
                             uint64_t nparity, char **backing, uint64_t actual_size);

#endif
