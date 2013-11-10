/*-
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
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

#ifndef SYS_DEV_RANDOM_RANDOM_ADAPTORS_H_INCLUDED
#define SYS_DEV_RANDOM_RANDOM_ADAPTORS_H_INCLUDED

MALLOC_DECLARE(M_ENTROPY);

struct random_adaptor {
	struct selinfo		rsel;
	const char		*ident;
	int			seeded;
	u_int			priority;
	random_init_func_t	*init;
	random_deinit_func_t	*deinit;
	random_block_func_t	*block;
	random_read_func_t	*read;
	random_poll_func_t	*poll;
	random_reseed_func_t	*reseed;
};

struct random_adaptors {
	LIST_ENTRY(random_adaptors) entries;	/* list of providers */
	const char		*name;		/* name of random adaptor */
	struct random_adaptor	*rsp;
};

void random_adaptor_register(const char *, struct random_adaptor *);
void random_adaptor_deregister(const char *);
void random_adaptor_choose(void);

extern struct random_adaptor *random_adaptor;

#endif /* SYS_DEV_RANDOM_RANDOM_ADAPTORS_H_INCLUDED */
