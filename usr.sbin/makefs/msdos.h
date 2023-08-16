/*	$NetBSD: msdos.h,v 1.3 2015/10/16 16:40:02 christos Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MAKEFS_MSDOS_H
#define _MAKEFS_MSDOS_H

#define NOCRED NULL

#define MSDOSFS_DPRINTF(args) do {	\
	if (debug & DEBUG_MSDOSFS)	\
		printf args;		\
} while (0);


struct denode;
struct fsnode;
struct msdosfsmount;

struct componentname {
	char *cn_nameptr;
	size_t cn_namelen;
};

struct m_vnode;
struct m_buf;

int msdosfs_fsiflush(struct msdosfsmount *);
struct msdosfsmount *msdosfs_mount(struct m_vnode *);
int msdosfs_root(struct msdosfsmount *, struct m_vnode *);

struct denode *msdosfs_mkfile(const char *, struct denode *, fsnode *);
struct denode *msdosfs_mkdire(const char *, struct denode *, fsnode *);

int m_readde(struct denode *dep, struct m_buf **bpp, struct direntry **epp);
int m_readep(struct msdosfsmount *pmp, u_long dirclust, u_long diroffset,
    struct m_buf **bpp, struct direntry **epp);
int m_extendfile(struct denode *dep, u_long count, struct m_buf **bpp,
    u_long *ncp, int flags);

struct msdosfsmount *m_msdosfs_mount(struct m_vnode *devvp);
#endif
