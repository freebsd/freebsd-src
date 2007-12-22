/*	$NetBSD: ntfs_ihash.c,v 1.5 1999/09/30 16:56:40 jdolecek Exp $	*/

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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>

#include <fs/ntfs/ntfs.h>
#include <fs/ntfs/ntfs_inode.h>
#include <fs/ntfs/ntfs_ihash.h>

MALLOC_DEFINE(M_NTFSNTHASH, "ntfs_nthash", "NTFS ntnode hash tables");

/*
 * Structures associated with inode cacheing.
 */
static LIST_HEAD(nthashhead, ntnode) *ntfs_nthashtbl;
static u_long	ntfs_nthash;		/* size of hash table - 1 */
#define	NTNOHASH(device, inum)	(&ntfs_nthashtbl[(minor(device) + (inum)) & ntfs_nthash])
static struct mtx ntfs_nthash_mtx;
struct lock ntfs_hashlock;

/*
 * Initialize inode hash table.
 */
void
ntfs_nthashinit()
{
	lockinit(&ntfs_hashlock, PINOD, "ntfs_nthashlock", 0, 0);
	ntfs_nthashtbl = hashinit(desiredvnodes, M_NTFSNTHASH, &ntfs_nthash);
	mtx_init(&ntfs_nthash_mtx, "ntfs nthash", NULL, MTX_DEF);
}

/*
 * Destroy inode hash table.
 */
void
ntfs_nthashdestroy(void)
{
	hashdestroy(ntfs_nthashtbl, M_NTFSNTHASH, ntfs_nthash);
	lockdestroy(&ntfs_hashlock);
	mtx_destroy(&ntfs_nthash_mtx);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct ntnode *
ntfs_nthashlookup(dev, inum)
	struct cdev *dev;
	ino_t inum;
{
	struct ntnode *ip;

	mtx_lock(&ntfs_nthash_mtx);
	LIST_FOREACH(ip, NTNOHASH(dev, inum), i_hash)
		if (inum == ip->i_number && dev == ip->i_dev)
			break;
	mtx_unlock(&ntfs_nthash_mtx);

	return (ip);
}

/*
 * Insert the ntnode into the hash table.
 */
void
ntfs_nthashins(ip)
	struct ntnode *ip;
{
	struct nthashhead *ipp;

	mtx_lock(&ntfs_nthash_mtx);
	ipp = NTNOHASH(ip->i_dev, ip->i_number);
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	ip->i_flag |= IN_HASHED;
	mtx_unlock(&ntfs_nthash_mtx);
}

/*
 * Remove the inode from the hash table.
 */
void
ntfs_nthashrem(ip)
	struct ntnode *ip;
{
	mtx_lock(&ntfs_nthash_mtx);
	if (ip->i_flag & IN_HASHED) {
		ip->i_flag &= ~IN_HASHED;
		LIST_REMOVE(ip, i_hash);
	}
	mtx_unlock(&ntfs_nthash_mtx);
}
