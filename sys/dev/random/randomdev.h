/*-
 * Copyright (c) 2000-2015 Mark R V Murray
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
 */

#ifndef SYS_DEV_RANDOM_RANDOMDEV_H_INCLUDED
#define	SYS_DEV_RANDOM_RANDOMDEV_H_INCLUDED

#ifdef _KERNEL

/* This header contains only those definitions that are global
 * and non algorithm-specific for the entropy processor
 */

#ifdef SYSCTL_DECL	/* from sysctl.h */
SYSCTL_DECL(_kern_random);
SYSCTL_DECL(_kern_random_initial_seeding);

#define	RANDOM_CHECK_UINT(name, min, max)				\
static int								\
random_check_uint_##name(SYSCTL_HANDLER_ARGS)				\
{									\
	if (oidp->oid_arg1 != NULL) {					\
		if (*(u_int *)(oidp->oid_arg1) <= (min))		\
			*(u_int *)(oidp->oid_arg1) = (min);		\
		else if (*(u_int *)(oidp->oid_arg1) > (max))		\
			*(u_int *)(oidp->oid_arg1) = (max);		\
	}								\
	return (sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,	\
		req));							\
}
#endif /* SYSCTL_DECL */

MALLOC_DECLARE(M_ENTROPY);

extern bool random_bypass_before_seeding;
extern bool read_random_bypassed_before_seeding;
extern bool arc4random_bypassed_before_seeding;
extern bool random_bypass_disable_warnings;

#endif /* _KERNEL */

struct harvest_event;

typedef void random_alg_pre_read_t(void);
typedef void random_alg_read_t(uint8_t *, size_t);
typedef bool random_alg_seeded_t(void);
typedef void random_alg_eventprocessor_t(struct harvest_event *);

typedef u_int random_source_read_t(void *, u_int);

/*
 * Random Algorithm is a processor of randomness for the kernel
 * and for userland.
 */
struct random_algorithm {
	const char			*ra_ident;
	u_int				 ra_poolcount;
	random_alg_pre_read_t		*ra_pre_read;
	random_alg_read_t		*ra_read;
	random_alg_seeded_t		*ra_seeded;
	random_alg_eventprocessor_t	*ra_event_processor;
};

#if defined(RANDOM_LOADABLE)
extern const struct random_algorithm *p_random_alg_context;
#else
extern const struct random_algorithm random_alg_context;
#define	p_random_alg_context (&random_alg_context)
#endif

#ifdef _KERNEL

/*
 * Random Source is a source of entropy that can provide
 * specified or approximate amount of entropy immediately
 * upon request.
 */
struct random_source {
	const char			*rs_ident;
	enum random_entropy_source	 rs_source;
	random_source_read_t		*rs_read;
};

void random_source_register(struct random_source *);
void random_source_deregister(struct random_source *);

#endif /* _KERNEL */

void randomdev_unblock(void);

#endif /* SYS_DEV_RANDOM_RANDOMDEV_H_INCLUDED */
