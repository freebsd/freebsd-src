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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)fstat.c	8.3 (Berkeley) 5/2/95";
#endif
#endif /* not lint */
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
#include <sys/filedesc.h>
#include <sys/queue.h>
#define	_KERNEL
#include <sys/pipe.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <fs/devfs/devfs.h>
#undef _KERNEL
#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "fstat.h"

#define	TEXT	-1
#define	CDIR	-2
#define	RDIR	-3
#define	TRACE	-4
#define	MMAP	-5
#define	JDIR	-6

DEVS *devs;

#ifdef notdef
struct nlist nl[] = {
	{ "" },
};
#endif

int 	fsflg,	/* show files on same filesystem as file(s) argument */
	pflg,	/* show files open by a particular pid */
	uflg;	/* show files open by a particular (effective) user */
int 	checkfile; /* true if restricting to particular files or filesystems */
int	nflg;	/* (numerical) display f.s. and rdev as dev_t */
int	vflg;	/* display errors in locating kernel data objects etc... */
int	mflg;	/* include memory-mapped files */


struct file **ofiles;	/* buffer of pointers to file structures */
int maxfiles;
#define ALLOC_OFILES(d)	\
	if ((d) > maxfiles) { \
		free(ofiles); \
		ofiles = malloc((d) * sizeof(struct file *)); \
		if (ofiles == NULL) { \
			err(1, NULL); \
		} \
		maxfiles = (d); \
	}

char *memf, *nlistf;
kvm_t *kd;

static void fstat_kvm(int, int);
static void fstat_sysctl(int, int);
void dofiles(struct kinfo_proc *kp);
void dommap(struct kinfo_proc *kp);
void vtrans(struct vnode *vp, int i, int flag);
int  ufs_filestat(struct vnode *vp, struct filestat *fsp);
int  nfs_filestat(struct vnode *vp, struct filestat *fsp);
int  devfs_filestat(struct vnode *vp, struct filestat *fsp);
char *getmnton(struct mount *m);
void pipetrans(struct pipe *pi, int i, int flag);
void socktrans(struct socket *sock, int i);
void getinetproto(int number);
int  getfname(const char *filename);
void usage(void);


int
main(int argc, char **argv)
{
	struct passwd *passwd;
	int arg, ch, what;

	arg = 0;
	what = KERN_PROC_ALL;
	nlistf = memf = NULL;
	while ((ch = getopt(argc, argv, "fmnp:u:vN:M:")) != -1)
		switch((char)ch) {
		case 'f':
			fsflg = 1;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'm':
			mflg = 1;
			break;
		case 'n':
			nflg = 1;
			break;
		case 'p':
			if (pflg++)
				usage();
			if (!isdigit(*optarg)) {
				warnx("-p requires a process id");
				usage();
			}
			what = KERN_PROC_PID;
			arg = atoi(optarg);
			break;
		case 'u':
			if (uflg++)
				usage();
			if (!(passwd = getpwnam(optarg)))
				errx(1, "%s: unknown uid", optarg);
			what = KERN_PROC_UID;
			arg = passwd->pw_uid;
			break;
		case 'v':
			vflg = 1;
			break;
		case '?':
		default:
			usage();
		}

	if (*(argv += optind)) {
		for (; *argv; ++argv) {
			if (getfname(*argv))
				checkfile = 1;
		}
		if (!checkfile)	/* file(s) specified, but none accessable */
			exit(1);
	}

	if (fsflg && !checkfile) {
		/* -f with no files means use wd */
		if (getfname(".") == 0)
			exit(1);
		checkfile = 1;
	}

	if (memf != NULL)
		fstat_kvm(what, arg);
	else
		fstat_sysctl(what, arg);
	exit(0);
}

static void
print_header(void)
{

	if (nflg)
		printf("%s",
"USER     CMD          PID   FD  DEV    INUM       MODE SZ|DV R/W");
	else
		printf("%s",
"USER     CMD          PID   FD MOUNT      INUM MODE         SZ|DV R/W");
	if (checkfile && fsflg == 0)
		printf(" NAME\n");
	else
		putchar('\n');
}

