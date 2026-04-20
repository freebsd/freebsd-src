/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#ifndef _LINUXKPI_LINUX_ATOMIC_ATOMIC_INSTRUMENTED_H_
#define	_LINUXKPI_LINUX_ATOMIC_ATOMIC_INSTRUMENTED_H_

static inline int
atomic_read_acquire(const atomic_t *v)
{
	return (raw_atomic_read_acquire(v));
}

#endif /* _LINUXKPI_LINUX_ATOMIC_ATOMIC_INSTRUMENTED_H_ */
