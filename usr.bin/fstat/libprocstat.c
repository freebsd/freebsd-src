/*-
 * Copyright (c) 2009 Stanislav Sedov <stas@FreeBSD.org>
 * Copyright (c) 1988, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
#include <sys/mount.h>
#include <sys/pipe.h>
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

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <libutil.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "common_kvm.h"
#include "libprocstat.h"

/*
 * Vnode-to-filestat types translation table.
 */
static struct {
        int     vtype; 
        int     fst_vtype;
} vt2fst[] = {
        { VNON, PS_FST_VTYPE_VNON },
        { VREG, PS_FST_VTYPE_VREG },
        { VDIR, PS_FST_VTYPE_VDIR },
        { VBLK, PS_FST_VTYPE_VBLK },
        { VCHR, PS_FST_VTYPE_VCHR },
        { VLNK, PS_FST_VTYPE_VLNK },
        { VSOCK, PS_FST_VTYPE_VSOCK },
        { VFIFO, PS_FST_VTYPE_VFIFO },
        { VBAD, PS_FST_VTYPE_VBAD }
};
#define NVFTYPES (sizeof(vt2fst) / sizeof(*vt2fst))

/*
 * Descriptor-to-filestat flags translation table.
 */
static struct {
	int flag;
	int fst_flag;
} fstflags[] = {
	{ FREAD, PS_FST_FFLAG_READ },
	{ FWRITE, PS_FST_FFLAG_WRITE },
	{ O_NONBLOCK, PS_FST_FFLAG_NONBLOCK },
	{ O_APPEND, PS_FST_FFLAG_APPEND },
	{ O_SHLOCK, PS_FST_FFLAG_SHLOCK },
	{ O_EXLOCK, PS_FST_FFLAG_EXLOCK },
	{ O_ASYNC, PS_FST_FFLAG_ASYNC },
	{ O_SYNC, PS_FST_FFLAG_SYNC },
	{ O_NOFOLLOW, PS_FST_FFLAG_NOFOLLOW },
	{ O_CREAT, PS_FST_FFLAG_CREAT },
	{ O_TRUNC, PS_FST_FFLAG_TRUNC },
	{ O_EXCL, PS_FST_FFLAG_EXCL }
};
#define NFSTFLAGS	(sizeof(fstflags) / sizeof(*fstflags))

/*
 * Filesystem specific handlers.
 */
#define FSTYPE(fst)     {#fst, fst##_filestat}
struct {
        const char      *tag;
        int             (*handler)(kvm_t *kd, struct vnode *vp,
            struct vnstat *vn);
} fstypes[] = {
        FSTYPE(ufs),
        FSTYPE(devfs),
        FSTYPE(nfs),
        FSTYPE(msdosfs),
        FSTYPE(isofs),
#ifdef ZFS
        FSTYPE(zfs),
#endif
/*
        FSTYPE(ntfs),
        FSTYPE(nwfs), 
        FSTYPE(smbfs),
        FSTYPE(udf), 
*/
};
#define NTYPES  (sizeof(fstypes) / sizeof(*fstypes))

#define	PROCSTAT_KVM	1
#define	PROCSTAT_SYSCTL	2

static char	*getmnton(kvm_t *kd, struct mount *m);
static struct filestat_list	*procstat_getfiles_kvm(kvm_t *kd,
    struct kinfo_proc *kp);
static struct filestat_list	*procstat_getfiles_sysctl(
    struct kinfo_proc *kp);
static int	procstat_get_pipe_info_sysctl(struct filestat *fst,
    struct pipestat *pipe, char *errbuf);
static int	procstat_get_pipe_info_kvm(kvm_t *kd, struct filestat *fst,
    struct pipestat *pipe, char *errbuf);
static int	procstat_get_pts_info_sysctl(struct filestat *fst,
    struct ptsstat *pts, char *errbuf);
static int	procstat_get_pts_info_kvm(kvm_t *kd, struct filestat *fst,
    struct ptsstat *pts, char *errbuf);
static int	procstat_get_socket_info_sysctl(struct filestat *fst,
    struct sockstat *sock, char *errbuf);
static int	procstat_get_socket_info_kvm(kvm_t *kd, struct filestat *fst,
    struct sockstat *sock, char *errbuf);