static void
fstat_kvm(int what, int arg)
{
	struct kinfo_proc *p, *plast;
	char buf[_POSIX2_LINE_MAX];
	int cnt;

	ALLOC_OFILES(256);	/* reserve space for file pointers */

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL)
		setgid(getgid());

	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf)) == NULL)
		errx(1, "%s", buf);
	setgid(getgid());
#ifdef notdef
	if (kvm_nlist(kd, nl) != 0)
		errx(1, "no namelist: %s", kvm_geterr(kd));
#endif
	if ((p = kvm_getprocs(kd, what, arg, &cnt)) == NULL)
		errx(1, "%s", kvm_geterr(kd));
	print_header();
	for (plast = &p[cnt]; p < plast; ++p) {
		if (p->ki_stat == SZOMB)
			continue;
		dofiles(p);
		if (mflg)
			dommap(p);
	}
}

static void
fstat_sysctl(int what, int arg)
{

	/* not yet implemented */
	fstat_kvm(what, arg);
}

const char	*Uname, *Comm;
int	Pid;

#define PREFIX(i) printf("%-8.8s %-10s %5d", Uname, Comm, Pid); \
	switch(i) { \
	case TEXT: \
		printf(" text"); \
		break; \
	case CDIR: \
		printf("   wd"); \
		break; \
	case RDIR: \
		printf(" root"); \
		break; \
	case TRACE: \
		printf("   tr"); \
		break; \
	case MMAP: \
		printf(" mmap"); \
		break; \
	case JDIR: \
		printf(" jail"); \
		break; \
	default: \
		printf(" %4d", i); \
		break; \
	}

/*
 * print open files attributed to this process
 */
void
dofiles(struct kinfo_proc *kp)
{
	int i;
	struct file file;
	struct filedesc filed;

	Uname = user_from_uid(kp->ki_uid, 0);
	Pid = kp->ki_pid;
	Comm = kp->ki_comm;

	if (kp->ki_fd == NULL)
		return;
	if (!KVM_READ(kp->ki_fd, &filed, sizeof (filed))) {
		dprintf(stderr, "can't read filedesc at %p for pid %d\n",
		    (void *)kp->ki_fd, Pid);
		return;
	}
	/*
	 * root directory vnode, if one
	 */
	if (filed.fd_rdir)
		vtrans(filed.fd_rdir, RDIR, FREAD);
	/*
	 * current working directory vnode
	 */
	vtrans(filed.fd_cdir, CDIR, FREAD);
	/*
	 * jail root, if any.
	 */
	if (filed.fd_jdir)
		vtrans(filed.fd_jdir, JDIR, FREAD);
	/*
	 * ktrace vnode, if one
	 */
	if (kp->ki_tracep)
		vtrans(kp->ki_tracep, TRACE, FREAD|FWRITE);
	/*
	 * text vnode, if one
	 */
	if (kp->ki_textvp)
		vtrans(kp->ki_textvp, TEXT, FREAD);
	/*
	 * open files
	 */
#define FPSIZE	(sizeof (struct file *))
#define MAX_LASTFILE	(0x1000000)

	/* Sanity check on filed.fd_lastfile */
	if (filed.fd_lastfile <= -1 || filed.fd_lastfile > MAX_LASTFILE)
		return;

	ALLOC_OFILES(filed.fd_lastfile+1);
	if (!KVM_READ(filed.fd_ofiles, ofiles,
	    (filed.fd_lastfile+1) * FPSIZE)) {
		dprintf(stderr,
		    "can't read file structures at %p for pid %d\n",
		    (void *)filed.fd_ofiles, Pid);
		return;
	}
	for (i = 0; i <= filed.fd_lastfile; i++) {
		if (ofiles[i] == NULL)
			continue;
		if (!KVM_READ(ofiles[i], &file, sizeof (struct file))) {
			dprintf(stderr, "can't read file %d at %p for pid %d\n",
			    i, (void *)ofiles[i], Pid);
			continue;
		}
		if (file.f_type == DTYPE_VNODE)
			vtrans(file.f_vnode, i, file.f_flag);
		else if (file.f_type == DTYPE_SOCKET) {
			if (checkfile == 0)
				socktrans(file.f_data, i);
		}
#ifdef DTYPE_PIPE
		else if (file.f_type == DTYPE_PIPE) {
			if (checkfile == 0)
				pipetrans(file.f_data, i, file.f_flag);
		}
#endif
#ifdef DTYPE_FIFO
		else if (file.f_type == DTYPE_FIFO) {
			if (checkfile == 0)
				vtrans(file.f_vnode, i, file.f_flag);
		}
#endif
		else {
			dprintf(stderr,
			    "unknown file type %d for file %d of pid %d\n",
			    file.f_type, i, Pid);
		}
	}
}

