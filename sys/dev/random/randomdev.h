/*-
 * Copyright (c) 2000-2004 Mark R V Murray
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
 * $FreeBSD: src/sys/dev/random/randomdev.h,v 1.7.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

/* This header contains only those definitions that are global
 * and non algorithm-specific for the entropy processor
 */

typedef void random_init_func_t(void);
typedef void random_deinit_func_t(void);
typedef int random_block_func_t(int);
typedef int random_read_func_t(void *, int);
typedef void random_write_func_t(void *, int);
typedef int random_poll_func_t(int, struct thread *);
typedef void random_reseed_func_t(void);

struct random_systat {
	struct selinfo		rsel;
	const char		*ident;
	int			seeded;
	random_init_func_t	*init;
	random_deinit_func_t	*deinit;
	random_block_func_t	*block;
	random_read_func_t	*read;
	random_write_func_t	*write;
	random_poll_func_t	*poll;
	random_reseed_func_t	*reseed;
};

extern struct random_systat random_systat;

extern void random_ident_hardware(struct random_systat *);
extern void random_null_func(void);