static int	to_filestat_flags(int flags);
static int	procstat_get_vnode_info_kvm(kvm_t *kd, struct filestat *fst,
    struct vnstat *vn, char *errbuf);
static int	procstat_get_vnode_info_sysctl(struct filestat *fst,
    struct vnstat *vn, char *errbuf);
static int	vntype2psfsttype(int type);

void
procstat_close(struct procstat *procstat)
{

	assert(procstat);
	if (procstat->type == PROCSTAT_KVM)
		kvm_close(procstat->kd);
}

struct procstat *
procstat_open(const char *nlistf, const char *memf)
{
	kvm_t *kd;
	char buf[_POSIX2_LINE_MAX];
	struct procstat *procstat;

	procstat = calloc(1, sizeof(*procstat));
	if (procstat == NULL) {
		warn("malloc()");
		return (NULL);
	}
	if (memf != NULL) {
		kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf);
		if (kd == NULL) {
			warnx("kvm_openfiles(): %s", buf);
			free(procstat);
			return (NULL);
		}
		procstat->type = PROCSTAT_KVM;
		procstat->kd = kd;
	} else {
		procstat->type = PROCSTAT_SYSCTL;
	}
	return (procstat);
}

struct kinfo_proc *
procstat_getprocs(struct procstat *procstat, int what, int arg,
    unsigned int *count)
{
	struct kinfo_proc *p0, *p;
	size_t len;
	int name[4];
	int error;

	assert(procstat);
	assert(count);
	p = NULL;
	if (procstat->type == PROCSTAT_KVM) {
		p0 = kvm_getprocs(procstat->kd, what, arg, count);
		if (p0 == NULL || count == 0)
			return (NULL);
		len = *count * sizeof(*p);
		p = malloc(len);
		if (p == NULL) {
			warnx("malloc(%zd)", len);
			goto fail;
		}
		bcopy(p0, p, len);
		return (p);
	} else if (procstat->type == PROCSTAT_SYSCTL) {
		len = 0;
		name[0] = CTL_KERN;
		name[1] = KERN_PROC;
		name[2] = what;
		name[3] = arg;
		error = sysctl(name, 4, NULL, &len, NULL, 0);
		if (error < 0) {
			warn("sysctl(kern.proc)");
			goto fail;
		}
		if (len == 0) {
			warnx("no processes?");
			goto fail;
		}
		p = malloc(len);
		if (p == NULL) {
			warnx("malloc(%zd)", len);
			goto fail;
		}
		error = sysctl(name, 4, p, &len, NULL, 0);
		if (error < 0) {
			warn("sysctl(kern.proc)");
			goto fail;
		}
		/* Perform simple consistency checks. */
		if ((len % sizeof(*p)) != 0 || p->ki_structsize != sizeof(*p)) {
			warnx("kinfo_proc structure size mismatch");
			goto fail;
		}
		*count = len / sizeof(*p);
		return (p);
	} else {
		warnx("unknown access method");
		return (NULL);
	}
fail:
	if (p)
		free(p);
	return (NULL);
}

struct filestat_list *
procstat_getfiles(struct procstat *procstat, struct kinfo_proc *kp)
{
	
	if (procstat->type == PROCSTAT_SYSCTL)
		return (procstat_getfiles_sysctl(kp));
	else if (procstat->type == PROCSTAT_KVM)
		 return (procstat_getfiles_kvm(procstat->kd, kp));
	else
		return (NULL);
}

static struct filestat *
filestat_new_entry(struct vnode *vp, int type, int fd, int fflags)
{
	struct filestat *entry;

	entry = calloc(1, sizeof(*entry));
	if (entry == NULL) {
		warn("malloc()");
		return (NULL);
	}
	entry->fs_typedep = vp;
	entry->fs_fflags = fflags;
	entry->fs_fd = fd;
	entry->fs_type = type;
	return (entry);
}