void
dommap(struct kinfo_proc *kp)
{
	vm_map_t map;
	struct vmspace vmspace;
	struct vm_map_entry entry;
	vm_map_entry_t entryp;
	struct vm_object object;
	vm_object_t objp;
	int prot, fflags;

	if (!KVM_READ(kp->ki_vmspace, &vmspace, sizeof(vmspace))) {
		dprintf(stderr,
		    "can't read vmspace at %p for pid %d\n",
		    (void *)kp->ki_vmspace, Pid);
		return;
	}
	map = &vmspace.vm_map;

	for (entryp = map->header.next;
	    entryp != &kp->ki_vmspace->vm_map.header; entryp = entry.next) {
		if (!KVM_READ(entryp, &entry, sizeof(entry))) {
			dprintf(stderr,
			    "can't read vm_map_entry at %p for pid %d\n",
			    (void *)entryp, Pid);
			return;
		}

		if (entry.eflags & MAP_ENTRY_IS_SUB_MAP)
			continue;

		if ((objp = entry.object.vm_object) == NULL)
			continue;

		for (; objp; objp = object.backing_object) {
			if (!KVM_READ(objp, &object, sizeof(object))) {
				dprintf(stderr,
				    "can't read vm_object at %p for pid %d\n",
				    (void *)objp, Pid);
				return;
			}
		}

		prot = entry.protection;
		fflags = (prot & VM_PROT_READ ? FREAD : 0) |
		    (prot & VM_PROT_WRITE ? FWRITE : 0);

		switch (object.type) {
		case OBJT_VNODE:
			vtrans((struct vnode *)object.handle, MMAP, fflags);
			break;
		default:
			break;
		}
	}
}

