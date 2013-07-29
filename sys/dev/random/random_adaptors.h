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

#ifndef __RANDOM_ADAPTORS_H__
#define __RANDOM_ADAPTORS_H__

#include <sys/eventhandler.h>

struct random_adaptors {
	LIST_ENTRY(random_adaptors) entries;	/* list of providesr */
	const char		*name;		/* name of random adaptor */
	struct random_adaptor	*rsp;
};

struct random_adaptor *random_adaptor_get(const char *);
int random_adaptor_register(const char *, struct random_adaptor *);

/*
 * random_adaptor's should be registered prior to
 * random module (SI_SUB_DRIVERS/SI_ORDER_MIDDLE)
 */
#define RANDOM_ADAPTOR_MODULE(name, modevent, ver)		\
    static moduledata_t name##_mod = {				\
	#name,							\
	modevent,						\
	0							\
    };								\
    DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS,		\
		   SI_ORDER_SECOND);				\
    MODULE_VERSION(name, ver);					\
    MODULE_DEPEND(name, random, 1, 1, 1);

typedef void (*random_adaptor_attach_hook)(void *, struct random_adaptor *);
EVENTHANDLER_DECLARE(random_adaptor_attach, random_adaptor_attach_hook);

/* kern.random sysctls */
#ifdef SYSCTL_DECL	/* from sysctl.h */
SYSCTL_DECL(_kern_random);
#endif /* SYSCTL_DECL */

#endif /* __RANDOM_ADAPTORS_H__ */
