/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ucred.h	8.4 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#ifndef _SYS_UCRED_H_
#define	_SYS_UCRED_H_

#include <sys/lock.h>		/* XXX */
#include <sys/mutex.h>		/* XXX */

/*
 * Credentials.
 *
 * Please do not inspect cr_uid directly to determine superuserness.
 * Only the suser()/suser_xxx() function should be used for this.
 */
struct ucred {
	u_int	cr_ref;			/* reference count */
	uid_t	cr_uid;			/* effective user id */
	short	cr_ngroups;		/* number of groups */
	gid_t	cr_groups[NGROUPS];	/* groups */
	struct	uidinfo *cr_uidinfo;	/* per uid resource consumption */
	struct	prison *cr_prison;	/* jail(4) */
	struct	mtx cr_mtx;		/* protect refcount */
};
#define cr_gid cr_groups[0]
#define NOCRED ((struct ucred *)0)	/* no credential available */
#define FSCRED ((struct ucred *)-1)	/* filesystem credential */

/*
 * This is the external representation of struct ucred, based upon the
 * size of a 4.2-RELEASE struct ucred.  There will probably never be
 * any need to change the size of this or layout of its used fields.
 */
struct xucred {
	u_short	_cr_unused0;		/* compatibility with old ucred */
	uid_t	cr_uid;			/* effective user id */
	short	cr_ngroups;		/* number of groups */
	gid_t	cr_groups[NGROUPS];	/* groups */
	void	*_cr_unused1;		/* compatibility with old ucred */
};

#ifdef _KERNEL

struct proc;

void		change_euid __P((struct proc *p, uid_t euid));
void		change_ruid __P((struct proc *p, uid_t ruid));
struct ucred	*crcopy __P((struct ucred *cr));
struct ucred	*crdup __P((struct ucred *cr));
void		crfree __P((struct ucred *cr));
struct ucred	*crget __P((void));
void		crhold __P((struct ucred *cr));
int		groupmember __P((gid_t gid, struct ucred *cred));
#endif /* _KERNEL */

#endif /* !_SYS_UCRED_H_ */
