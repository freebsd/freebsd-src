/* $FreeBSD$ */

#ifndef HASHTABLE_H
#define HASHTABLE_H 1

#include <sys/types.h>

#define HASH_OK         0
#define HASH_UPDATED    1
#define HASH_FAIL       2
#define HASH_FULL       3
#define HASH_NOTFOUND   4

#define HASHSTEP(x,c) (((x << 5) + x) + (c))

typedef struct {
  void *key;
  void *value;
} hashtable_entry;

typedef struct {
  size_t key_size;
  size_t table_size;
  size_t usage;
  size_t value_size;
  hashtable_entry **entries;
} hashtable;

void hashtable_free(hashtable *);
int hashtable_get(hashtable *, const void *, void *);
hashtable *hashtable_init(size_t, size_t, size_t);
int hashtable_put(hashtable *, const void *, const void *);
int hashtable_remove(hashtable *, const void *);

#endif	/* HASHTABLE.H */
