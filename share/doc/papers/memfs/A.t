.\" Copyright (c) 1990 The Regents of the University of California.
.\" All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\" 3. All advertising materials mentioning features or use of this software
.\"    must display the following acknowledgement:
.\"	This product includes software developed by the University of
.\"	California, Berkeley and its contributors.
.\" 4. Neither the name of the University nor the names of its contributors
.\"    may be used to endorse or promote products derived from this software
.\"    without specific prior written permission.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"
.\"	@(#)A.t	5.1 (Berkeley) 4/16/91
.\"
.bp
.nr PS 10
.nr VS 12
.SH
Appendix A - Implementation Details
.LP
.nf
.vS
/*
 * This structure defines the control data for the memory
 * based file system.
 */
struct mfsnode {
	struct	vnode *mfs_vnode;	/* vnode associated with this mfsnode */
	caddr_t	mfs_baseoff;		/* base of file system in memory */
	long	mfs_size;		/* size of memory file system */
	pid_t	mfs_pid;		/* supporting process pid */
	struct	buf *mfs_buflist;	/* list of I/O requests */
};

/*
 * Convert between mfsnode pointers and vnode pointers
 */
#define	VTOMFS(vp)	((struct mfsnode *)(vp)->v_data)
#define	MFSTOV(mfsp)	((mfsp)->mfs_vnode)
#define	MFS_EXIT	(struct buf *)-1

/*
 * Arguments to mount MFS
 */
struct mfs_args {
	char	*name;		/* name to export for statfs */
	caddr_t	base;		/* base address of file system in memory */
	u_long	size;		/* size of file system */
};
.bp
/*
 * Mount an MFS filesystem.
 */
mfs_mount(mp, path, data)
	struct mount *mp;
	char *path;
	caddr_t data;
{
	struct vnode *devvp;
	struct mfsnode *mfsp;
	struct buf *bp;
	struct mfs_args args;

	/*
	 * Create a block device to represent the disk.
	 */
	devvp = getnewvnode(VT_MFS, VBLK, &mfs_vnodeops);
	mfsp = VTOMFS(devvp);
	/*
	 * Save base address of the filesystem from the supporting process.
	 */
	copyin(data, &args, (sizeof mfs_args));
	mfsp->mfs_baseoff = args.base;
	mfsp->mfs_size = args.size;
	/*
	 * Record the process identifier of the supporting process.
	 */
	mfsp->mfs_pid = u.u_procp->p_pid;
	/*
	 * Mount the filesystem.
	 */
	mfsp->mfs_buflist = NULL;
	mountfs(devvp, mp);
	/*
	 * Loop processing I/O requests.
	 */
	while (mfsp->mfs_buflist != MFS_EXIT) {
		while (mfsp->mfs_buflist != NULL) {
			bp = mfsp->mfs_buflist;
			mfsp->mfs_buflist = bp->av_forw;
			offset = mfsp->mfs_baseoff + (bp->b_blkno * DEV_BSIZE);
			if (bp->b_flags & B_READ)
				copyin(offset, bp->b_un.b_addr, bp->b_bcount);
			else /* write_request */
				copyout(bp->b_un.b_addr, offset, bp->b_bcount);
			biodone(bp);
		}
		sleep((caddr_t)devvp, PWAIT);
	}
}
.bp
/*
 * If the MFS process requests the I/O then we must do it directly.
 * Otherwise put the request on the list and request the MFS process
 * to be run.
 */
mfs_strategy(bp)
	struct buf *bp;
{
	struct vnode *devvp;
	struct mfsnode *mfsp;
	off_t offset;

	devvp = bp->b_vp;
	mfsp = VTOMFS(devvp);
	if (mfsp->mfs_pid == u.u_procp->p_pid) {
		offset = mfsp->mfs_baseoff + (bp->b_blkno * DEV_BSIZE);
		if (bp->b_flags & B_READ)
			copyin(offset, bp->b_un.b_addr, bp->b_bcount);
		else /* write_request */
			copyout(bp->b_un.b_addr, offset, bp->b_bcount);
		biodone(bp);
	} else {
		bp->av_forw = mfsp->mfs_buflist;
		mfsp->mfs_buflist = bp;
		wakeup((caddr_t)bp->b_vp);
	}
}

/*
 * The close routine is called by unmount after the filesystem
 * has been successfully unmounted.
 */
mfs_close(devvp)
	struct vnode *devvp;
{
	struct mfsnode *mfsp = VTOMFS(vp);
	struct buf *bp;

	/*
	 * Finish any pending I/O requests.
	 */
	while (bp = mfsp->mfs_buflist) {
		mfsp->mfs_buflist = bp->av_forw;
		mfs_doio(bp, mfsp->mfs_baseoff);
		wakeup((caddr_t)bp);
	}
	/*
	 * Send a request to the filesystem server to exit.
	 */
	mfsp->mfs_buflist = MFS_EXIT;
	wakeup((caddr_t)vp);
}
.vE
