/*-
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
 * $FreeBSD: src/sbin/gpt/map.h,v 1.6.10.1 2010/02/10 00:26:20 kensmith Exp $
 */

#ifndef _MAP_H_
#define	_MAP_H_

typedef struct map {
	off_t	map_start;
	off_t	map_size;
	struct map *map_next;
	struct map *map_prev;
	int	map_type;
#define	MAP_TYPE_UNUSED		0
#define	MAP_TYPE_MBR		1
#define	MAP_TYPE_MBR_PART	2
#define	MAP_TYPE_PRI_GPT_HDR	3
#define	MAP_TYPE_SEC_GPT_HDR	4
#define	MAP_TYPE_PRI_GPT_TBL	5
#define	MAP_TYPE_SEC_GPT_TBL	6
#define	MAP_TYPE_GPT_PART	7
#define	MAP_TYPE_PMBR		8
	unsigned int map_index;
	void 	*map_data;
} map_t;

extern int lbawidth;

map_t *map_add(off_t, off_t, int, void*);
map_t *map_alloc(off_t, off_t);
map_t *map_find(int);
map_t *map_first(void);
map_t *map_last(void);

off_t map_free(off_t, off_t);

void map_init(off_t);

#endif /* _MAP_H_ */
