#ifndef LIBZDB_LIBNVPAIR_H
#define LIBZDB_LIBNVPAIR_H

#include <libnvpair.h> /* ZFS */

#include <libzdb/list.h>
#include <libzdb/zpool.h>

/* single vdev */
typedef struct vdev_info
{
  libzdb_list_t names;
  zpool_type_t type;
  size_t nparity;
  size_t ashift;
} vdi_t;

/* entire zpool */
typedef struct vdev_tree_info
{
  char *name;
  libzdb_list_t vdevs;
} vdti_t;

void libzdb_dump_nvlist(nvlist_t *list, int indent, const char *zpool_name,
                        vdti_t **zpool, vdi_t *vdev);

#endif
