/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#ifndef _SYS_EXTERRVAR_H_
#define	_SYS_EXTERRVAR_H_

#include <sys/_exterr.h>
#include <sys/_uexterror.h>
#include <sys/exterr_cat.h>

#define	UEXTERROR_MAXLEN	256

#define	UEXTERROR_VER		0x10010001

#define	EXTERRCTL_ENABLE	1
#define	EXTERRCTL_DISABLE	2

#define	EXTERRCTLF_FORCE	0x00000001

#ifdef _KERNEL

#ifndef EXTERR_CATEGORY
#error "Specify error category before including sys/exterrvar.h"
#endif

#ifdef	BLOAT_KERNEL_WITH_EXTERR
#define	SET_ERROR_MSG(mmsg)	_Td->td_kexterr.msg = mmsg
#else
#define	SET_ERROR_MSG(mmsg)	_Td->td_kexterr.msg = NULL
#endif

#define	SET_ERROR2(eerror, mmsg, pp1, pp2) do {	\
	struct thread *_Td = curthread;				\
	if ((_Td->td_pflags2 & TDP2_UEXTERR) != 0) {		\
		_Td->td_pflags2 |= TDP2_EXTERR;			\
		_Td->td_kexterr.error = eerror;			\
		_Td->td_kexterr.cat = EXTERR_CATEGORY;		\
		SET_ERROR_MSG(mmsg);				\
		_Td->td_kexterr.p1 = (uintptr_t)pp1;		\
		_Td->td_kexterr.p2 = (uintptr_t)pp2;		\
		_Td->td_kexterr.src_line = __LINE__;		\
	}							\
} while (0)
#define	SET_ERROR0(eerror, mmsg)	SET_ERROR2(eerror, mmsg, 0, 0)
#define	SET_ERROR1(eerror, mmsg, pp1)	SET_ERROR2(eerror, mmsg, pp1, 0)

int exterr_to_ue(struct thread *td, struct uexterror *ue);

#else	/* _KERNEL */

#include <sys/types.h>

__BEGIN_DECLS
int exterrctl(u_int op, u_int flags, void *ptr);
__END_DECLS

#endif	/* _KERNEL */

#endif