static struct filestat_list *
procstat_getfiles_kvm(kvm_t *kd, struct kinfo_proc *kp)
{
	int i;
	struct file file;
	struct filedesc filed;
	unsigned int nfiles;
	struct file **ofiles;
	struct filestat *entry;
	struct filestat_list *head;
	int type;
	void *data;

	assert(kd);

	if (kp->ki_fd == NULL)
		return (NULL);
	if (!kvm_read_all(kd, (unsigned long)kp->ki_fd, &filed,
	    sizeof(filed))) {
		warnx("can't read filedesc at %p\n", (void *)kp->ki_fd);
		return (NULL);
	}

	/*
	 * Allocate list head.
	 */
	head = malloc(sizeof(*head));
	if (head == NULL)
		return (NULL);
	STAILQ_INIT(head);

	/* root directory vnode, if one. */
	if (filed.fd_rdir) {
		entry = filestat_new_entry(filed.fd_rdir, PS_FST_TYPE_VNODE, PS_FST_FD_RDIR,
		    PS_FST_FFLAG_READ);
		if (entry != NULL)
			STAILQ_INSERT_TAIL(head, entry, next);
	}
	/* current working directory vnode. */
	if (filed.fd_cdir) {
		entry = filestat_new_entry(filed.fd_cdir, PS_FST_TYPE_VNODE, PS_FST_FD_CDIR,
		    PS_FST_FFLAG_READ);
		if (entry != NULL)
			STAILQ_INSERT_TAIL(head, entry, next);
	}
	/* jail root, if any. */
	if (filed.fd_jdir) {
		entry = filestat_new_entry(filed.fd_jdir, PS_FST_TYPE_VNODE, PS_FST_FD_JAIL,
		    PS_FST_FFLAG_READ);
		if (entry != NULL)
			STAILQ_INSERT_TAIL(head, entry, next);
	}
	/* ktrace vnode, if one */
	if (kp->ki_tracep) {
		entry = filestat_new_entry(kp->ki_tracep, PS_FST_TYPE_VNODE, PS_FST_FD_TRACE,
		    PS_FST_FFLAG_READ | PS_FST_FFLAG_WRITE);
		if (entry != NULL)
			STAILQ_INSERT_TAIL(head, entry, next);
	}
	/* text vnode, if one */
	if (kp->ki_textvp) {
		entry = filestat_new_entry(kp->ki_textvp, PS_FST_TYPE_VNODE, PS_FST_FD_TEXT,
		    PS_FST_FFLAG_READ);
		if (entry != NULL)
			STAILQ_INSERT_TAIL(head, entry, next);
	}

	nfiles = filed.fd_lastfile + 1;
	ofiles = malloc(nfiles * sizeof(struct file *));
	if (ofiles == NULL) {
		warn("malloc(%zd)", nfiles * sizeof(struct file *));
		goto exit;
	}
	if (!kvm_read_all(kd, (unsigned long)filed.fd_ofiles, ofiles,
	    nfiles * sizeof(struct file *))) {
		warn("cannot read file structures at %p\n",
		    (void *)filed.fd_ofiles);
		free(ofiles);
		goto exit;
	}
	for (i = 0; i <= filed.fd_lastfile; i++) {
		if (ofiles[i] == NULL)
			continue;
		if (!kvm_read_all(kd, (unsigned long)ofiles[i], &file,
		    sizeof(struct file))) {
			warn("can't read file %d at %p\n", i,
			    (void *)ofiles[i]);
			continue;
		}
		switch (file.f_type) {
		case DTYPE_VNODE:
			type = PS_FST_TYPE_VNODE;
			data = file.f_vnode;
			break;
		case DTYPE_SOCKET:
			type = PS_FST_TYPE_SOCKET;
			data = file.f_data;
			break;
		case DTYPE_PIPE:
			type = PS_FST_TYPE_PIPE;
			data = file.f_data;
			break;
		case DTYPE_FIFO:
			type = PS_FST_TYPE_FIFO;
			data = file.f_vnode;
			break;
#ifdef DTYPE_PTS
		case DTYPE_PTS:
			type = PS_FST_TYPE_PTS;
			data = file.f_data;
			break;
#endif
		default:
			warnx("unknown file type %d for file %d\n",
			    file.f_type, i);
			continue;
		}
		entry = filestat_new_entry(data, type, i,
		    to_filestat_flags(file.f_flag));
		if (entry != NULL)
			STAILQ_INSERT_TAIL(head, entry, next);
	}
	free(ofiles);
exit:
	return (head);
}

