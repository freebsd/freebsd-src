/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *	from: @(#)isofs_inode.c
 *	$Id: isofs_node.c,v 1.6 1993/11/25 01:32:23 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "mount.h"
#include "proc.h"
#include "file.h"
#include "buf.h"
#include "vnode.h"
#include "kernel.h"
#include "malloc.h"

#include "iso.h"
#include "isofs_node.h"
#include "iso_rrip.h"

#define	INOHSZ	512
#if	((INOHSZ&(INOHSZ-1)) == 0)
#define	INOHASH(dev,ino)	(((dev)+(ino))&(INOHSZ-1))
#else
#define	INOHASH(dev,ino)	(((unsigned)((dev)+(ino)))%INOHSZ)
#endif

union iso_ihead {
	union  iso_ihead *ih_head[2];
	struct iso_node *ih_chain[2];
} iso_ihead[INOHSZ];

int prtactive;	/* 1 => print out reclaim of active vnodes */

/*
 * Initialize hash links for inodes.
 */
void
isofs_init()
{
	register int i;
	register union iso_ihead *ih = iso_ihead;

#ifndef lint
	if (VN_MAXPRIVATE < sizeof(struct iso_node))
		panic("ihinit: too small");
#endif /* not lint */
	for (i = INOHSZ; --i >= 0; ih++) {
		ih->ih_head[0] = ih;
		ih->ih_head[1] = ih;
	}
}

/*
 * Look up a ISOFS dinode number to find its incore vnode.
 * If it is not in core, read it in from the specified device.
 * If it is in core, wait for the lock bit to clear, then
 * return the inode locked. Detection and handling of mount
 * points must be done by the calling routine.
 */
int
iso_iget(xp, ino, ipp, isodir)
	struct iso_node *xp;
	ino_t ino;
	struct iso_node **ipp;
	struct iso_directory_record *isodir;
{
	dev_t dev = xp->i_dev;
	struct mount *mntp = ITOV(xp)->v_mount;
	extern struct vnodeops isofs_vnodeops, spec_inodeops;
	register struct iso_node *ip, *iq;
	register struct vnode *vp;
	struct vnode *nvp;
	struct buf *bp;
	struct dinode *dp;
	union iso_ihead *ih;
	int i, error, result = 0;
	struct iso_mnt *imp;

	ih = &iso_ihead[INOHASH(dev, ino)];
loop:
	for (ip = ih->ih_chain[0];
	     ip != (struct iso_node *)ih;
	     ip = ip->i_forw) {
		if (ino != ip->i_number || dev != ip->i_dev)
			continue;
		if ((ip->i_flag&ILOCKED) != 0) {
			ip->i_flag |= IWANT;
			tsleep((caddr_t)ip, PINOD, "isoiget", 0);
			goto loop;
		}
		if (vget(ITOV(ip)))
			goto loop;
		*ipp = ip;
		return(0);
	}
	/*
	 * Allocate a new inode.
	 */
	if (error = getnewvnode(VT_ISOFS, mntp, &isofs_vnodeops, &nvp)) {
		*ipp = 0;
		return (error);
	}
	ip = VTOI(nvp);
	ip->i_vnode = nvp;
	ip->i_flag = 0;
	ip->i_devvp = 0;
	ip->i_diroff = 0;
	ip->iso_parent = xp->i_diroff; /* Parent directory's */
	ip->iso_parent_ext = xp->iso_extent;
	ip->i_lockf = 0;
	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ip->i_dev = dev;
	ip->i_number = ino;
	insque(ip, ih);
	ISO_ILOCK(ip);

	ip->iso_reclen = isonum_711 (isodir->length);
	ip->iso_extlen = isonum_711 (isodir->ext_attr_length);
	ip->iso_extent = isonum_733 (isodir->extent);
	ip->i_size = isonum_733 (isodir->size);
	ip->iso_flags = isonum_711 (isodir->flags);
	ip->iso_unit_size = isonum_711 (isodir->file_unit_size);
	ip->iso_interleave_gap = isonum_711 (isodir->interleave);
	ip->iso_volume_seq = isonum_723 (isodir->volume_sequence_number);
	ip->iso_namelen = isonum_711 (isodir->name_len);

	imp = VFSTOISOFS (mntp);
	vp = ITOV(ip);
	/*
	 * Setup time stamp, attribute , if CL or PL, set loc but not yet..
	 */
	switch ( imp->iso_ftype ) {
		case ISO_FTYPE_9660:
			isofs_rrip_defattr  ( isodir, &(ip->inode) );
			isofs_rrip_deftstamp( isodir, &(ip->inode) );
			goto FlameOff;
			break;  
		case ISO_FTYPE_RRIP:
			result = isofs_rrip_analyze( isodir, &(ip->inode) );
			break;  
		default:
			printf("unknown iso_ftype.. %d\n", imp->iso_ftype );
			break;
	}
	/*
	 * Initialize the associated vnode
	 */
	if ( result & ISO_SUSP_SLINK ) {
		vp->v_type = VLNK;	      /* Symbolic Link */
	} else {
FlameOff:
		if (ip->iso_flags & 2) {
			vp->v_type = VDIR;
		} else {
			vp->v_type = VREG;
		}
	}

	imp = VFSTOISOFS (mntp);

	if (ino == imp->root_extent)
		vp->v_flag |= VROOT;
	/*
	 * Finish inode initialization.
	 */
	ip->i_mnt = imp;
	ip->i_devvp = imp->im_devvp;
	VREF(ip->i_devvp);
	*ipp = ip;
	return (0);
}

/*
 * Unlock and decrement the reference count of an inode structure.
 */
void
iso_iput(ip)
	register struct iso_node *ip;
{

	if ((ip->i_flag & ILOCKED) == 0)
		panic("iso_iput");
	ISO_IUNLOCK(ip);
	vrele(ITOV(ip));
}

/*
 * Last reference to an inode, write the inode out and if necessary,
 * truncate and deallocate the file.
 */
int
isofs_inactive(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	register struct iso_node *ip = VTOI(vp);
	int mode, error = 0;

	if (prtactive && vp->v_usecount != 0)
		vprint("isofs_inactive: pushing active", vp);

	ip->i_flag = 0;
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */

	/*
	 * Purge symlink entries since they cause problems
	 * when cached.  Leave other entries alone since flushing
	 * them every time is a major performance hit.
	 */
	if (vp->v_usecount == 0 && vp->v_type == VLNK) {
/*		printf("Flushing symlink entry\n");*/
		vgone(vp);
	}
	return (error);
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
isofs_reclaim(vp)
	register struct vnode *vp;
{
	register struct iso_node *ip = VTOI(vp);
	int i;

	if (prtactive && vp->v_usecount != 0)
		vprint("isofs_reclaim: pushing active", vp);
	/*
	 * Remove the inode from its hash chain.
	 */
	remque(ip);
	ip->i_forw = ip;
	ip->i_back = ip;
	/*
	 * Purge old data structures associated with the inode.
	 */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}
	ip->i_flag = 0;
	return (0);
}

/*
 * Lock an inode. If its already locked, set the WANT bit and sleep.
 */
void
iso_ilock(ip)
	register struct iso_node *ip;
{

	while (ip->i_flag & ILOCKED) {
		ip->i_flag |= IWANT;
		if (ip->i_spare0 == curproc->p_pid)
			panic("locking against myself");
		ip->i_spare1 = curproc->p_pid;
		(void) tsleep((caddr_t)ip, PINOD, "isoilck", 0);
	}
	ip->i_spare1 = 0;
	ip->i_spare0 = curproc->p_pid;
	ip->i_flag |= ILOCKED;
}

/*
 * Unlock an inode.  If WANT bit is on, wakeup.
 */
void
iso_iunlock(ip)
	register struct iso_node *ip;
{

	if ((ip->i_flag & ILOCKED) == 0)
		vprint("iso_iunlock: unlocked inode", ITOV(ip));
	ip->i_spare0 = 0;
	ip->i_flag &= ~ILOCKED;
	if (ip->i_flag&IWANT) {
		ip->i_flag &= ~IWANT;
		wakeup((caddr_t)ip);
	}
}
