/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ffs_vnops.c	8.15 (Berkeley) 5/14/95
 * $Id: ffs_vnops.c,v 1.38 1998/01/06 05:23:44 dyson Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

static int	ffs_fsync __P((struct vop_fsync_args *));
static int	ffs_getpages __P((struct vop_getpages_args *));
static int	ffs_read __P((struct vop_read_args *));
static int	ffs_write __P((struct vop_write_args *));

/* Global vfs data structures for ufs. */
vop_t **ffs_vnodeop_p;
static struct vnodeopv_entry_desc ffs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperate },
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ &vop_getpages_desc,		(vop_t *) ffs_getpages },
	{ &vop_read_desc,		(vop_t *) ffs_read },
	{ &vop_reallocblks_desc,	(vop_t *) ffs_reallocblks },
	{ &vop_write_desc,		(vop_t *) ffs_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc ffs_vnodeop_opv_desc =
	{ &ffs_vnodeop_p, ffs_vnodeop_entries };

vop_t **ffs_specop_p;
static struct vnodeopv_entry_desc ffs_specop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperatespec },
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ NULL, NULL }
};
static struct vnodeopv_desc ffs_specop_opv_desc =
	{ &ffs_specop_p, ffs_specop_entries };

vop_t **ffs_fifoop_p;
static struct vnodeopv_entry_desc ffs_fifoop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperatefifo },
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ NULL, NULL }
};
static struct vnodeopv_desc ffs_fifoop_opv_desc =
	{ &ffs_fifoop_p, ffs_fifoop_entries };

VNODEOP_SET(ffs_vnodeop_opv_desc);
VNODEOP_SET(ffs_specop_opv_desc);
VNODEOP_SET(ffs_fifoop_opv_desc);

SYSCTL_NODE(_vfs, MOUNT_UFS, ffs, CTLFLAG_RW, 0, "FFS filesystem");

#include <ufs/ufs/ufs_readwrite.c>

int ffs_log_sync = 0;


/*
 * Synch an open file.
 */
/* ARGSUSED */
static int
ffs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct buf *bp;
	struct timeval tv;
	struct buf *nbp;
	int pass;
	int s;
	daddr_t lbn;

	if (vp->v_type == VBLK) {
		lbn = INT_MAX;
	} else {
		struct inode *ip;
		ip = VTOI(vp);
		lbn = lblkno(ip->i_fs, (ip->i_size + ip->i_fs->fs_bsize - 1));
	}

	if (ffs_log_sync) {
		struct inode* ip = VTOI(vp);
		printf("fsync i %u iflags 0x%x vn 0x%x vtype %d tag %d vflags 0x%x\n",
			ip->i_number, ip->i_flag, vp, vp->v_type, vp->v_tag, vp->v_flag);
	}

	pass = 0;
	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if ((bp->b_flags & B_BUSY) || (pass == 0 && (bp->b_lblkno < 0)))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("ffs_fsync: not dirty");

		if (ffs_log_sync) {
			printf("  blk %u (%u) flags 0x%x vn 0x%x\n", bp->b_lblkno, bp->b_blkno,
			bp->b_flags, bp->b_vp);
		}

		if (((bp->b_vp != vp) || (ap->a_waitfor != MNT_NOWAIT)) ||
			((vp->v_type != VREG) && (vp->v_type != VBLK))) {

			bremfree(bp);
			bp->b_flags |= B_BUSY;
			splx(s);

			/*
			 * Wait for I/O associated with indirect blocks to complete,
			 * since there is no way to quickly wait for them below.
			 */
			if ((bp->b_vp == vp) && (ap->a_waitfor == MNT_NOWAIT)) {
				if (bp->b_flags & B_CLUSTEROK) {
					bdwrite(bp);
					(void) vfs_bio_awrite(bp);
				} else {
					(void) bawrite(bp);
				}
			} else {
				(void) bwrite(bp);
			}

		} else if ((vp->v_type == VREG) && (bp->b_lblkno >= lbn)) {

			bremfree(bp);
			bp->b_flags |= B_BUSY | B_INVAL | B_NOCACHE;
			brelse(bp);
			splx(s);

		} else {
			vfs_bio_awrite(bp);
			splx(s);
		}
		goto loop;
	}
	splx(s);

	if (pass == 0) {
		pass = 1;
		goto loop;
	}

	if (ap->a_waitfor == MNT_WAIT) {
		s = splbio();
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
			(void) tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "ffsfsn", 0);
		}
		splx(s);
#ifdef DIAGNOSTIC
		if (vp->v_dirtyblkhd.lh_first) {
			vprint("ffs_fsync: dirty", vp);
			goto loop;
		}
#endif
	}

	gettime(&tv);
	return (UFS_UPDATE(ap->a_vp, &tv, &tv, 1));
}
