/*
 * stdlib.h compatibility shim
 * Public domain
 */

#include_next <stdlib.h>

#ifndef DIFFCOMPAT_STDLIB_H
#define DIFFCOMPAT_STDLIB_H

#include <sys/types.h>
#include <stdint.h>

const char * getprogname(void);

void *reallocarray(void *, size_t, size_t);
void *recallocarray(void *, size_t, size_t, size_t);
int mergesort(void *, size_t, size_t, int (*cmp)(const void *, const void *));

#endif