void
vtrans(struct vnode *vp, int i, int flag)
{
	struct vnode vn;
	struct filestat fst;
	char rw[3], mode[15], tagstr[12], *tagptr;
	const char *badtype, *filename;

	filename = badtype = NULL;
	if (!KVM_READ(vp, &vn, sizeof (struct vnode))) {
		dprintf(stderr, "can't read vnode at %p for pid %d\n",
		    (void *)vp, Pid);
		return;
	}
	if (!KVM_READ(&vp->v_tag, &tagptr, sizeof tagptr) ||
	    !KVM_READ(tagptr, tagstr, sizeof tagstr)) {
		dprintf(stderr, "can't read v_tag at %p for pid %d\n",
		    (void *)vp, Pid);
		return;
	}
	tagstr[sizeof(tagstr) - 1] = '\0';
	if (vn.v_type == VNON)
		badtype = "none";
	else if (vn.v_type == VBAD)
		badtype = "bad";
	else {
		if (!strcmp("ufs", tagstr)) {
			if (!ufs_filestat(&vn, &fst))
				badtype = "error";
		} else if (!strcmp("devfs", tagstr)) {
			if (!devfs_filestat(&vn, &fst))
				badtype = "error";
		} else if (!strcmp("nfs", tagstr)) {
			if (!nfs_filestat(&vn, &fst))
				badtype = "error";
		} else if (!strcmp("msdosfs", tagstr)) {
			if (!msdosfs_filestat(&vn, &fst))
				badtype = "error";
		} else if (!strcmp("isofs", tagstr)) {
			if (!isofs_filestat(&vn, &fst))
				badtype = "error";
		} else {
			static char unknown[32];
			snprintf(unknown, sizeof unknown, "?(%s)", tagstr);
			badtype = unknown;
		}
	}
	if (checkfile) {
		int fsmatch = 0;
		DEVS *d;

		if (badtype)
			return;
		for (d = devs; d != NULL; d = d->next)
			if (d->fsid == fst.fsid) {
				fsmatch = 1;
				if (d->ino == fst.fileid) {
					filename = d->name;
					break;
				}
			}
		if (fsmatch == 0 || (filename == NULL && fsflg == 0))
			return;
	}
	PREFIX(i);
	if (badtype) {
		(void)printf(" -         -  %10s    -\n", badtype);
		return;
	}
	if (nflg)
		(void)printf(" %2d,%-2d", major(fst.fsid), minor(fst.fsid));
	else
		(void)printf(" %-8s", getmnton(vn.v_mount));
	if (nflg)
		(void)sprintf(mode, "%o", fst.mode);
	else
		strmode(fst.mode, mode);
	(void)printf(" %6ld %10s", fst.fileid, mode);
	switch (vn.v_type) {
	case VBLK:
	case VCHR: {
		char *name;

		if (nflg || ((name = devname(fst.rdev, vn.v_type == VCHR ?
		    S_IFCHR : S_IFBLK)) == NULL))
			printf("  %2d,%-2d", major(fst.rdev), minor(fst.rdev));
		else
			printf(" %6s", name);
		break;
	}
	default:
		printf(" %6lu", fst.size);
	}
	rw[0] = '\0';
	if (flag & FREAD)
		strcat(rw, "r");
	if (flag & FWRITE)
		strcat(rw, "w");
	printf(" %2s", rw);
	if (filename && !fsflg)
		printf("  %s", filename);
	putchar('\n');
}

int
ufs_filestat(struct vnode *vp, struct filestat *fsp)
{
	struct inode inode;

	if (!KVM_READ(VTOI(vp), &inode, sizeof (inode))) {
		dprintf(stderr, "can't read inode at %p for pid %d\n",
		    (void *)VTOI(vp), Pid);
		return 0;
	}
	/*
	 * The st_dev from stat(2) is a dev_t. These kernel structures
	 * contain cdev pointers. We need to convert to dev_t to make
	 * comparisons
	 */
	fsp->fsid = dev2udev(inode.i_dev);
	fsp->fileid = (long)inode.i_number;
	fsp->mode = (mode_t)inode.i_mode;
	fsp->size = (u_long)inode.i_size;
#if should_be_but_is_hard
	/* XXX - need to load i_ump and i_din[12] from kernel memory */
	if (inode.i_ump->um_fstype == UFS1)
		fsp->rdev = inode.i_din1->di_rdev;
	else
		fsp->rdev = inode.i_din2->di_rdev;
#else
	fsp->rdev = 0;
#endif

	return 1;
}

int
devfs_filestat(struct vnode *vp, struct filestat *fsp)
{
	struct devfs_dirent devfs_dirent;
	struct mount mount;
	struct vnode vnode;

	if (!KVM_READ(vp->v_data, &devfs_dirent, sizeof (devfs_dirent))) {
		dprintf(stderr, "can't read devfs_dirent at %p for pid %d\n",
		    (void *)vp->v_data, Pid);
		return 0;
	}
	if (!KVM_READ(vp->v_mount, &mount, sizeof (mount))) {
		dprintf(stderr, "can't read mount at %p for pid %d\n",
		    (void *)vp->v_mount, Pid);
		return 0;
	}
	if (!KVM_READ(devfs_dirent.de_vnode, &vnode, sizeof (vnode))) {
		dprintf(stderr, "can't read vnode at %p for pid %d\n",
		    (void *)devfs_dirent.de_vnode, Pid);
		return 0;
	}
	fsp->fsid = (long)mount.mnt_stat.f_fsid.val[0];
	fsp->fileid = devfs_dirent.de_inode;
	fsp->mode = (devfs_dirent.de_mode & ~S_IFMT) | S_IFCHR;
	fsp->size = 0;
	fsp->rdev = dev2udev(vnode.v_rdev);

	return 1;
}

