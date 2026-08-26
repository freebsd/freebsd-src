/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#ifndef _SYS_NETEXPORT_H_
#define _SYS_NETEXPORT_H_

/*
 * This file must be included after net/radix.h so that "struct radix_node"
 * is defined.
 */
#ifdef _KERNEL
/*
 * Network address lookup element
 */
struct netcred {
	struct	radix_node netc_rnodes[2];
	uint64_t netc_exflags;
	struct	ucred *netc_anon;
	int	netc_numsecflavors;
	int	netc_secflavors[MAXSECFLAVORS];
};

/*
 * Network export information
 * ne_defexported - Protected by mnt_explock.
 * ne4 - Protected by mnt_explock.
 * ne6 - Protected by mnt_explock.
 * The following fields are used by the pNFS server's replenisher process.
 * ne_pnfsnumfile - Protected by the ne_mtx mutex.
 * ne_pnfsnextfile - Protected by the vnode lock for the numfiles directory.
 * ne_pnfsnumcnt - Handled as an atomic.
 *                 (Although this value doesn't need to be exact, so using
 *                  an atomic is not really necessary.)
 */
struct netexport {
#define	ne_startzero	ne_defexported
	struct	netcred ne_defexported;		/* Default export */
	struct 	radix_node_head	*ne4;
	struct 	radix_node_head	*ne6;
#define	ne_endzero	ne_mtx
	struct	mtx ne_mtx;			/* For ne_pnfsnumfile. */
	struct	vnode *ne_pnfsnumfile;
	uint64_t ne_pnfsnextfile;
	u_int	ne_ref;				/* Refcount for structure */
	u_int	ne_pnfsnumcnt;			/* For stats, not protected. */
};

#define	MNTEXP_LOCK(n)		mtx_lock(&(n)->ne_mtx)
#define	MNTEXP_UNLOCK(n)	mtx_unlock(&(n)->ne_mtx)
#define	MNTEXP_MTX(n)		(&(n)->ne_mtx)

#define	PNFSD_START		((struct vnode *)-1)
#define	PNFSD_STOP		((struct vnode *)-2)
#define	PNFSD_STOPPED		((struct vnode *)-3)

#endif	/* _KERNEL */

#endif /* !_SYS_NETEXPORT_H_ */
