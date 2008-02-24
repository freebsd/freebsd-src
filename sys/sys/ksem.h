/*-
 * Copyright (c) 2002 Alfred Perlstein <alfred@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/ksem.h,v 1.3 2006/11/11 16:15:35 trhodes Exp $
 */

#ifndef _POSIX4_KSEM_H_
#define	_POSIX4_KSEM_H_

#ifndef _KERNEL
#error "no user-servicable parts inside"
#endif

#include <sys/condvar.h>
#include <sys/queue.h>

struct kuser {
	pid_t ku_pid;
	LIST_ENTRY(kuser) ku_next;
};

struct ksem {
	LIST_ENTRY(ksem) ks_entry;	/* global list entry */
	int ks_onlist;			/* boolean if on a list (ks_entry) */
	char *ks_name;			/* if named, this is the name */
	int ks_ref;			/* number of references */
	mode_t ks_mode;			/* protection bits */
	uid_t ks_uid;			/* creator uid */
	gid_t ks_gid;			/* creator gid */
	unsigned int ks_value;		/* current value */
	struct cv ks_cv;		/* waiters sleep here */
	int ks_waiters;			/* number of waiters */
	LIST_HEAD(, kuser) ks_users;	/* pids using this sem */
	struct label *ks_label;		/* MAC label */
};

#endif /* !_POSIX4_KSEM_H_ */
