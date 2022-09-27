#ifndef LIBZDB_LIST_H
#define LIBZDB_LIST_H

#include <stddef.h>

typedef struct libzdb_node libzdb_node_t;

/* zfs defines list at sys/list_impl.h */
typedef struct libzdb_list
{
  libzdb_node_t *head;
  libzdb_node_t *tail;
  size_t count;
} libzdb_list_t;

void libzdb_list_init (libzdb_list_t *list);
size_t libzdb_list_pushback (libzdb_list_t *list, void *new_value);
libzdb_node_t *libzdb_list_head (libzdb_list_t *list);
libzdb_node_t *libzdb_list_next (libzdb_node_t *node);
void *libzdb_list_get (libzdb_node_t *node);
void libzdb_list_fin (libzdb_list_t *list, void (*free_value) (void *));

#endif
