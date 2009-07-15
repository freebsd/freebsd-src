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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/filedesc.h>
#include <sys/queue.h>
#define	_WANT_FILE
#include <sys/file.h>
#include <sys/conf.h>
#define	_KERNEL
#include <sys/pipe.h>
#include <sys/mount.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <fs/devfs/devfs.h>
#include <fs/devfs/devfs_int.h>
#undef _KERNEL
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>


#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "common.h"

int vflg = 0;

void
dprintf(FILE *file, const char *fmt, ...) {
	va_list ap;

	if (vflg != 0) {
		va_start(ap, fmt);
		vfprintf(file, fmt, ap);
		va_end(ap);
	}
}

int
kvm_read_all(kvm_t *kd, unsigned long addr, void *buf, size_t nbytes)
{
	ssize_t error;

	if (nbytes >= SSIZE_MAX)
		return (0);
	error = kvm_read(kd, addr, buf, nbytes);
	return (error == (ssize_t)(nbytes));
}

char *
kdevtoname(kvm_t *kd, struct cdev *dev)
{
	struct cdev si;

	if (!kvm_read_all(kd, (unsigned long)dev, &si, sizeof(si)))
		return (NULL);
	return (strdup(si.__si_namebuf));
}

int
ufs_filestat(kvm_t *kd, struct vnode *vp, struct vnstat *vn)
{
	struct inode inode;

	if (!kvm_read_all(kd, (unsigned long)VTOI(vp), &inode, sizeof(inode))) {
		warnx("can't read inode at %p", (void *)VTOI(vp));
		return (1);
	}
	/*
	 * The st_dev from stat(2) is a dev_t. These kernel structures
	 * contain cdev pointers. We need to convert to dev_t to make
	 * comparisons
	 */
	vn->vn_fsid = dev2udev(kd, inode.i_dev);
	vn->vn_fileid = (long)inode.i_number;
	vn->vn_mode = (mode_t)inode.i_mode;
	vn->vn_size = (u_long)inode.i_size;
	return (0);
}

int
devfs_filestat(kvm_t *kd, struct vnode *vp, struct vnstat *vn)
{
	struct devfs_dirent devfs_dirent;
	struct mount mount;

	if (!kvm_read_all(kd, (unsigned long)vp->v_data, &devfs_dirent,
	    sizeof(devfs_dirent))) {
		warnx("can't read devfs_dirent at %p",
		    (void *)vp->v_data);
		return (1);
	}
	if (!kvm_read_all(kd, (unsigned long)vp->v_mount, &mount,
	    sizeof(mount))) {
		warnx("can't read mount at %p",
		    (void *)vp->v_mount);
		return (1);
	}
	vn->vn_fsid = (long)mount.mnt_stat.f_fsid.val[0];
	vn->vn_fileid = devfs_dirent.de_inode;
	vn->vn_mode = (devfs_dirent.de_mode & ~S_IFMT) | S_IFCHR;
	vn->vn_size = 0;
	return (0);
}

int
nfs_filestat(kvm_t *kd, struct vnode *vp, struct vnstat *vn)
{
	struct nfsnode nfsnode;
	mode_t mode;

	if (!kvm_read_all(kd, (unsigned long)VTONFS(vp), &nfsnode,
	    sizeof(nfsnode))) {
		warnx("can't read nfsnode at %p",
		    (void *)VTONFS(vp));
		return (1);
	}
	vn->vn_fsid = nfsnode.n_vattr.va_fsid;
	vn->vn_fileid = nfsnode.n_vattr.va_fileid;
	vn->vn_size = nfsnode.n_size;
	mode = (mode_t)nfsnode.n_vattr.va_mode;
	switch (vp->v_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		break;
	};
	vn->vn_mode = mode;
	return (0);
}

/*
 * Read the cdev structure in the kernel in order to work out the
 * associated dev_t
 */
dev_t
dev2udev(kvm_t *kd, struct cdev *dev)
{
	struct cdev_priv priv;

	if (kvm_read_all(kd, (unsigned long)cdev2priv(dev), &priv,
	    sizeof(priv))) {
		return ((dev_t)priv.cdp_inode);
	} else {
		dprintf(stderr, "can't convert cdev *%p to a dev_t\n", dev);
		return -1;
	}
}

#ifdef ZFS
void *
getvnodedata(struct vnode *vp)
{
	return (vp->v_data);
}

struct mount *
getvnodemount(struct vnode *vp)
{
	return (vp->v_mount);
}
#endif
