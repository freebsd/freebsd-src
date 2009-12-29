/*-
 * Copyright 2006-2008 John Birrell <jb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 * Statically Defined Tracing (SDT) definitions.
 *
 */

#ifndef _SYS_SDT_H
#define	_SYS_SDT_H

/* Stub these for the time being. */
#define	DTRACE_PROBE(name)
#define	DTRACE_PROBE1(name, type1, arg1)
#define	DTRACE_PROBE2(name, type1, arg1, type2, arg2)
#define	DTRACE_PROBE3(name, type1, arg1, type2, arg2, type3, arg3)
#define	DTRACE_PROBE4(name, type1, arg1, type2, arg2, type3, arg3, type4, arg4)

#ifndef _KERNEL

/* The promise of things to come. Worlds to explore. People to meet. Things to do. */

#else

#ifndef KDTRACE_HOOKS

#define SDT_PROVIDER_DEFINE(prov)
#define SDT_PROVIDER_DECLARE(prov)
#define SDT_PROBE_DEFINE(prov, mod, func, name)
#define SDT_PROBE_DECLARE(prov, mod, func, name)
#define SDT_PROBE(prov, mod, func, name, arg0, arg1, arg2, arg3, arg4)
#define SDT_PROBE_ARGTYPE(prov, mod, func, name, num, type)

#define	SDT_PROBE_DEFINE1(prov, mod, func, name, arg0)
#define	SDT_PROBE_DEFINE2(prov, mod, func, name, arg0, arg1)
#define	SDT_PROBE_DEFINE3(prov, mod, func, name, arg0, arg1, arg2)
#define	SDT_PROBE_DEFINE4(prov, mod, func, name, arg0, arg1, arg2, arg3)
#define	SDT_PROBE_DEFINE5(prov, mod, func, name, arg0, arg1, arg2, arg3, arg4)

#else

/*
 * This type definition must match that of dtrace_probe. It is defined this
 * way to avoid having to rely on CDDL code.
 */
typedef	void (*sdt_probe_func_t)(u_int32_t, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);

/*
 * The hook for the probe function. See kern_sdt.c which defaults this to
 * it's own stub. The 'sdt' provider will set it to dtrace_probe when it
 * loads.
 */
extern sdt_probe_func_t	sdt_probe_func;

typedef enum {
	SDT_UNINIT = 1,
	SDT_INIT,
} sdt_state_t;

struct sdt_probe;
struct sdt_provider;

struct sdt_argtype {
	int	ndx;			/* Argument index. */
	const char *type;		/* Argument type string. */
	TAILQ_ENTRY(sdt_argtype)
			argtype_entry;	/* Argument type list entry. */
	struct sdt_probe
			*probe;		/* Ptr to the probe structure. */
};

struct sdt_probe {
	int		version;	/* Set to sizeof(struct sdt_ref). */
	sdt_state_t	state;
	struct sdt_provider
			*prov;		/* Ptr to the provider structure. */
	TAILQ_ENTRY(sdt_probe)
			probe_entry;	/* SDT probe list entry. */
	TAILQ_HEAD(argtype_list_head, sdt_argtype) argtype_list;
	const char	*mod;
	const char	*func;
	const char	*name;
	id_t		id;		/* DTrace probe ID. */
	int		n_args;		/* Number of arguments. */
};

struct sdt_provider {
	const char *name;		/* Provider name. */
	TAILQ_ENTRY(sdt_provider)
			prov_entry;	/* SDT provider list entry. */
	TAILQ_HEAD(probe_list_head, sdt_probe) probe_list;
	uintptr_t	id;		/* DTrace provider ID. */
};

