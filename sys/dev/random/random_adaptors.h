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

typedef void random_adaptor_init_func_t(void);
typedef void random_adaptor_deinit_func_t(void);
typedef void random_adaptor_read_func_t(uint8_t *, u_int);
typedef void random_adaptor_write_func_t(uint8_t *, u_int);
typedef int random_adaptor_seeded_func_t(void);
typedef void random_adaptor_reseed_func_t(void);

struct random_adaptor {
	const char			*ra_ident;
	int				 ra_priority;
	random_adaptor_init_func_t	*ra_init;
	random_adaptor_deinit_func_t	*ra_deinit;
	random_adaptor_read_func_t	*ra_read;
	random_adaptor_write_func_t	*ra_write;
	random_adaptor_reseed_func_t	*ra_reseed;
	random_adaptor_seeded_func_t	*ra_seeded;
};

struct random_adaptors {
	LIST_ENTRY(random_adaptors) rra_entries;	/* list of providers */
	const char		*rra_name;		/* name of random adaptor */
	struct random_adaptor	*rra_ra;
};

/* Dummy "always-block" pseudo-device */
extern struct random_adaptor randomdev_dummy;

void random_adaptors_init(void);
void random_adaptors_deinit(void);

void random_adaptor_register(const char *, struct random_adaptor *);
void random_adaptor_deregister(const char *);

int random_adaptor_read(struct cdev *, struct uio *, int);
int random_adaptor_write(struct cdev *, struct uio *, int);
int random_adaptor_poll(struct cdev *, int, struct thread *);

int random_adaptor_read_rate(void);
void random_adaptor_unblock(void);

#endif /* SYS_DEV_RANDOM_RANDOM_ADAPTORS_H_INCLUDED */