static struct filestat_list *
procstat_getfiles_sysctl(struct kinfo_proc *kp __unused)
{
	return (NULL);
}

int
procstat_get_pipe_info(struct procstat *procstat, struct filestat *fst,
    struct pipestat *pipe, char *errbuf)
{

	assert(pipe);
	if (procstat->type == PROCSTAT_KVM) {
		return (procstat_get_pipe_info_kvm(procstat->kd, fst, pipe,
		    errbuf));
	} else if (procstat->type == PROCSTAT_SYSCTL) {
		return (procstat_get_pipe_info_sysctl(fst, pipe, errbuf));
	} else {
		warnx("unknow access method: %d", procstat->type);
		snprintf(errbuf, _POSIX2_LINE_MAX, "error");
		return (1);
	}
}

static int
procstat_get_pipe_info_kvm(kvm_t *kd, struct filestat *fst,
    struct pipestat *pipe, char *errbuf)
{
	struct pipe pi;
	void *pipep;

	assert(kd);
	assert(pipe);
	assert(fst);
	bzero(pipe, sizeof(*pipe));
	pipep = fst->fs_typedep;
	if (pipep == NULL)
		goto fail;
	if (!kvm_read_all(kd, (unsigned long)pipep, &pi, sizeof(struct pipe))) {
		warnx("can't read pipe at %p", (void *)pipep);
		goto fail;
	}
	pipe->addr = (caddr_t)pipep;
	pipe->peer = (caddr_t)pi.pipe_peer;
	pipe->buffer_cnt = pi.pipe_buffer.cnt;
	return (0);

fail:
	snprintf(errbuf, _POSIX2_LINE_MAX, "error");
	return (1);
}

static int
procstat_get_pipe_info_sysctl(struct filestat *fst, struct pipestat *pipe,
    char *errbuf)
{

	warnx("not implemented: %s:%d", __FUNCTION__, __LINE__);
	snprintf(errbuf, _POSIX2_LINE_MAX, "error");
	return (1);
}

int
procstat_get_pts_info(struct procstat *procstat, struct filestat *fst,
    struct ptsstat *pts, char *errbuf)
{

	assert(pts);
	if (procstat->type == PROCSTAT_KVM) {
		return (procstat_get_pts_info_kvm(procstat->kd, fst, pts,
		    errbuf));
	} else if (procstat->type == PROCSTAT_SYSCTL) {
		return (procstat_get_pts_info_sysctl(fst, pts, errbuf));
	} else {
		warnx("unknow access method: %d", procstat->type);
		snprintf(errbuf, _POSIX2_LINE_MAX, "error");
		return (1);
	}
}

static int
procstat_get_pts_info_kvm(kvm_t *kd, struct filestat *fst,
    struct ptsstat *pts, char *errbuf)
{
	struct tty tty;
	void *ttyp;

	assert(kd);
	assert(pts);
	assert(fst);
	bzero(pts, sizeof(*pts));
	ttyp = fst->fs_typedep;
	if (ttyp == NULL)
		goto fail;
	if (!kvm_read_all(kd, (unsigned long)ttyp, &tty, sizeof(struct tty))) {
		warnx("can't read tty at %p", (void *)ttyp);
		goto fail;
	}
	pts->dev = dev2udev(kd, tty.t_dev);
	(void)kdevtoname(kd, tty.t_dev, pts->devname);
	return (0);

fail:
	snprintf(errbuf, _POSIX2_LINE_MAX, "error");
	return (1);
}

static int
procstat_get_pts_info_sysctl(struct filestat *fst, struct ptsstat *pts,
    char *errbuf)
{

	warnx("not implemented: %s:%d", __FUNCTION__, __LINE__);
	snprintf(errbuf, _POSIX2_LINE_MAX, "error");
	return (1);
}

int
procstat_get_vnode_info(struct procstat *procstat, struct filestat *fst,
    struct vnstat *vn, char *errbuf)
{

	assert(vn);
	if (procstat->type == PROCSTAT_KVM) {
		return (procstat_get_vnode_info_kvm(procstat->kd, fst, vn,
		    errbuf));
	} else if (procstat->type == PROCSTAT_SYSCTL) {
		return (procstat_get_vnode_info_sysctl(fst, vn, errbuf));
	} else {
		warnx("unknow access method: %d", procstat->type);
		snprintf(errbuf, _POSIX2_LINE_MAX, "error");
		return (1);
	}
}

