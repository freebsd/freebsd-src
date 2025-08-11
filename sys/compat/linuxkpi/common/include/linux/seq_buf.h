/*
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 * Copyright (c) 2025-2026 Jean-Sébastien Pédron <dumbbell@FreeBSD.org>
 *
 * This software was developed by Jean-Sébastien Pédron under sponsorship
 * from the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _LINUXKPI_LINUX_SEQ_BUF_H_
#define	_LINUXKPI_LINUX_SEQ_BUF_H_

#include <linux/bug.h>
#include <linux/minmax.h>
#include <linux/seq_file.h>
#include <linux/types.h>

struct seq_buf {
	char	*buffer;
	size_t	 size;
	size_t	 len;
};

#define DECLARE_SEQ_BUF(NAME, SIZE)			\
	struct seq_buf NAME = {				\
		.buffer = (char[SIZE]) { 0 },		\
		.size = SIZE,				\
	}

static inline void
seq_buf_clear(struct seq_buf *s)
{
	s->len = 0;
	if (s->size > 0)
		s->buffer[0] = '\0';
}

static inline void
seq_buf_set_overflow(struct seq_buf *s)
{
	s->len = s->size + 1;
}

static inline bool
seq_buf_has_overflowed(struct seq_buf *s)
{
	return (s->len > s->size);
}

static inline bool
seq_buf_buffer_left(struct seq_buf *s)
{
	if (seq_buf_has_overflowed(s))
		return (0);

	return (s->size - s->len);
}

#define	seq_buf_init(s, buf, size) linuxkpi_seq_buf_init((s), (buf), (size))
void linuxkpi_seq_buf_init(struct seq_buf *s, char *buf, unsigned int size);

#define	seq_buf_printf(s, f, ...) linuxkpi_seq_buf_printf((s), (f), __VA_ARGS__)
int linuxkpi_seq_buf_printf(struct seq_buf *s, const char *fmt, ...) \
    __printflike(2, 3);

#define	seq_buf_vprintf(s, f, a) linuxkpi_seq_buf_vprintf((s), (f), (a))
int linuxkpi_seq_buf_vprintf(struct seq_buf *s, const char *fmt, va_list args);

#define	seq_buf_str(s) linuxkpi_seq_buf_str((s))
const char * linuxkpi_seq_buf_str(struct seq_buf *s);

#endif
