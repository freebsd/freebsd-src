/*
 * Copyright (c) 1995 Scott Bartram
 * Copyright (c) 1995 Steven Wallace
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>

#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_stat.h>
#include <i386/ibcs2/ibcs2_statfs.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_utsname.h>


static void bsd_stat2ibcs_stat(struct stat *, struct ibcs2_stat *);
static int  cvt_statfs(struct statfs *, caddr_t, int);

static void
bsd_stat2ibcs_stat(st, st4)
	struct stat *st;
	struct ibcs2_stat *st4;
{
	bzero(st4, sizeof(*st4));
	st4->st_dev  = (ibcs2_dev_t)st->st_dev;
	st4->st_ino  = (ibcs2_ino_t)st->st_ino;
	st4->st_mode = (ibcs2_mode_t)st->st_mode;
	st4->st_nlink= (ibcs2_nlink_t)st->st_nlink;
	st4->st_uid  = (ibcs2_uid_t)st->st_uid;
	st4->st_gid  = (ibcs2_gid_t)st->st_gid;
	st4->st_rdev = (ibcs2_dev_t)st->st_rdev;
	if (st->st_size < (quad_t)1 << 32)
		st4->st_size = (ibcs2_off_t)st->st_size;
	else
		st4->st_size = -2;
	st4->st_atim = (ibcs2_time_t)st->st_atime;
	st4->st_mtim = (ibcs2_time_t)st->st_mtime;
	st4->st_ctim = (ibcs2_time_t)st->st_ctime;
}

static int
cvt_statfs(sp, buf, len)
	struct statfs *sp;
	caddr_t buf;
	int len;
{
	struct ibcs2_statfs ssfs;

	bzero(&ssfs, sizeof ssfs);
	ssfs.f_fstyp = 0;
	ssfs.f_bsize = sp->f_bsize;
	ssfs.f_frsize = 0;
	ssfs.f_blocks = sp->f_blocks;
	ssfs.f_bfree = sp->f_bfree;
	ssfs.f_files = sp->f_files;
	ssfs.f_ffree = sp->f_ffree;
	ssfs.f_fname[0] = 0;
	ssfs.f_fpack[0] = 0;
	return copyout((caddr_t)&ssfs, buf, len);
}	

int
ibcs2_statfs(td, uap)
	struct thread *td;
	struct ibcs2_statfs_args *uap;
{
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;
	caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, SCARG(uap, path));
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if ((error = VFS_STATFS(mp, sp, td)) != 0)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return cvt_statfs(sp, (caddr_t)SCARG(uap, buf), SCARG(uap, len));
}

int
ibcs2_fstatfs(td, uap)
	struct thread *td;
	struct ibcs2_fstatfs_args *uap;
{
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	int error;

	if ((error = getvnode(td->td_proc->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	error = VFS_STATFS(mp, sp, td);
	fdrop(fp, td);
	if (error != 0)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return cvt_statfs(sp, (caddr_t)SCARG(uap, buf), SCARG(uap, len));
}

int
ibcs2_stat(td, uap)
	struct thread *td;
	struct ibcs2_stat_args *uap;
{
	struct stat st;
	struct ibcs2_stat ibcs2_st;
	struct stat_args cup;
	int error;
	caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, SCARG(uap, path));
	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(st));

	if ((error = stat(td, &cup)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof(st))) != 0)
		return error;
	bsd_stat2ibcs_stat(&st, &ibcs2_st);
	return copyout((caddr_t)&ibcs2_st, (caddr_t)SCARG(uap, st),
		       ibcs2_stat_len);
}

int
ibcs2_lstat(td, uap)
	struct thread *td;
	struct ibcs2_lstat_args *uap;
{
	struct stat st;
	struct ibcs2_stat ibcs2_st;
	struct lstat_args cup;
	int error;
	caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, SCARG(uap, path));
	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(st));

	if ((error = lstat(td, &cup)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof(st))) != 0)
		return error;
	bsd_stat2ibcs_stat(&st, &ibcs2_st);
	return copyout((caddr_t)&ibcs2_st, (caddr_t)SCARG(uap, st),
		       ibcs2_stat_len);
}

int
ibcs2_fstat(td, uap)
	struct thread *td;
	struct ibcs2_fstat_args *uap;
{
	struct stat st;
	struct ibcs2_stat ibcs2_st;
	struct fstat_args cup;
	int error;
	caddr_t sg = stackgap_init();

	SCARG(&cup, fd) = SCARG(uap, fd);
	SCARG(&cup, sb) = stackgap_alloc(&sg, sizeof(st));

	if ((error = fstat(td, &cup)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, sb), &st, sizeof(st))) != 0)
		return error;
	bsd_stat2ibcs_stat(&st, &ibcs2_st);
	return copyout((caddr_t)&ibcs2_st, (caddr_t)SCARG(uap, st),
		       ibcs2_stat_len);
}

int
ibcs2_utssys(td, uap)
	struct thread *td;
	struct ibcs2_utssys_args *uap;
{
	switch (SCARG(uap, flag)) {
	case 0:			/* uname(2) */
	{
		char machine_name[9], *p;
		struct ibcs2_utsname sut;
		bzero(&sut, ibcs2_utsname_len);

		strncpy(sut.sysname,
			IBCS2_UNAME_SYSNAME, sizeof(sut.sysname) - 1);
		strncpy(sut.release,
			IBCS2_UNAME_RELEASE, sizeof(sut.release) - 1);
		strncpy(sut.version,
			IBCS2_UNAME_VERSION, sizeof(sut.version) - 1);
		getcredhostname(td->td_ucred, machine_name,
		    sizeof(machine_name) - 1);
		p = index(machine_name, '.');
		if ( p )
			*p = '\0';
		strncpy(sut.nodename, machine_name, sizeof(sut.nodename) - 1);
		strncpy(sut.machine, machine, sizeof(sut.machine) - 1);

		DPRINTF(("IBCS2 uname: sys=%s rel=%s ver=%s node=%s mach=%s\n",
			 sut.sysname, sut.release, sut.version, sut.nodename,
			 sut.machine));
		return copyout((caddr_t)&sut, (caddr_t)SCARG(uap, a1),
			       ibcs2_utsname_len);
	}

	case 2:			/* ustat(2) */
	{
		return ENOSYS;	/* XXX - TODO */
	}

	default:
		return ENOSYS;
	}
}
