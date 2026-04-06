/*
 * Memory Utilities Implementation
 * uOS(m) - User OS Mobile
 */

#include "memory_utils.h"

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memset(void *dest, int c, size_t n) {
    uint8_t *d = dest;
    while (n--) *d++ = (uint8_t)c;
    return dest;
}