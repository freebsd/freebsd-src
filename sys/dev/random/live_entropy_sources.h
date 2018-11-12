/*-
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
 * Copyright (c) 2013 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#ifndef SYS_DEV_RANDOM_LIVE_ENTROPY_SOURCES_H_INCLUDED
#define SYS_DEV_RANDOM_LIVE_ENTROPY_SOURCES_H_INCLUDED

typedef u_int random_live_read_func_t(void *, u_int);

/*
 * Live entropy source is a source of entropy that can provide
 * specified or approximate amount of entropy immediately upon request or within
 * an acceptable amount of time.
 */
struct live_entropy_source {
	const char			*les_ident;
	enum random_entropy_source	 les_source;
	random_live_read_func_t		*les_read;
};

struct live_entropy_sources {
	LIST_ENTRY(live_entropy_sources) lles_entries;	/* list of providers */
	struct live_entropy_source	*lles_rsource;	/* associated random adaptor */
};

extern struct mtx live_mtx;

void live_entropy_sources_init(void);
void live_entropy_sources_deinit(void);
void live_entropy_source_register(struct live_entropy_source *);
void live_entropy_source_deregister(struct live_entropy_source *);
void live_entropy_sources_feed(void);

#endif /* SYS_DEV_RANDOM_LIVE_ENTROPY_SOURCES_H_INCLUDED */