int
nfs_filestat(struct vnode *vp, struct filestat *fsp)
{
	struct nfsnode nfsnode;
	mode_t mode;

	if (!KVM_READ(VTONFS(vp), &nfsnode, sizeof (nfsnode))) {
		dprintf(stderr, "can't read nfsnode at %p for pid %d\n",
		    (void *)VTONFS(vp), Pid);
		return 0;
	}
	fsp->fsid = nfsnode.n_vattr.va_fsid;
	fsp->fileid = nfsnode.n_vattr.va_fileid;
	fsp->size = nfsnode.n_size;
	fsp->rdev = nfsnode.n_vattr.va_rdev;
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
	case VNON:
	case VBAD:
		return 0;
	};
	fsp->mode = mode;

	return 1;
}


char *
getmnton(struct mount *m)
{
	static struct mount mount;
	static struct mtab {
		struct mtab *next;
		struct mount *m;
		char mntonname[MNAMELEN];
	} *mhead = NULL;
	struct mtab *mt;

	for (mt = mhead; mt != NULL; mt = mt->next)
		if (m == mt->m)
			return (mt->mntonname);
	if (!KVM_READ(m, &mount, sizeof(struct mount))) {
		warnx("can't read mount table at %p", (void *)m);
		return (NULL);
	}
	if ((mt = malloc(sizeof (struct mtab))) == NULL)
		err(1, NULL);
	mt->m = m;
	bcopy(&mount.mnt_stat.f_mntonname[0], &mt->mntonname[0], MNAMELEN);
	mt->next = mhead;
	mhead = mt;
	return (mt->mntonname);
}

void
pipetrans(struct pipe *pi, int i, int flag)
{
	struct pipe pip;
	char rw[3];

	PREFIX(i);

	/* fill in socket */
	if (!KVM_READ(pi, &pip, sizeof(struct pipe))) {
		dprintf(stderr, "can't read pipe at %p\n", (void *)pi);
		goto bad;
	}

	printf("* pipe %8lx <-> %8lx", (u_long)pi, (u_long)pip.pipe_peer);
	printf(" %6d", (int)pip.pipe_buffer.cnt);
	rw[0] = '\0';
	if (flag & FREAD)
		strcat(rw, "r");
	if (flag & FWRITE)
		strcat(rw, "w");
	printf(" %2s", rw);
	putchar('\n');
	return;

bad:
	printf("* error\n");
}

