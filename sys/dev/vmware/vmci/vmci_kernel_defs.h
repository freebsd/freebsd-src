/*-
 * Copyright (c) 2018 VMware, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: (BSD-2-Clause AND GPL-2.0)
 *
 * $FreeBSD$
 */

/* Some common utilities used by the VMCI kernel module. */

#ifndef _VMCI_KERNEL_DEFS_H_
#define _VMCI_KERNEL_DEFS_H_

#include <sys/param.h>
#include <sys/systm.h>

typedef uint32_t PPN;

#define ASSERT(cond)		KASSERT(cond, (""))
#define ASSERT_ON_COMPILE(e)	_Static_assert(e, #e);

#define LIKELY(_exp)		__builtin_expect(!!(_exp), 1)
#define UNLIKELY(_exp)		__builtin_expect((_exp), 0)

#define CONST64U(c)		c##uL

#define ARRAYSIZE(a)		(sizeof(a) / sizeof(*(a)))

#define ROUNDUP(x, y)		(((x) + (y) - 1) / (y) * (y))
#define CEILING(x, y)		(((x) + (y) - 1) / (y))

#endif /* !_VMCI_KERNEL_DEFS_H_ */
