/*-
 * Copyright (c) 2000-2013 Mark R V Murray
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

#ifndef SYS_DEV_RANDOM_RANDOMDEV_H_INCLUDED
#define SYS_DEV_RANDOM_RANDOMDEV_H_INCLUDED

/* This header contains only those definitions that are global
 * and non algorithm-specific for the entropy processor
 */

typedef void random_init_func_t(void);
typedef void random_deinit_func_t(void);

void randomdev_init_harvester(void (*)(const void *, u_int, u_int, enum random_entropy_source));
void randomdev_init_reader(u_int (*)(uint8_t *, u_int));
void randomdev_deinit_harvester(void);
void randomdev_deinit_reader(void);

/* Stub/fake routines for when no entropy processor is loaded */
extern u_int dummy_random_read_phony(uint8_t *, u_int);

/* kern.random sysctls */
#ifdef SYSCTL_DECL	/* from sysctl.h */
SYSCTL_DECL(_kern_random);

/* If this was C++, the macro below would be a template */
#define RANDOM_CHECK_UINT(name, min, max)				\
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

#endif
