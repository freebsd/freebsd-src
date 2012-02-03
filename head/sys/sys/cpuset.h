/*-
 * Copyright (c) 2008,	Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Copyright (c) 2008 Nokia Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#ifndef _SYS_CPUSET_H_
#define	_SYS_CPUSET_H_

#include <sys/_cpuset.h>

#define	CPUSETBUFSIZ	((2 + sizeof(long) * 2) * _NCPUWORDS)

#define	__cpuset_mask(n)	((long)1 << ((n) % _NCPUBITS))
#define	CPU_CLR(n, p)	((p)->__bits[(n)/_NCPUBITS] &= ~__cpuset_mask(n))
#define	CPU_COPY(f, t)	(void)(*(t) = *(f))
#define	CPU_ISSET(n, p)	(((p)->__bits[(n)/_NCPUBITS] & __cpuset_mask(n)) != 0)
#define	CPU_SET(n, p)	((p)->__bits[(n)/_NCPUBITS] |= __cpuset_mask(n))
#define	CPU_ZERO(p) do {				\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		(p)->__bits[__i] = 0;			\
} while (0)

#define	CPU_FILL(p) do {				\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		(p)->__bits[__i] = -1;			\
} while (0)

#define	CPU_SETOF(n, p) do {					\
	CPU_ZERO(p);						\
	((p)->__bits[(n)/_NCPUBITS] = __cpuset_mask(n));	\
} while (0)

/* Is p empty. */
#define	CPU_EMPTY(p) __extension__ ({			\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		if ((p)->__bits[__i])			\
			break;				\
	__i == _NCPUWORDS;				\
})

/* Is p full set. */
#define	CPU_ISFULLSET(p) __extension__ ({		\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		if ((p)->__bits[__i] != (long)-1)	\
			break;				\
	__i == _NCPUWORDS;				\
})

/* Is c a subset of p. */
#define	CPU_SUBSET(p, c) __extension__ ({		\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		if (((c)->__bits[__i] &			\
		    (p)->__bits[__i]) !=		\
		    (c)->__bits[__i])			\
			break;				\
	__i == _NCPUWORDS;				\
})

/* Are there any common bits between b & c? */
#define	CPU_OVERLAP(p, c) __extension__ ({		\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		if (((c)->__bits[__i] &			\
		    (p)->__bits[__i]) != 0)		\
			break;				\
	__i != _NCPUWORDS;				\
})

/* Compare two sets, returns 0 if equal 1 otherwise. */
#define	CPU_CMP(p, c) __extension__ ({			\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		if (((c)->__bits[__i] !=		\
		    (p)->__bits[__i]))			\
			break;				\
	__i != _NCPUWORDS;				\
})

#define	CPU_OR(d, s) do {				\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		(d)->__bits[__i] |= (s)->__bits[__i];	\
} while (0)

#define	CPU_AND(d, s) do {				\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		(d)->__bits[__i] &= (s)->__bits[__i];	\
} while (0)

#define	CPU_NAND(d, s) do {				\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		(d)->__bits[__i] &= ~(s)->__bits[__i];	\
} while (0)

#define	CPU_CLR_ATOMIC(n, p)						\
	atomic_clear_long(&(p)->__bits[(n)/_NCPUBITS], __cpuset_mask(n))

#define	CPU_SET_ATOMIC(n, p)						\
	atomic_set_long(&(p)->__bits[(n)/_NCPUBITS], __cpuset_mask(n))

/* Convenience functions catering special cases. */ 
#define	CPU_OR_ATOMIC(d, s) do {			\
	__size_t __i;					\
	for (__i = 0; __i < _NCPUWORDS; __i++)		\
		atomic_set_long(&(d)->__bits[__i],	\
		    (s)->__bits[__i]);			\
} while (0)

#define	CPU_COPY_STORE_REL(f, t) do {				\
	__size_t __i;						\
	for (__i = 0; __i < _NCPUWORDS; __i++)			\
		atomic_store_rel_long(&(t)->__bits[__i],	\
		    (f)->__bits[__i]);				\
} while (0)

/*
 * Valid cpulevel_t values.
 */
#define	CPU_LEVEL_ROOT		1	/* All system cpus. */
#define	CPU_LEVEL_CPUSET	2	/* Available cpus for which. */
#define	CPU_LEVEL_WHICH		3	/* Actual mask/id for which. */

/*
 * Valid cpuwhich_t values.
 */
#define	CPU_WHICH_TID		1	/* Specifies a thread id. */
#define	CPU_WHICH_PID		2	/* Specifies a process id. */
#define	CPU_WHICH_CPUSET	3	/* Specifies a set id. */
#define	CPU_WHICH_IRQ		4	/* Specifies an irq #. */
#define	CPU_WHICH_JAIL		5	/* Specifies a jail id. */

/*
 * Reserved cpuset identifiers.
 */
#define	CPUSET_INVALID	-1
#define	CPUSET_DEFAULT	0

#ifdef _KERNEL
LIST_HEAD(setlist, cpuset);

/*
 * cpusets encapsulate cpu binding information for one or more threads.
 *
 * 	a - Accessed with atomics.
 *	s - Set at creation, never modified.  Only a ref required to read.
 *	c - Locked internally by a cpuset lock.
 *
 * The bitmask is only modified while holding the cpuset lock.  It may be
 * read while only a reference is held but the consumer must be prepared
 * to deal with inconsistent results.
 */
struct cpuset {
	cpuset_t		cs_mask;	/* bitmask of valid cpus. */
	volatile u_int		cs_ref;		/* (a) Reference count. */
	int			cs_flags;	/* (s) Flags from below. */
	cpusetid_t		cs_id;		/* (s) Id or INVALID. */
	struct cpuset		*cs_parent;	/* (s) Pointer to our parent. */
	LIST_ENTRY(cpuset)	cs_link;	/* (c) All identified sets. */
	LIST_ENTRY(cpuset)	cs_siblings;	/* (c) Sibling set link. */
	struct setlist		cs_children;	/* (c) List of children. */
};

#define CPU_SET_ROOT    0x0001  /* Set is a root set. */
#define CPU_SET_RDONLY  0x0002  /* No modification allowed. */

extern cpuset_t *cpuset_root;
struct prison;
struct proc;

struct cpuset *cpuset_thread0(void);
struct cpuset *cpuset_ref(struct cpuset *);
void	cpuset_rel(struct cpuset *);
int	cpuset_setthread(lwpid_t id, cpuset_t *);
int	cpuset_create_root(struct prison *, struct cpuset **);
int	cpuset_setproc_update_set(struct proc *, struct cpuset *);
int	cpusetobj_ffs(const cpuset_t *);
char	*cpusetobj_strprint(char *, const cpuset_t *);
int	cpusetobj_strscan(cpuset_t *, const char *);

#else
__BEGIN_DECLS
int	cpuset(cpusetid_t *);
int	cpuset_setid(cpuwhich_t, id_t, cpusetid_t);
int	cpuset_getid(cpulevel_t, cpuwhich_t, id_t, cpusetid_t *);
int	cpuset_getaffinity(cpulevel_t, cpuwhich_t, id_t, size_t, cpuset_t *);
int	cpuset_setaffinity(cpulevel_t, cpuwhich_t, id_t, size_t, const cpuset_t *);
__END_DECLS
#endif
#endif /* !_SYS_CPUSET_H_ */