#define SDT_PROVIDER_DEFINE(prov)						\
	struct sdt_provider sdt_provider_##prov[1] = {				\
		{ #prov, { NULL, NULL }, { NULL, NULL } }			\
	};									\
	SYSINIT(sdt_provider_##prov##_init, SI_SUB_KDTRACE, 			\
	    SI_ORDER_SECOND, sdt_provider_register, 				\
	    sdt_provider_##prov );						\
	SYSUNINIT(sdt_provider_##prov##_uninit, SI_SUB_KDTRACE,			\
	    SI_ORDER_SECOND, sdt_provider_deregister, 				\
	    sdt_provider_##prov )

#define SDT_PROVIDER_DECLARE(prov)						\
	extern struct sdt_provider sdt_provider_##prov[1]

#define SDT_PROBE_DEFINE(prov, mod, func, name)					\
	struct sdt_probe sdt_##prov##_##mod##_##func##_##name[1] = {		\
		{ sizeof(struct sdt_probe), 0, sdt_provider_##prov,		\
		    { NULL, NULL }, { NULL, NULL }, #mod, #func, #name, 0, 0 }	\
	};									\
	SYSINIT(sdt_##prov##_##mod##_##func##_##name##_init, SI_SUB_KDTRACE, 	\
	    SI_ORDER_SECOND + 1, sdt_probe_register, 				\
	    sdt_##prov##_##mod##_##func##_##name );				\
	SYSUNINIT(sdt_##prov##_##mod##_##func##_##name##_uninit, 		\
	    SI_SUB_KDTRACE, SI_ORDER_SECOND + 1, sdt_probe_deregister, 		\
	    sdt_##prov##_##mod##_##func##_##name )

#define SDT_PROBE_DECLARE(prov, mod, func, name)				\
	extern struct sdt_probe sdt_##prov##_##mod##_##func##_##name[1]

#define SDT_PROBE(prov, mod, func, name, arg0, arg1, arg2, arg3, arg4)		\
	if (sdt_##prov##_##mod##_##func##_##name->id)				\
		(*sdt_probe_func)(sdt_##prov##_##mod##_##func##_##name->id,	\
		    (uintptr_t) arg0, (uintptr_t) arg1, (uintptr_t) arg2,	\
		    (uintptr_t) arg3, (uintptr_t) arg4)

#define SDT_PROBE_ARGTYPE(prov, mod, func, name, num, type)			\
	struct sdt_argtype sdt_##prov##_##mod##_##func##_##name##num[1]		\
	    = { { num, type, { NULL, NULL },					\
	    sdt_##prov##_##mod##_##func##_##name }				\
	};									\
	SYSINIT(sdt_##prov##_##mod##_##func##_##name##num##_init,		\
	    SI_SUB_KDTRACE, SI_ORDER_SECOND + 2, sdt_argtype_register, 		\
	    sdt_##prov##_##mod##_##func##_##name##num );			\
	SYSUNINIT(sdt_##prov##_##mod##_##func##_##name##num##_uninit, 		\
	    SI_SUB_KDTRACE, SI_ORDER_SECOND + 2, sdt_argtype_deregister,	\
	    sdt_##prov##_##mod##_##func##_##name##num )

#define	SDT_PROBE_DEFINE1(prov, mod, func, name, arg0)			\
	SDT_PROBE_DEFINE(prov, mod, func, name);			\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 0, arg0)

#define	SDT_PROBE_DEFINE2(prov, mod, func, name, arg0, arg1)		\
	SDT_PROBE_DEFINE(prov, mod, func, name);			\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 0, arg0)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 1, arg1)

#define	SDT_PROBE_DEFINE3(prov, mod, func, name, arg0, arg1, arg2)	\
	SDT_PROBE_DEFINE(prov, mod, func, name);			\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 0, arg0)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 1, arg1)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 2, arg2)

#define	SDT_PROBE_DEFINE4(prov, mod, func, name, arg0, arg1, arg2, arg3) \
	SDT_PROBE_DEFINE(prov, mod, func, name);			\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 0, arg0)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 1, arg1)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 2, arg2)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 3, arg3)

#define	SDT_PROBE_DEFINE5(prov, mod, func, name, arg0, arg1, arg2, arg3, arg4) \
	SDT_PROBE_DEFINE(prov, mod, func, name);			\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 0, arg0)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 1, arg1)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 2, arg2)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 3, arg3)		\
	SDT_PROBE_ARGTYPE(prov, mod, func, name, 4, arg4)

typedef int (*sdt_argtype_listall_func_t)(struct sdt_argtype *, void *);
typedef int (*sdt_probe_listall_func_t)(struct sdt_probe *, void *);
typedef int (*sdt_provider_listall_func_t)(struct sdt_provider *, void *);

void sdt_argtype_deregister(void *);
void sdt_argtype_register(void *);
void sdt_probe_deregister(void *);
void sdt_probe_register(void *);
void sdt_provider_deregister(void *);
void sdt_provider_register(void *);
void sdt_probe_stub(u_int32_t, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2,
    uintptr_t arg3, uintptr_t arg4);
int sdt_argtype_listall(struct sdt_probe *, sdt_argtype_listall_func_t, void *);
int sdt_probe_listall(struct sdt_provider *, sdt_probe_listall_func_t, void *);
int sdt_provider_listall(sdt_provider_listall_func_t,void *);

#endif /* KDTRACE_HOOKS */

#endif /* _KERNEL */

#endif /* _SYS_SDT_H */
