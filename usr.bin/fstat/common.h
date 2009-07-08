/*-
 * Copyright (c) 1988, 1993
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
 * $FreeBSD$
 */

#ifndef	__COMMON_H__
#define	__COMMON_H__

struct  filestat {
	long	fsid;
	long	fileid;
	mode_t	mode;
	u_long	size;
	dev_t	rdev;
};

extern int vflg;

dev_t	dev2udev(kvm_t *kd, struct cdev *dev);
void	dprintf(FILE *file, const char *fmt, ...);
char	*kdevtoname(kvm_t *kd, struct cdev *dev);
int	kvm_read_all(kvm_t *kd, unsigned long addr, void *buf,
    size_t nbytes);

/*
 * Filesystems specific access routines.
 */
int	devfs_filestat(kvm_t *kd, struct vnode *vp, struct filestat *fsp);
int	isofs_filestat(kvm_t *kd, struct vnode *vp, struct filestat *fsp);
int	msdosfs_filestat(kvm_t *kd, struct vnode *vp, struct filestat *fsp);
int	nfs_filestat(kvm_t *kd, struct vnode *vp, struct filestat *fsp);
int	ufs_filestat(kvm_t *kd, struct vnode *vp, struct filestat *fsp);
#ifdef ZFS
int	zfs_filestat(kvm_t *kd, struct vnode *vp, struct filestat *fsp);
void	*getvnodedata(struct vnode *vp);
struct mount	*getvnodemount(struct vnode *vp);
#endif

#endif /* __COMMON_H__ */
