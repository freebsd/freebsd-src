/*
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "map.h"

static int lbawidth;
static map_t *mediamap;

static map_t *
mkmap(off_t start, off_t size, int type)
{
	map_t *m;

	m = malloc(sizeof(*m));
	if (m == NULL)
		return (NULL);
	m->map_start = start;
	m->map_size = size;
	m->map_next = m->map_prev = NULL;
	m->map_type = type;
	m->map_data = NULL;
	return (m);
}

map_t *
map_add(off_t start, off_t size, int type, void *data)
{
	map_t *m, *n, *p;

	n = mediamap;
	while (n != NULL && n->map_start + n->map_size <= start)
		n = n->map_next;
	if (n == NULL)
		return (NULL);

	if (n->map_start + n->map_size < start + size) {
		warnx("error: bogus map");
		return (0);
	}

	if (n->map_start == start && n->map_size == size) {
		if (n->map_type != MAP_TYPE_UNUSED)
			warnx("warning: partition(%llu,%llu) mirrored",
			    (long long)start, (long long)size);
		n->map_type = type;
		n->map_data = data;
		return (n);
	}

	if (n->map_type != MAP_TYPE_UNUSED) {
		warnx(
    "error: partition(%llu,%llu) overlaps partition(%llu,%llu)",
		    (long long)start, (long long)size,
		    (long long)n->map_start, (long long)n->map_size);
		return (0);
	}

	m = mkmap(start, size, type);
	if (m == NULL)
		return (NULL);

	m->map_data = data;

	if (start == n->map_start) {
		m->map_prev = n->map_prev;
		m->map_next = n;
		if (m->map_prev != NULL)
			m->map_prev->map_next = m;
		else
			mediamap = m;
		n->map_prev = m;
		n->map_start += size;
		n->map_size -= size;
	} else if (start + size == n->map_start + n->map_size) {
		p = n;
		m->map_next = p->map_next;
		m->map_prev = p;
		if (m->map_next != NULL)
			m->map_next->map_prev = m;
		p->map_next = m;
		p->map_size -= size;
	} else {
		p = mkmap(n->map_start, start - n->map_start, n->map_type);
		n->map_start += p->map_size + m->map_size;
		n->map_size -= (p->map_size + m->map_size);
		p->map_prev = n->map_prev;
		m->map_prev = p;
		n->map_prev = m;
		m->map_next = n;
		p->map_next = m;
		if (p->map_prev != NULL)
			p->map_prev->map_next = p;
		else
			mediamap = p;
	}

	return (m);
}

map_t *
map_find(int type)
{
	map_t *m;

	m = mediamap;
	while (m != NULL && m->map_type != type)
		m = m->map_next;
	return (m);
}

map_t *
map_first(void)
{
	return mediamap;
}

map_t *
map_last(void)
{
	map_t *m;

	m = mediamap;
	while (m != NULL && m->map_next != NULL)
		m = m->map_next;
	return (m);
}

off_t
map_unused(off_t start, off_t size)
{
	map_t *m;

	m = mediamap;

	while (m != NULL && m->map_start + m->map_size <= start)
		m = m->map_next;
	if (m == NULL || m->map_type != MAP_TYPE_UNUSED)
		return (0LL);
	if (size)
		return ((m->map_start + m->map_size >= start + size) ? 1 : 0);
	return (m->map_size - (start - m->map_start));
}

void
map_dump(void)
{
	off_t end;
	map_t *m;

	printf("  %*s", lbawidth, "start");
	printf("  %*s", lbawidth, "end");
	printf("  %*s", lbawidth, "size");
	printf("  %s\n", "contents");

	m = mediamap;
	while (m != NULL) {
		end = m->map_start + m->map_size - 1;
		printf("  %*llu", lbawidth, (long long)m->map_start);
		printf("  %*llu", lbawidth, (long long)end);
		printf("  %*llu", lbawidth, (long long)m->map_size);

		putchar(' '); putchar(' ');
		switch (m->map_type) {
		case MAP_TYPE_MBR:
			printf("MBR");
			break;
		case MAP_TYPE_PRI_GPT_HDR:
			printf("Pri GPT header");
			break;
		case MAP_TYPE_SEC_GPT_HDR:
			printf("Sec GPT header");
			break;
		case MAP_TYPE_PRI_GPT_TBL:
			printf("Pri GPT table");
			break;
		case MAP_TYPE_SEC_GPT_TBL:
			printf("Sec GPT table");
			break;
		case MAP_TYPE_MBR_PART:
			printf("MBR partition");
			break;
		case MAP_TYPE_GPT_PART:
			printf("GPT partition");
			break;
		}
		putchar('\n');
		m = m->map_next;
	}
}

void
map_init(off_t size)
{
	char buf[32];

	mediamap = mkmap(0LL, size, MAP_TYPE_UNUSED);
	lbawidth = sprintf(buf, "%llu", (long long)size);
	if (lbawidth < 5)
		lbawidth = 5;
}
