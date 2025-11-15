/*
 * Copyright 2022, Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

/* TODO: openzfs/include/sys/uio_impl.h must not be included in stand */
#ifndef _SYS_UIO_IMPL_H
#define _SYS_UIO_IMPL_H
#endif

/*
 * sys/atomic.h must be included after sys/sysmacros.h. The latter includes
 * machine/atomic.h, which interferes. Sadly, upstream includes them in the
 * wrong order, so we include it here to fix that.
 */
#include <sys/sysmacros.h>

#include_next <sys/zfs_context.h>

#define	SYSCTL_HANDLER_ARGS void

/*
 * Not sure why I need these, but including the canonical stand.h fails because
 * the normal string.h doesn't like all the other shenanigans in this environment.
 */
void *memcpy(void *dst, const void *src, size_t len);
void *memset(void *dest, int c, size_t len);
void *memmem(const void *big, size_t big_len, const void *little,
         size_t little_len);
