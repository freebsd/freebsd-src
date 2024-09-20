/*-
 * Copyright (c) 2017 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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
 */

#ifndef FS_P9FS_P9_DEBUG_H
#define FS_P9FS_P9_DEBUG_H

extern int p9_debug_level; /* All debugs on now */

/* 9P debug flags */
#define P9_DEBUG_TRANS			0x0001	/* Trace transport */
#define P9_DEBUG_SUBR			0x0002	/* Trace driver submissions */
#define P9_DEBUG_LPROTO			0x0004	/* Low level protocol tracing */
#define P9_DEBUG_PROTO			0x0008	/* High level protocol tracing */
#define P9_DEBUG_VOPS			0x0010	/* VOPs tracing */
#define P9_DEBUG_ERROR			0x0020	/* verbose error messages */

#define P9_DEBUG(category, fmt, ...) do {			\
	if ((p9_debug_level & P9_DEBUG_##category) != 0)	\
		printf(fmt, ##__VA_ARGS__);			\
} while (0)

#endif /* FS_P9FS_P9_DEBUG_H */