static int
procstat_get_vnode_info_kvm(kvm_t *kd, struct filestat *fst,
    struct vnstat *vn, char *errbuf)
{
	char tagstr[12];
	int error;
	int found;
	struct vnode vnode;
	void *vp;
	unsigned int i;

	assert(kd);
	assert(vn);
	assert(fst);
	vp = fst->fs_typedep;
	if (vp == NULL)
		goto fail;
	error = kvm_read_all(kd, (unsigned long)vp, &vnode, sizeof(vnode));
	if (error == 0) {
		warnx("can't read vnode at %p\n", (void *)vp);
		goto fail;
	}
	bzero(vn, sizeof(*vn));
	vn->vn_type = vntype2psfsttype(vnode.v_type);
	if (vnode.v_type == VNON || vnode.v_type == VBAD)
		return (0);
	error = kvm_read_all(kd, (unsigned long)vnode.v_tag, tagstr, sizeof(tagstr));
	if (error == 0) {
		warnx("can't read v_tag at %p\n", (void *)vp);
		goto fail;
	}
	tagstr[sizeof(tagstr) - 1] = '\0';

	/*
	 * Find appropriate handler.
	 */
	for (i = 0, found = 0; i < NTYPES; i++)
		if (!strcmp(fstypes[i].tag, tagstr)) {
			if (fstypes[i].handler(kd, &vnode, vn) != 0) {
				goto fail;
			}
			break;
		}
	if (i == NTYPES) {
		snprintf(errbuf, _POSIX2_LINE_MAX, "?(%s)", tagstr);
		return (1);
	}
	vn->mntdir = getmnton(kd, vnode.v_mount);
	if ((vnode.v_type == VBLK || vnode.v_type == VCHR) &&
	    vnode.v_rdev != NULL){
		vn->vn_dev = dev2udev(kd, vnode.v_rdev);
		(void)kdevtoname(kd, vnode.v_rdev, vn->vn_devname);
	} else {
		vn->vn_dev = -1;
	}
	return (0);

fail:
	snprintf(errbuf, _POSIX2_LINE_MAX, "error");
	return (1);
}

static int
procstat_get_vnode_info_sysctl(struct filestat *fst, struct vnstat *vn,
    char *errbuf)
{

	warnx("not implemented: %s:%d", __FUNCTION__, __LINE__);
	snprintf(errbuf, _POSIX2_LINE_MAX, "error");
	return (1);
}

int
procstat_get_socket_info(struct procstat *procstat, struct filestat *fst,
    struct sockstat *sock, char *errbuf)
{

	assert(sock);
	if (procstat->type == PROCSTAT_KVM) {
		return (procstat_get_socket_info_kvm(procstat->kd, fst, sock,
		    errbuf));
	} else if (procstat->type == PROCSTAT_SYSCTL) {
		return (procstat_get_socket_info_sysctl(fst, sock, errbuf));
	} else {
		warnx("unknow access method: %d", procstat->type);
		snprintf(errbuf, _POSIX2_LINE_MAX, "error");
		return (1);
	}
}

static int
procstat_get_socket_info_kvm(kvm_t *kd, struct filestat *fst,
    struct sockstat *sock, char *errbuf)
{
	struct socket s;
	struct protosw proto;
	struct domain dom;
	struct inpcb inpcb;
	struct unpcb unpcb;
	ssize_t len;
	void *so;

