/*
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 * Copyright (c) 2025-2026 Jean-Sébastien Pédron <dumbbell@FreeBSD.org>
 *
 * This software was developed by Jean-Sébastien Pédron under sponsorship
 * from the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <linux/seq_buf.h>
 
void
linuxkpi_seq_buf_init(struct seq_buf *s, char *buf, unsigned int size)
{
	s->buffer = buf;
	s->size = size;

	seq_buf_clear(s);
}
 
int
linuxkpi_seq_buf_printf(struct seq_buf *s, const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = seq_buf_vprintf(s, fmt, args);
	va_end(args);

	return (ret);
}
 
int
linuxkpi_seq_buf_vprintf(struct seq_buf *s, const char *fmt, va_list args)
{
	int ret;

	if (!seq_buf_has_overflowed(s)) {
		ret = vsnprintf(s->buffer + s->len, s->size - s->len, fmt, args);
		if (s->len + ret < s->size) {
			s->len += ret;
			return (0);
		}
	}

	seq_buf_set_overflow(s);
	return (-1);
}

const char *
linuxkpi_seq_buf_str(struct seq_buf *s)
{
	if (s->size == 0)
		return ("");

	if (seq_buf_buffer_left(s))
		s->buffer[s->len] = '\0';
	else
		s->buffer[s->size - 1] = '\0';

	return (s->buffer);
}
