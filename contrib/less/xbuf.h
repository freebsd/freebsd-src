/*
 * Copyright (C) 1984-2026  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

#ifndef XBUF_H_
#define XBUF_H_

#include "lang.h"

struct xbuffer
{
	unsigned char *data;
	size_t end;
	size_t size;
	size_t init_size;
};

void xbuf_init(struct xbuffer *xbuf);
void xbuf_init_size(struct xbuffer *xbuf, size_t init_size);
void xbuf_deinit(struct xbuffer *xbuf);
void xbuf_reset(struct xbuffer *xbuf);
void xbuf_add_byte(struct xbuffer *xbuf, unsigned char b);
void xbuf_add_char(struct xbuffer *xbuf, char c);
void xbuf_add_data(struct xbuffer *xbuf, constant void *data, size_t len);
int xbuf_pop(struct xbuffer *xbuf);
constant char *xbuf_char_data(constant struct xbuffer *xbuf);

#endif
