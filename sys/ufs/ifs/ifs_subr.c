/*
 * Copyright (c) 1999, 2000
 *	Adrian Chadd <adrian@FreeBSD.org>
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/dirent.h>

#include <machine/limits.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ifs/ifs_extern.h>


/*
 * Check whether the given inode number is free.
 *
 * This routine is a chunk of ffs_nodealloccg - we aren't
 * allocating here. We also check whether there will be
 * any other inodes in the cylinder group, and if not,
 * we return -1.
 */
int
ifs_isinodealloc(struct inode *ip, ufs_daddr_t ino)
{
        struct fs *fs;
        struct cg *cgp;
        struct buf *bp;
        int error;
        int cg;
        int retval = 0;

        /* Grab the filesystem info and cylinder group */
        fs = ip->i_fs;
        cg = ino_to_cg(fs, ino);
        /* Read in the cylinder group inode allocation bitmap .. */
        error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
                (int)fs->fs_cgsize, NOCRED, &bp);
        if (error) {
  		retval = IFS_INODE_NOALLOC; 
		goto end;
        }
        cgp = (struct cg *)bp->b_data;
        if (!cg_chkmagic(cgp)) {
		retval = IFS_INODE_NOALLOC;
		goto end;
        }
        ino %= fs->fs_ipg;
	/*
	 * Check whether we have any inodes in this cg, or whether the
	 * inode is allocated
	 */
	if (!isclr(cg_inosused(cgp), ino))
                retval = IFS_INODE_ISALLOC;		/* it is allocated */
	else if (cgp->cg_niblk == cgp->cg_cs.cs_nifree)
		retval = IFS_INODE_EMPTYCG;		/* empty cg */
	else
                retval = IFS_INODE_NOALLOC;		/* its not allocated */
end:
        /* Close the buffer and return */
        brelse(bp);
        return (retval);
}

