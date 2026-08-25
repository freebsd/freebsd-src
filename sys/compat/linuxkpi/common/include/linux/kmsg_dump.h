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

struct kmsg_dump_iter {
	uint64_t	cur_seq;
	uint64_t	next_seq;
};

struct kmsg_dump_detail {
	enum kmsg_dump_reason reason;
	const char *description;
};

struct kmsg_dumper {
	struct list_head list;
#if defined(LINUXKPI_VERSION) && LINUXKPI_VERSION < 61200
	void (*dump)(struct kmsg_dumper *dumper, enum kmsg_dump_reason reason);
#else
	void (*dump)(struct kmsg_dumper *dumper, struct kmsg_dump_detail *detail);
#endif
	enum kmsg_dump_reason max_reason;
	bool registered;
};

static inline void
kmsg_dump(enum kmsg_dump_reason reason)
{
	pr_debug("TODO");
}

static inline bool
kmsg_dump_get_line(struct kmsg_dump_iter *iter, bool syslog,
    const char *line, size_t size, size_t *len)
{
	pr_debug("TODO");

	return (false);
}

static inline bool
kmsg_dump_get_buffer(struct kmsg_dump_iter *iter, bool syslog,
    char *buf, size_t size, size_t *len)
{
	pr_debug("TODO");

	return (false);
}

static inline void
kmsg_dump_rewind(struct kmsg_dump_iter *iter)
{
	pr_debug("TODO");
}

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

static inline const char *
kmsg_dump_reason_str(enum kmsg_dump_reason reason)
{
	pr_debug("TODO");

	return ("Unknown");
}
#endif
