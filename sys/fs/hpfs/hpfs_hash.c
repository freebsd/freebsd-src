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
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#include <fs/hpfs/hpfs.h>

MALLOC_DEFINE(M_HPFSHASH, "HPFS hash", "HPFS node hash tables");

/*
 * Structures associated with hpfsnode cacheing.
 */
static LIST_HEAD(hphashhead, hpfsnode) *hpfs_hphashtbl;
static u_long	hpfs_hphash;		/* size of hash table - 1 */
#define	HPNOHASH(dev, lsn)	(&hpfs_hphashtbl[(minor(dev) + (lsn)) & hpfs_hphash])
static struct mtx hpfs_hphash_mtx;
struct lock hpfs_hphash_lock;

/*
 * Initialize inode hash table.
 */
void
hpfs_hphashinit()
{

	lockinit (&hpfs_hphash_lock, PINOD, "hpfs_hphashlock", 0, 0);
	hpfs_hphashtbl = hashinit(desiredvnodes, M_HPFSHASH, &hpfs_hphash);
	mtx_init(&hpfs_hphash_mtx, "hpfs hphash", MTX_DEF);
}

/*
 * Destroy inode hash table.
 */
void
hpfs_hphashdestroy(void)
{

	lockdestroy(&hpfs_hphash_lock);
	mtx_destroy(&hpfs_hphash_mtx);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct hpfsnode *
hpfs_hphashlookup(dev, ino)
	dev_t dev;
	lsn_t ino;
{
	struct hpfsnode *hp;

	mtx_lock(&hpfs_hphash_mtx);
	LIST_FOREACH(hp, HPNOHASH(dev, ino), h_hash)
		if (ino == hp->h_no && dev == hp->h_dev)
			break;
	mtx_unlock(&hpfs_hphash_mtx);

	return (hp);
}

#if 0
struct hpfsnode *
hpfs_hphashget(dev, ino)
	dev_t dev;
	lsn_t ino;
{
	struct hpfsnode *hp;

loop:
	mtx_lock(&hpfs_hphash_mtx);
	LIST_FOREACH(hp, HPNOHASH(dev, ino), h_hash) {
		if (ino == hp->h_no && dev == hp->h_dev) {
			lockmgr(&hp->h_intlock, LK_EXCLUSIVE | LK_INTERLOCK,
			    &hpfs_hphash_slock, NULL);
			return (hp);
		}
	}
	mtx_unlock(&hpfs_hphash_mtx);
	return (hp);
}
#endif

struct vnode *
hpfs_hphashvget(dev, ino, td)
	dev_t dev;
	lsn_t ino;
	struct thread *td;
{
	struct hpfsnode *hp;
	struct vnode *vp;

loop:
	mtx_lock(&hpfs_hphash_mtx);
	LIST_FOREACH(hp, HPNOHASH(dev, ino), h_hash) {
		if (ino == hp->h_no && dev == hp->h_dev) {
			vp = HPTOV(hp);
			mtx_lock(&vp->v_interlock);
			mtx_unlock(&hpfs_hphash_mtx);
			if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td))
				goto loop;
			return (vp);
		}
	}
	mtx_unlock(&hpfs_hphash_mtx);
	return (NULLVP);
}

/*
 * Insert the hpfsnode into the hash table.
 */
void
hpfs_hphashins(hp)
	struct hpfsnode *hp;
{
	struct hphashhead *hpp;

	mtx_lock(&hpfs_hphash_mtx);
	hpp = HPNOHASH(hp->h_dev, hp->h_no);
	hp->h_flag |= H_HASHED;
	LIST_INSERT_HEAD(hpp, hp, h_hash);
	mtx_unlock(&hpfs_hphash_mtx);
}

/*
 * Remove the inode from the hash table.
 */
void
hpfs_hphashrem(hp)
	struct hpfsnode *hp;
{
	mtx_lock(&hpfs_hphash_mtx);
	if (hp->h_flag & H_HASHED) {
		hp->h_flag &= ~H_HASHED;
		LIST_REMOVE(hp, h_hash);
	}
	mtx_unlock(&hpfs_hphash_mtx);
}