	assert(kd);
	assert(sock);
	assert(fst);
	bzero(sock, sizeof(*sock));
	so = fst->fs_typedep;
	if (so == NULL)
		goto fail;
	sock->so_addr = (caddr_t)so;
	/* fill in socket */
	if (!kvm_read_all(kd, (unsigned long)so, &s,
	    sizeof(struct socket))) {
		warnx("can't read sock at %p", (void *)so);
		goto fail;
	}
	/* fill in protosw entry */
	if (!kvm_read_all(kd, (unsigned long)s.so_proto, &proto,
	    sizeof(struct protosw))) {
		warnx("can't read protosw at %p", (void *)s.so_proto);
		goto fail;
	}
	/* fill in domain */
	if (!kvm_read_all(kd, (unsigned long)proto.pr_domain, &dom,
	    sizeof(struct domain))) {
		warnx("can't read domain at %p",
		    (void *)proto.pr_domain);
		goto fail;
	}
	if ((len = kvm_read(kd, (unsigned long)dom.dom_name, sock->dname,
	    sizeof(sock->dname) - 1)) < 0) {
		warnx("can't read domain name at %p", (void *)dom.dom_name);
		sock->dname[0] = '\0';
	}
	else
		sock->dname[len] = '\0';
	
	/*
	 * Fill in known data.
	 */
	sock->type = s.so_type;
	sock->proto = proto.pr_protocol;
	sock->dom_family = dom.dom_family;
	sock->so_pcb = s.so_pcb;

	/*
	 * Protocol specific data.
	 */
	switch(dom.dom_family) {
	case AF_INET:
	case AF_INET6:
		if (proto.pr_protocol == IPPROTO_TCP ) {
			if (s.so_pcb) {
				if (kvm_read(kd, (u_long)s.so_pcb,
				    (char *)&inpcb, sizeof(struct inpcb))
				    != sizeof(struct inpcb)) {
					warnx("can't read inpcb at %p\n",
					    (void *)s.so_pcb);
				} else
					sock->inp_ppcb =
					    (caddr_t)inpcb.inp_ppcb;
			}
		}
		break;
	case AF_UNIX:
		if (s.so_pcb) {
			if (kvm_read(kd, (u_long)s.so_pcb, (char *)&unpcb,
			    sizeof(struct unpcb)) != sizeof(struct unpcb)){
				warnx("can't read unpcb at %p\n",
				    (void *)s.so_pcb);
			} else if (unpcb.unp_conn) {
				sock->so_rcv_sb_state = s.so_rcv.sb_state;
				sock->so_snd_sb_state = s.so_snd.sb_state;
				sock->unp_conn = (caddr_t)unpcb.unp_conn;
			}
		}
		break;
	default:
		break;
	}
	return (0);

fail:
	snprintf(errbuf, _POSIX2_LINE_MAX, "error");
	return (1);
}

static int
procstat_get_socket_info_sysctl(struct filestat *fst, struct sockstat *sock,
    char *errbuf)
{

	warnx("not implemented: %s:%d", __FUNCTION__, __LINE__);
	snprintf(errbuf, _POSIX2_LINE_MAX, "error");
	return (1);
}

static int
to_filestat_flags(int flags)
{
	int fst_flags;
	unsigned int i;

	fst_flags = 0;
	for (i = 0; i < NFSTFLAGS; i++)
		if (flags & fstflags[i].flag)
			fst_flags |= fstflags[i].fst_flag;
	return (fst_flags);
}

static int
vntype2psfsttype(int type)
{
	unsigned int i, fst_type;

	fst_type = PS_FST_VTYPE_UNKNOWN;
	for (i = 0; i < NVFTYPES; i++) {
		if (type == vt2fst[i].vtype) {
			fst_type = vt2fst[i].fst_vtype;
			break;
		}
	}
	return (fst_type);
}

static char *
getmnton(kvm_t *kd, struct mount *m)
{
	static struct mount mnt;
	static struct mtab {
		struct mtab *next;
		struct mount *m;
		char mntonname[MNAMELEN + 1];
	} *mhead = NULL;
	struct mtab *mt;

	for (mt = mhead; mt != NULL; mt = mt->next)
		if (m == mt->m)
			return (mt->mntonname);
	if (!kvm_read_all(kd, (unsigned long)m, &mnt, sizeof(struct mount))) {
		warnx("can't read mount table at %p", (void *)m);
		return (NULL);
	}
	if ((mt = malloc(sizeof (struct mtab))) == NULL)
		err(1, NULL);
	mt->m = m;
	bcopy(&mnt.mnt_stat.f_mntonname[0], &mt->mntonname[0], MNAMELEN);
	mnt.mnt_stat.f_mntonname[MNAMELEN] = '\0';
	mt->next = mhead;
	mhead = mt;
	return (mt->mntonname);
}
