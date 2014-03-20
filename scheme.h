/*-
 * Copyright (c) 2013,2014 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MKIMG_SCHEME_H_
#define	_MKIMG_SCHEME_H_

struct mkimg_alias {
	const char *name;
	uintptr_t tp;
#define	ALIAS_PTR(p)	(uintptr_t)(p)
#define	ALIAS_INT(i)	(uintptr_t)(i)
};

struct mkimg_scheme {
	const char *name;
	const char *description;
	int	nparts;
	int	padding;
	struct mkimg_alias *aliases;
	off_t (*get_leader)(u_int);
	off_t (*get_trailer)(u_int);
};

SET_DECLARE(schemes, struct mkimg_scheme);
#define	SCHEME_DEFINE(nm)	DATA_SET(schemes, nm)

int	scheme_select(const char *);
struct mkimg_scheme *scheme_selected(void);

int scheme_check_part(struct part *);
u_int scheme_max_parts(void);
off_t scheme_first_offset(u_int);
off_t scheme_next_offset(off_t, uint64_t);
void scheme_write(int, off_t);

#endif /* _MKIMG_SCHEME_H_ */
