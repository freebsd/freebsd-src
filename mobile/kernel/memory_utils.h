/*
 * Memory Utilities for uOS(m)
 * Simple implementations of standard memory functions
 */

#ifndef _MEMORY_UTILS_H_
#define _MEMORY_UTILS_H_

#include <stdint.h>
#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);

#endif /* _MEMORY_UTILS_H_ */