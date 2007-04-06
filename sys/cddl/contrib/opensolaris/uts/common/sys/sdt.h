/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_SDT_H
#define	_SYS_SDT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _KERNEL

#define	DTRACE_PROBE(provider, name) {					\
	extern void __dtrace_##provider##___##name(void);		\
	__dtrace_##provider##___##name();				\
}

#define	DTRACE_PROBE1(provider, name, arg1) {				\
	extern void __dtrace_##provider##___##name(unsigned long);	\
	__dtrace_##provider##___##name((unsigned long)arg1);		\
}

#define	DTRACE_PROBE2(provider, name, arg1, arg2) {			\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long);						\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2);					\
}

#define	DTRACE_PROBE3(provider, name, arg1, arg2, arg3) {		\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long);				\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3);			\
}

#define	DTRACE_PROBE4(provider, name, arg1, arg2, arg3, arg4) {		\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long);		\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4);					\
}

#define	DTRACE_PROBE5(provider, name, arg1, arg2, arg3, arg4, arg5) {	\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long);\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4, (unsigned long)arg5);			\
}

#else /* _KERNEL */

#define	DTRACE_PROBE(name)	{					\
	extern void __dtrace_probe_##name(void);			\
	__dtrace_probe_##name();					\
}

#define	DTRACE_PROBE1(name, type1, arg1) {				\
	extern void __dtrace_probe_##name(uintptr_t);			\
	__dtrace_probe_##name((uintptr_t)(arg1));			\
}

#define	DTRACE_PROBE2(name, type1, arg1, type2, arg2) {			\
	extern void __dtrace_probe_##name(uintptr_t, uintptr_t);	\
	__dtrace_probe_##name((uintptr_t)(arg1), (uintptr_t)(arg2));	\
}

#define	DTRACE_PROBE3(name, type1, arg1, type2, arg2, type3, arg3) {	\
	extern void __dtrace_probe_##name(uintptr_t, uintptr_t, uintptr_t); \
	__dtrace_probe_##name((uintptr_t)(arg1), (uintptr_t)(arg2),	\
	    (uintptr_t)(arg3));						\
}

#define	DTRACE_PROBE4(name, type1, arg1, type2, arg2, 			\
    type3, arg3, type4, arg4) {						\
	extern void __dtrace_probe_##name(uintptr_t, uintptr_t,		\
	    uintptr_t, uintptr_t);					\
	__dtrace_probe_##name((uintptr_t)(arg1), (uintptr_t)(arg2),	\
	    (uintptr_t)(arg3), (uintptr_t)(arg4));			\
}

#define	DTRACE_SCHED(name)						\
	DTRACE_PROBE(__sched_##name);

#define	DTRACE_SCHED1(name, type1, arg1)				\
	DTRACE_PROBE1(__sched_##name, type1, arg1);

#define	DTRACE_SCHED2(name, type1, arg1, type2, arg2)			\
	DTRACE_PROBE2(__sched_##name, type1, arg1, type2, arg2);

#define	DTRACE_SCHED3(name, type1, arg1, type2, arg2, type3, arg3)	\
	DTRACE_PROBE3(__sched_##name, type1, arg1, type2, arg2, type3, arg3);

#define	DTRACE_SCHED4(name, type1, arg1, type2, arg2, 			\
    type3, arg3, type4, arg4)						\
	DTRACE_PROBE4(__sched_##name, type1, arg1, type2, arg2, 	\
	    type3, arg3, type4, arg4);

#define	DTRACE_PROC(name)						\
	DTRACE_PROBE(__proc_##name);

#define	DTRACE_PROC1(name, type1, arg1)					\
	DTRACE_PROBE1(__proc_##name, type1, arg1);

#define	DTRACE_PROC2(name, type1, arg1, type2, arg2)			\
	DTRACE_PROBE2(__proc_##name, type1, arg1, type2, arg2);

#define	DTRACE_PROC3(name, type1, arg1, type2, arg2, type3, arg3)	\
	DTRACE_PROBE3(__proc_##name, type1, arg1, type2, arg2, type3, arg3);

#define	DTRACE_PROC4(name, type1, arg1, type2, arg2, 			\
    type3, arg3, type4, arg4)						\
	DTRACE_PROBE4(__proc_##name, type1, arg1, type2, arg2, 		\
	    type3, arg3, type4, arg4);

#define	DTRACE_IO(name)							\
	DTRACE_PROBE(__io_##name);

#define	DTRACE_IO1(name, type1, arg1)					\
	DTRACE_PROBE1(__io_##name, type1, arg1);

#define	DTRACE_IO2(name, type1, arg1, type2, arg2)			\
	DTRACE_PROBE2(__io_##name, type1, arg1, type2, arg2);

#define	DTRACE_IO3(name, type1, arg1, type2, arg2, type3, arg3)	\
	DTRACE_PROBE3(__io_##name, type1, arg1, type2, arg2, type3, arg3);

#define	DTRACE_IO4(name, type1, arg1, type2, arg2, 			\
    type3, arg3, type4, arg4)						\
	DTRACE_PROBE4(__io_##name, type1, arg1, type2, arg2, 		\
	    type3, arg3, type4, arg4);

#define	DTRACE_SYSEVENT2(name, type1, arg1, type2, arg2)		\
	DTRACE_PROBE2(__sysevent_##name, type1, arg1, type2, arg2);

#endif /* _KERNEL */

extern const char *sdt_prefix;

typedef struct sdt_probedesc {
	char			*sdpd_name;	/* name of this probe */
	unsigned long		sdpd_offset;	/* offset of call in text */
	struct sdt_probedesc	*sdpd_next;	/* next static probe */
} sdt_probedesc_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SDT_H */