void
socktrans(struct socket *sock, int i)
{
	static const char *stypename[] = {
		"unused",	/* 0 */
		"stream", 	/* 1 */
		"dgram",	/* 2 */
		"raw",		/* 3 */
		"rdm",		/* 4 */
		"seqpak"	/* 5 */
	};
#define	STYPEMAX 5
	struct socket	so;
	struct protosw	proto;
	struct domain	dom;
	struct inpcb	inpcb;
	struct unpcb	unpcb;
	int len;
	char dname[32];

	PREFIX(i);

	/* fill in socket */
	if (!KVM_READ(sock, &so, sizeof(struct socket))) {
		dprintf(stderr, "can't read sock at %p\n", (void *)sock);
		goto bad;
	}

	/* fill in protosw entry */
	if (!KVM_READ(so.so_proto, &proto, sizeof(struct protosw))) {
		dprintf(stderr, "can't read protosw at %p",
		    (void *)so.so_proto);
		goto bad;
	}

	/* fill in domain */
	if (!KVM_READ(proto.pr_domain, &dom, sizeof(struct domain))) {
		dprintf(stderr, "can't read domain at %p\n",
		    (void *)proto.pr_domain);
		goto bad;
	}

	if ((len = kvm_read(kd, (u_long)dom.dom_name, dname,
	    sizeof(dname) - 1)) < 0) {
		dprintf(stderr, "can't read domain name at %p\n",
		    (void *)dom.dom_name);
		dname[0] = '\0';
	}
	else
		dname[len] = '\0';

	if ((u_short)so.so_type > STYPEMAX)
		printf("* %s ?%d", dname, so.so_type);
	else
		printf("* %s %s", dname, stypename[so.so_type]);

	/*
	 * protocol specific formatting
	 *
	 * Try to find interesting things to print.  For tcp, the interesting
	 * thing is the address of the tcpcb, for udp and others, just the
	 * inpcb (socket pcb).  For unix domain, its the address of the socket
	 * pcb and the address of the connected pcb (if connected).  Otherwise
	 * just print the protocol number and address of the socket itself.
	 * The idea is not to duplicate netstat, but to make available enough
	 * information for further analysis.
	 */
	switch(dom.dom_family) {
	case AF_INET:
	case AF_INET6:
		getinetproto(proto.pr_protocol);
		if (proto.pr_protocol == IPPROTO_TCP ) {
			if (so.so_pcb) {
				if (kvm_read(kd, (u_long)so.so_pcb,
				    (char *)&inpcb, sizeof(struct inpcb))
				    != sizeof(struct inpcb)) {
					dprintf(stderr,
					    "can't read inpcb at %p\n",
					    (void *)so.so_pcb);
					goto bad;
				}
				printf(" %lx", (u_long)inpcb.inp_ppcb);
			}
		}
		else if (so.so_pcb)
			printf(" %lx", (u_long)so.so_pcb);
		break;
	case AF_UNIX:
		/* print address of pcb and connected pcb */
		if (so.so_pcb) {
			printf(" %lx", (u_long)so.so_pcb);
			if (kvm_read(kd, (u_long)so.so_pcb, (char *)&unpcb,
			    sizeof(struct unpcb)) != sizeof(struct unpcb)){
				dprintf(stderr, "can't read unpcb at %p\n",
				    (void *)so.so_pcb);
				goto bad;
			}
			if (unpcb.unp_conn) {
				char shoconn[4], *cp;

				cp = shoconn;
				if (!(so.so_rcv.sb_state & SBS_CANTRCVMORE))
					*cp++ = '<';
				*cp++ = '-';
				if (!(so.so_snd.sb_state & SBS_CANTSENDMORE))
					*cp++ = '>';
				*cp = '\0';
				printf(" %s %lx", shoconn,
				    (u_long)unpcb.unp_conn);
			}
		}
		break;
	default:
		/* print protocol number and socket address */
		printf(" %d %lx", proto.pr_protocol, (u_long)sock);
	}
	printf("\n");
	return;
bad:
	printf("* error\n");
}


/*
 * Read the cdev structure in the kernel in order to work out the
 * associated dev_t
 */
dev_t
dev2udev(struct cdev *dev)
{
	struct cdev si;

	if (KVM_READ(dev, &si, sizeof si)) {
		return si.si_udev;
	} else {
		dprintf(stderr, "can't convert cdev *%p to a dev_t\n", dev);
		return -1;
	}
}

/*
 * getinetproto --
 *	print name of protocol number
 */
void
getinetproto(int number)
{
	static int isopen;
	struct protoent *pe;

	if (!isopen)
		setprotoent(++isopen);
	if ((pe = getprotobynumber(number)) != NULL)
		printf(" %s", pe->p_name);
	else
		printf(" %d", number);
}

int
getfname(const char *filename)
{
	struct stat statbuf;
	DEVS *cur;

	if (stat(filename, &statbuf)) {
		warn("%s", filename);
		return(0);
	}
	if ((cur = malloc(sizeof(DEVS))) == NULL)
		err(1, NULL);
	cur->next = devs;
	devs = cur;

	cur->ino = statbuf.st_ino;
	cur->fsid = statbuf.st_dev;
	cur->name = filename;
	return(1);
}

void
usage(void)
{
	(void)fprintf(stderr,
 "usage: fstat [-fmnv] [-p pid] [-u user] [-N system] [-M core] [file ...]\n");
	exit(1);
}
