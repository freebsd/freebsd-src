/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 * Copyright (c) 2025-2026 Jean-Sébastien Pédron <dumbbell@FreeBSD.org>
 *
 * This software was developed by Jean-Sébastien Pédron under sponsorship
 * from the FreeBSD Foundation.
 */

#ifndef _LINUXKPI_LINUX_KMSG_DUMP_H_
#define	_LINUXKPI_LINUX_KMSG_DUMP_H_

#include <linux/errno.h>
#include <linux/list.h>

#include <linux/kernel.h> /* For pr_debug() */

enum kmsg_dump_reason {
	KMSG_DUMP_UNDEF,
	KMSG_DUMP_PANIC,
	KMSG_DUMP_OOPS,
	KMSG_DUMP_EMERG,
	KMSG_DUMP_SHUTDOWN,
	KMSG_DUMP_MAX
};

struct kmsg_dumper {
	struct list_head list;
	void (*dump)(struct kmsg_dumper *dumper, enum kmsg_dump_reason reason);
	enum kmsg_dump_reason max_reason;
	bool registered;
};

static inline int
kmsg_dump_register(struct kmsg_dumper *dumper)
{
	pr_debug("TODO");

	return (-EINVAL);
}

static inline int
kmsg_dump_unregister(struct kmsg_dumper *dumper)
{
	pr_debug("TODO");

	return (-EINVAL);
}

#endif
