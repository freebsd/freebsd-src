/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mount.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

static MALLOC_DEFINE(M_UFSIHASH, "UFS ihash", "UFS Inode hash tables");
/*
 * Structures associated with inode cacheing.
 */
static LIST_HEAD(ihashhead, inode) *ihashtbl;
static u_long	ihash;		/* size of hash table - 1 */
#define	INOHASH(device, inum)	(&ihashtbl[(minor(device) + (inum)) & ihash])
static struct mtx ufs_ihash_mtx;

/*
 * Initialize inode hash table.
 */
void
ufs_ihashinit()
{

	ihashtbl = hashinit(desiredvnodes, M_UFSIHASH, &ihash);
	mtx_init(&ufs_ihash_mtx, "ufs ihash", NULL, MTX_DEF);
}

/*
 * Destroy the inode hash table.
 */
void
ufs_ihashuninit()
{

	hashdestroy(ihashtbl, M_UFSIHASH, ihash);
	mtx_destroy(&ufs_ihash_mtx);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct vnode *
ufs_ihashlookup(dev, inum)
	struct cdev *dev;
	ino_t inum;
{
	struct inode *ip;

	mtx_lock(&ufs_ihash_mtx);
	LIST_FOREACH(ip, INOHASH(dev, inum), i_hash)
		if (inum == ip->i_number && dev == ip->i_dev)
			break;
	mtx_unlock(&ufs_ihash_mtx);

	if (ip)
		return (ITOV(ip));
	return (NULLVP);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
int
ufs_ihashget(dev, inum, flags, vpp)
	struct cdev *dev;
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
	mtx_lock(&ufs_ihash_mtx);
	LIST_FOREACH(ip, INOHASH(dev, inum), i_hash) {
		if (inum == ip->i_number && dev == ip->i_dev) {
			vp = ITOV(ip);
			VI_LOCK(vp);
			mtx_unlock(&ufs_ihash_mtx);
			error = vget(vp, flags | LK_INTERLOCK, td);
			if (error == ENOENT)
				goto loop;
			if (error)
				return (error);
			*vpp = vp;
			return (0);
		}
	}
	mtx_unlock(&ufs_ihash_mtx);
	return (0);
}

/*
 * Check hash for duplicate of passed inode, and add if there is no one.
 * if there is a duplicate, vget() it and return to the caller.
 */
int
ufs_ihashins(ip, flags, ovpp)
	struct inode *ip;
	int flags;
	struct vnode **ovpp;
{
	struct thread *td = curthread;		/* XXX */
	struct ihashhead *ipp;
	struct inode *oip;
	struct vnode *ovp;
	int error;

loop:
	mtx_lock(&ufs_ihash_mtx);
	ipp = INOHASH(ip->i_dev, ip->i_number);
	LIST_FOREACH(oip, ipp, i_hash) {
		if (ip->i_number == oip->i_number && ip->i_dev == oip->i_dev) {
			ovp = ITOV(oip);
			VI_LOCK(ovp);
			mtx_unlock(&ufs_ihash_mtx);
			error = vget(ovp, flags | LK_INTERLOCK, td);
			if (error == ENOENT)
				goto loop;
			if (error)
				return (error);
			*ovpp = ovp;
			return (0);
		}
	}
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	ip->i_flag |= IN_HASHED;
	mtx_unlock(&ufs_ihash_mtx);
	*ovpp = NULL;
	return (0);
}

/*
 * Remove the inode from the hash table.
 */
void
ufs_ihashrem(ip)
	struct inode *ip;
{
	mtx_lock(&ufs_ihash_mtx);
	if (ip->i_flag & IN_HASHED) {
		ip->i_flag &= ~IN_HASHED;
		LIST_REMOVE(ip, i_hash);
	}
	mtx_unlock(&ufs_ihash_mtx);
}
