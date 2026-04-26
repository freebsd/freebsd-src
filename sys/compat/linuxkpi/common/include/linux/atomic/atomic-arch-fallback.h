/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#ifndef _LINUXKPI_LINUX_ATOMIC_ATOMIC_ARCH_FALLBACK_H_
#define	_LINUXKPI_LINUX_ATOMIC_ATOMIC_ARCH_FALLBACK_H_

static inline int
raw_atomic_read_acquire(const atomic_t *v)
{
	return (atomic_load_acq_int(&v->counter));
}

#endif /* _LINUXKPI_LINUX_ATOMIC_ATOMIC_ARCH_FALLBACK_H_ */
