#include <libzdb/list.h>

#include <stdlib.h>
#include <string.h>

struct libzdb_node
{
  void *value;
  libzdb_node_t *next;
};

void
libzdb_list_init (libzdb_list_t *list)
{
  if (list)
    {
      memset (list, 0, sizeof (libzdb_list_t));
    }
}

size_t
libzdb_list_pushback (libzdb_list_t *list, void *new_value)
{
  if (!list)
    {
      return 0;
    }

  libzdb_node_t *new_libzdb_node = malloc (sizeof (libzdb_node_t));
  new_libzdb_node->value = new_value;
  new_libzdb_node->next = NULL;

  if (!list->head)
    {
      list->head = new_libzdb_node;
    }

  if (list->tail)
    {
      list->tail->next = new_libzdb_node;
    }

  list->tail = new_libzdb_node;
  list->count++;

  return list->count;
}

libzdb_node_t *
libzdb_list_head (libzdb_list_t *list)
{
  return list ? list->head : NULL;
}

libzdb_node_t *
libzdb_list_next (libzdb_node_t *libzdb_node)
{
  return libzdb_node ? libzdb_node->next : NULL;
}

void *
libzdb_list_get (libzdb_node_t *libzdb_node)
{
  return libzdb_node ? libzdb_node->value : NULL;
}

void
libzdb_list_fin (libzdb_list_t *list, void (*free_value) (void *))
{
  libzdb_node_t *libzdb_node = libzdb_list_head (list);
  while (libzdb_node)
    {
      libzdb_node_t *next = libzdb_list_next (libzdb_node);
      if (free_value)
        {
          free_value (libzdb_node->value);
        }

      free (libzdb_node);
      libzdb_node = next;
    }
}
