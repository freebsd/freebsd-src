/*
 * string.h compatibility shim
 * Public domain
 */

#include_next <string.h>

#ifndef DIFFCOMPAT_STRING_H
#define DIFFCOMPAT_STRING_H

#include <sys/types.h>

size_t strlcpy(char *dst, const char *src, size_t dstsize);
size_t strlcat(char *dst, const char *src, size_t dstsize);

#endif
