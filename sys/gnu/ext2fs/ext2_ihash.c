/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993, 1995
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
 *	@(#)ufs_ihash.c	8.7 (Berkeley) 5/17/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_extern.h>

static MALLOC_DEFINE(M_EXT2IHASH, "EXT2 ihash", "EXT2 Inode hash tables");
/*
 * Structures associated with inode cacheing.
 */
static LIST_HEAD(ihashhead, inode) *ihashtbl;
static u_long	ihash;		/* size of hash table - 1 */
#define	INOHASH(device, inum)	(&ihashtbl[(minor(device) + (inum)) & ihash])
static struct mtx ext2_ihash_mtx;

/*
 * Initialize inode hash table.
 */
void
ext2_ihashinit()
{

	KASSERT(ihashtbl == NULL, ("ext2_ihashinit called twice"));
	ihashtbl = hashinit(desiredvnodes, M_EXT2IHASH, &ihash);
	mtx_init(&ext2_ihash_mtx, "ext2 ihash", NULL, MTX_DEF);
}

/*
 * Destroy the inode hash table.
 */
void
ext2_ihashuninit()
{

	hashdestroy(ihashtbl, M_EXT2IHASH, ihash);
	mtx_destroy(&ext2_ihash_mtx);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct vnode *
ext2_ihashlookup(dev, inum)
	dev_t dev;
	ino_t inum;
{
	struct inode *ip;

	mtx_lock(&ext2_ihash_mtx);
	LIST_FOREACH(ip, INOHASH(dev, inum), i_hash)
		if (inum == ip->i_number && dev == ip->i_dev)
			break;
	mtx_unlock(&ext2_ihash_mtx);

	if (ip)
		return (ITOV(ip));
	return (NULLVP);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
int
ext2_ihashget(dev, inum, flags, vpp)
	dev_t dev;
	ino_t inum;
	int flags;
	struct vnode **vpp;
{
	struct thread *td = curthread;	/* XXX */
	struct inode *ip;
	struct vnode *vp;
	int error;

	*vpp = NULL;
loop:
	mtx_lock(&ext2_ihash_mtx);
	LIST_FOREACH(ip, INOHASH(dev, inum), i_hash) {
		if (inum == ip->i_number && dev == ip->i_dev) {
			vp = ITOV(ip);
			mtx_lock(&vp->v_interlock);
			mtx_unlock(&ext2_ihash_mtx);
			error = vget(vp, flags | LK_INTERLOCK, td);
			if (error == ENOENT)
				goto loop;
			if (error)
				return (error);
			*vpp = vp;
			return (0);
		}
	}
	mtx_unlock(&ext2_ihash_mtx);
	return (0);
}

/*
 * Insert the inode into the hash table, and return it locked.
 */
void
ext2_ihashins(ip)
	struct inode *ip;
{
	struct thread *td = curthread;		/* XXX */
	struct ihashhead *ipp;

	/* lock the inode, then put it on the appropriate hash list */
	vn_lock(ITOV(ip), LK_EXCLUSIVE | LK_RETRY, td);

	mtx_lock(&ext2_ihash_mtx);
	ipp = INOHASH(ip->i_dev, ip->i_number);
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	ip->i_flag |= IN_HASHED;
	mtx_unlock(&ext2_ihash_mtx);
}

/*
 * Remove the inode from the hash table.
 */
void
ext2_ihashrem(ip)
	struct inode *ip;
{
	mtx_lock(&ext2_ihash_mtx);
	if (ip->i_flag & IN_HASHED) {
		ip->i_flag &= ~IN_HASHED;
		LIST_REMOVE(ip, i_hash);
	}
	mtx_unlock(&ext2_ihash_mtx);
}
