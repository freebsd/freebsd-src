/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef AME_COMPAT_H
#define AME_COMPAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AME_COMPAT_ABI_VERSION 1u

typedef enum ame_compat_status {
    AME_COMPAT_OK = 0,
    AME_COMPAT_INVALID_ARGUMENT = 1,
    AME_COMPAT_BUFFER_TOO_SMALL = 2,
    AME_COMPAT_NOT_FOUND = 3
} ame_compat_status;

typedef struct ame_compat_version {
    uint32_t abi_version;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} ame_compat_version;

/* Returns the ABI and layer version. The pointer may be NULL. */
ame_compat_status ame_compat_get_version(ame_compat_version *out);

/* Copies a NUL-terminated environment value into out. */
ame_compat_status ame_compat_getenv(
    const char *name,
    char *out,
    size_t out_len,
    size_t *required_len);

/* Sets an environment value for the current process. */
ame_compat_status ame_compat_setenv(
    const char *name,
    const char *value,
    int overwrite);

/* libc-compatible byte operations. They return the destination pointer. */
void *ame_compat_memmove(void *destination, const void *source, size_t length);
void *ame_compat_memset(void *destination, int value, size_t length);

#ifdef __cplusplus
}
#endif

#endif
