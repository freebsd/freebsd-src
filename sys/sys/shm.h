/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: @(#)shm.h	7.2 (Berkeley) 2/5/91
 *	$Id: shm.h,v 1.5 1993/11/07 17:53:00 wollman Exp $
 */

/*
 * SVID compatible shm.h file
 */
#ifndef _SHM_H_
#define _SHM_H_

#ifdef KERNEL
#include "ipc.h"
#else
#include <sys/ipc.h>
#endif

struct shmid_ds {
	struct	ipc_perm shm_perm;	/* operation perms */
	int	shm_segsz;		/* size of segment (bytes) */
	ushort	shm_cpid;		/* pid, creator */
	ushort	shm_lpid;		/* pid, last operation */
	short	shm_nattch;		/* no. of current attaches */
	time_t	shm_atime;		/* last attach time */
	time_t	shm_dtime;		/* last detach time */
	time_t	shm_ctime;		/* last change time */
	void	*shm_handle;		/* internal handle for shm segment */
};

/*
 * System 5 style catch-all structure for shared memory constants that
 * might be of interest to user programs.  Do we really want/need this?
 */
struct	shminfo {
	int	shmmax;		/* max shared memory segment size (bytes) */
	int	shmmin;		/* min shared memory segment size (bytes) */
	int	shmmni;		/* max number of shared memory identifiers */
	int	shmseg;		/* max shared memory segments per process */
	int	shmall;		/* max amount of shared memory (pages) */
};

/* internal "mode" bits */
#define	SHM_ALLOC	01000	/* segment is allocated */
#define	SHM_DEST	02000	/* segment will be destroyed on last detach */

/* SVID required constants (same values as system 5) */
#define	SHM_RDONLY	010000	/* read-only access */
#define	SHM_RND		020000	/* round attach address to SHMLBA boundary */

/* implementation constants */
#define	SHMLBA		CLBYTES	/* segment low boundary address multiple */
#define	SHMMMNI		512	/* maximum value for shminfo.shmmni */

#ifdef KERNEL
extern struct	shmid_ds	*shmsegs;
extern struct	shminfo		shminfo;
#endif

#ifndef KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS

int shmsys __P((int, ...));

void *shmat  __P((int, void *, int));
int shmget __P((key_t, int, int));
int shmctl __P((int, int, void *));
int shmdt  __P((void *));

__END_DECLS

#endif

#endif /* !_SHM_H_ */
