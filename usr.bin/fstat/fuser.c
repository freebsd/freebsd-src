/*-
 * Copyright (c) 2005,2009 Stanislav Sedov <stas@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <sysexits.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <assert.h>

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <sys/queue.h>
#include <sys/tty.h>
#define	_WANT_FILE
#include <sys/conf.h>
#include <sys/file.h>
#define	_KERNEL
#include <fs/devfs/devfs.h>
#include <fs/devfs/devfs_int.h>
#undef _KERNEL

#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include "common.h"
#include "functions.h"

/*
 * Local types
 */

enum {REQFILE, REQDEV, REQMNT}; /* Type of requested file */
typedef struct reqfile {
	ino_t			ino;
	dev_t			dev;	
	const char		*fname;
	int			type;
	SLIST_ENTRY(reqfile)	next;
} reqfile_t;
typedef SLIST_HEAD(, finfo) fds_head_t; /* List of opened files */

typedef struct pinfo {
	pid_t			pid;
	uid_t			uid;
	fds_head_t		fds;
	SLIST_ENTRY(pinfo)	next;
} pinfo_t;

typedef struct finfo{
	struct filestat		stat;
	int			uflags; /* How this file is used */
	SLIST_ENTRY(finfo)	next;
} finfo_t;

/*
 * Local definitions
 */

/* Option flags */
#define	UFLAG	0x01	/* -u flag: show users				*/
#define	FFLAG	0x02	/* -f flag: specified files only		*/
#define	CFLAG	0x04	/* -c flag: treat as mpoints			*/
#define	MFLAG	0x10	/* -m flag: mmapped files too			*/
#define	KFLAG	0x20	/* -k flag: send signal (SIGKILL by default)	*/

/* Macros for freeing SLISTs, probably must be in /sys/queue.h */
#define SLIST_FREE(head, field, freef) do {			\
    typeof(SLIST_FIRST(head)) __elm0;				\
    typeof(SLIST_FIRST(head)) __elm;				\
    SLIST_FOREACH_SAFE(__elm, (head), field, __elm0)		\
    	(void)(freef)(__elm);					\
} while(0);

/* File's usage flags */
#define UFL_RDIR	0x0001	/* root dir			*/
#define UFL_CDIR	0x0002	/* cwd				*/
#define UFL_JDIR	0x0004	/* jail root			*/
#define UFL_TRACEP	0x0008	/* trace vnode			*/
#define UFL_TEXTVP	0x0010	/* text(executable) file	*/
#define UFL_CTTY	0x0020	/* contolling tty		*/
#define UFL_MMAP	0x0040	/* file is mmapped		*/
#define UFL_FREAD	0x0080	/* file opened for reading	*/
#define UFL_FWRITE	0x0100	/* file opened for writing	*/
#define UFL_FAPPEND	0x0200	/* file opened as append-only	*/
#define UFL_FDIRECT	0x0400	/* file bypasses fs cache	*/
#define UFL_FSHLOCK	0x0800	/* shared lock is obtained	*/
#define UFL_FEXLOCK	0x1000	/* exclusive lock is obtained	*/

struct {
	int	val;
	char	ch;
} uflags[] = {
	{UFL_RDIR,	'r'},
	{UFL_JDIR,	'j'},
	{UFL_CDIR,	'c'},
	{UFL_TRACEP,	't'},
	{UFL_TEXTVP,	'x'},
	{UFL_CTTY,	'y'},
	{UFL_MMAP,	'm'},
	{UFL_FWRITE,	'w'},
	{UFL_FAPPEND,	'a'},
	{UFL_FDIRECT,	'd'},
	{UFL_FSHLOCK,	's'},
	{UFL_FEXLOCK,	'e'},
};
#define NUFLAGS (sizeof(uflags) / sizeof(*uflags))
	
/* Filesystem-specific handlers */
#define FSTYPE(fst) {#fst, fst##_filestat}
static struct {
	const char	*tag;
	int		(*handler)(kvm_t *kd, struct vnode *vp, struct filestat *fsp);
} fstypes[] = {
	FSTYPE(ufs),
	FSTYPE(devfs),
	FSTYPE(nfs),
	FSTYPE(msdosfs),
	FSTYPE(isofs),
/*
	FSTYPE(ntfs),
	FSTYPE(nwfs),
	FSTYPE(smbfs),
	FSTYPE(udf),
*/
};
#define NTYPES (sizeof(fstypes) / sizeof(*fstypes))

/*
 * Global vars
 */

kvm_t		*kd;		/* KVM descriptors	*/
static int	flags = 0;	/* Option flags		*/

/* List of requested files */
static SLIST_HEAD(, reqfile)	rfiles = SLIST_HEAD_INITIALIZER(&rfiles);

/* List of current processes */
static SLIST_HEAD(, pinfo)	prclist = SLIST_HEAD_INITIALIZER(&prclist);

/*
 * Prototypes
 */
static const struct vnode	*get_ctty	\
			__P((const struct kinfo_proc *p));
static int		vp2finfo	\
			__P((const struct vnode *vp, fds_head_t *h, int fl));
static void		print_file_info	\
			__P((pid_t pid, uid_t uid, int ufl));
static int		add_mmapped	\
			__P((const struct kinfo_proc *p, fds_head_t *head));
static int		str2sig		\
			__P((const char *str));
static void		usage		\
			__P((void)) __dead2;
static int		gather_pinfo	\
		 	__P((const struct kinfo_proc *p));
static int		addfile		\
			__P((const char *path));
static int		add_ofiles	\
			__P((const struct filedesc *fd, fds_head_t *head));
static int		get_uflags	\
			__P((const reqfile_t *rfile, const pinfo_t *pinfo));
static void		pinfo_free	\
			__P((pinfo_t *pinfo));
int			main		\
			__P((int argc, char *argv[]));

static void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-cfkmu] [-C core] [-K kernel]" \
	    " [-s signal] file ...\n", getprogname());

	exit(EX_USAGE);
}

void
print_file_info(pid, uid, ufl)
	pid_t		pid;
	uid_t		uid;
	int		ufl;
{
	uint	i;

	(void)fprintf(stdout, "%6d", pid);
	(void)fflush(stdout);

	for (i = 0; i < NUFLAGS; i++)
		if ((ufl & uflags[i].val) != 0)
			(void)fprintf(stderr, "%c", uflags[i].ch);

	if ((flags & UFLAG) != 0)
		(void)fprintf(stderr,"(%s)", user_from_uid(uid, 0));
	
	(void)fflush(stderr);
}

/*
 * Add file to the list.
 */
static int
addfile(path)
	const char	*path;
{
	struct stat	sb;
	int		type;
	reqfile_t	*rfile;

	assert(path);

	if (stat(path, &sb) != 0) {
		warn("%s", path);
		return 1;
	}

	rfile = (reqfile_t *)malloc(sizeof(reqfile_t));
	if (rfile == NULL)
		err(EX_OSERR, "malloc()");

	type = sb.st_mode & S_IFMT;

	rfile->ino = sb.st_ino;
	rfile->dev = sb.st_dev;
	rfile->fname = path;

	if ((flags & CFLAG) != 0)
		rfile->type = REQMNT;
	else if ((type == S_IFCHR || type == S_IFBLK) && ((flags & FFLAG) == 0))
		rfile->type = REQDEV;
	else 
		rfile->type = REQFILE;

	SLIST_INSERT_HEAD(&rfiles, rfile, next);

	return 0;	
}

/*
 * The purpose of this routine is to walk through list of fds, opened
 * by a given process and add suitable entries to list. 
 */
static int
add_ofiles(fd, head)
	const struct filedesc	*fd;
	fds_head_t		*head;
{
	struct file	**ofiles;
	struct file	file;
	int		nfiles;
	int		ufl;
	uint i;

	assert(head);
	assert(fd);

	nfiles = (fd->fd_lastfile + 1);
	if (nfiles <= 0) {
		return 1;
	}

#define OFSIZE (nfiles * sizeof(*ofiles))
	ofiles = (struct file **)malloc(OFSIZE);
	if (ofiles == NULL)
		err(EX_OSERR, "malloc()");

	if (!kvm_read_all(kd, (unsigned long)fd->fd_ofiles, ofiles, OFSIZE)) {
		warnx("can't read file structures at %p", fd->fd_ofiles);

		free(ofiles);
		return 1;
	}
#undef OFSIZE

	for (i = 0; i < (unsigned)nfiles; i++) {
		if (ofiles[i] == 0)
			continue;

		if (!kvm_read_all(kd, (unsigned long)ofiles[i], &file,
		    sizeof(file))) {
			warnx("can't read file structure at %p", ofiles[i]);
			continue;
		}

		ufl = 0;
		if ((file.f_flag & FREAD) != 0)
			ufl |= UFL_FREAD;
		if ((file.f_flag & FWRITE) != 0)
			ufl |= UFL_FWRITE;
		if ((file.f_flag & O_APPEND) != 0)
			ufl |= UFL_FAPPEND;
		if ((file.f_flag & O_DIRECT) != 0)
			ufl |= UFL_FDIRECT;
		if ((file.f_flag & O_SHLOCK) != 0)
			ufl |= UFL_FSHLOCK;
		if ((file.f_flag & O_EXLOCK) != 0)
			ufl |= UFL_FEXLOCK;

		switch (file.f_type) {
               	case DTYPE_VNODE:	
		case DTYPE_FIFO:
			(void)vp2finfo(file.f_vnode, head, ufl);

		default:
			continue;
		}
	}

	free(ofiles);

	return 0;
}

/*
 * This routine returns controlling tty of the process, if exist.
 */
const struct vnode *
get_ctty(p)
	const struct kinfo_proc	*p;
{
	struct proc	proc;
	struct pgrp	pgrp;
	struct session	sess;

	assert(p);
	if (!kvm_read_all(kd, (unsigned long)p->ki_paddr, &proc,
	    sizeof(proc))) {
		warnx("can't read proc struct at %p for pid %d", \
		    p->ki_paddr, p->ki_pid);
		return NULL;
	}

	if (proc.p_pgrp == NULL)
		return NULL;

	if (!kvm_read_all(kd, (unsigned long)proc.p_pgrp, &pgrp, sizeof(pgrp))) {
		warnx("can't read pgrp struct at %p for pid %d", \
		    proc.p_pgrp, p->ki_pid);
		return NULL;
	}
	
	if (!kvm_read_all(kd, (unsigned long)pgrp.pg_session, &sess,
	    sizeof(sess))) {
		warnx("can't read session struct at %p for pid %d", \
		    pgrp.pg_session, p->ki_pid);
		return NULL;
	}

	return sess.s_ttyvp;
}

/*
 * The purpose of this routine is to build the entire pinfo structure for
 * given process. The structure's pointer will be inserted in the list.
 */
int
gather_pinfo(p)
	const struct kinfo_proc	*p;
{
	struct filedesc	fd_info;
	pinfo_t		*pinfo;

	assert(p);
	if (p->ki_stat == SZOMB || p->ki_fd == NULL)
		return 1;

	if (!kvm_read_all(kd, (unsigned long)p->ki_fd, &fd_info,
	    sizeof(fd_info))) {
		warnx("can't read open file's info at %p for pid %d", \
		    p->ki_fd, p->ki_pid);
		return 1;
	}

	pinfo = (pinfo_t *)malloc(sizeof(pinfo_t));
	if (pinfo == NULL)
		err(EX_OSERR, "malloc()");

	pinfo->pid = p->ki_pid;
	pinfo->uid = p->ki_uid;
	SLIST_INIT(&pinfo->fds);

	/* Add files from process's open fds list */
	(void)add_ofiles(&fd_info, &pinfo->fds);

	if ((flags & MFLAG) != 0)
		(void)add_mmapped(p, &pinfo->fds);

	(void)vp2finfo(p->ki_tracep, &pinfo->fds, \
	    UFL_FREAD|UFL_FWRITE|UFL_TRACEP);

	(void)vp2finfo(p->ki_textvp, &pinfo->fds, UFL_FREAD|UFL_TEXTVP);

	(void)vp2finfo(get_ctty(p), &pinfo->fds, UFL_CTTY);

	(void)vp2finfo(fd_info.fd_rdir, &pinfo->fds, UFL_FREAD|UFL_RDIR);

	(void)vp2finfo(fd_info.fd_cdir, &pinfo->fds, UFL_FREAD|UFL_CDIR);

	(void)vp2finfo(fd_info.fd_jdir, &pinfo->fds, UFL_FREAD|UFL_JDIR);

	SLIST_INSERT_HEAD(&prclist, pinfo, next);

	return 0;
}

/*
 * Insert finfo structure for given vnode into the list
 */
static int
vp2finfo(vp, head, ufl)
	const struct vnode	*vp;
	fds_head_t	*head;
	int		ufl;
{
	struct vnode		vn;
	finfo_t		*finfo;
	char		tag[8]; /* Max expected fs name length */
	uint		found, i;

	assert(head);
	if (vp == NULL)
		return 1;

	finfo = (finfo_t *)malloc(sizeof(finfo_t));
	if (finfo == NULL)
		err(EX_OSERR, "malloc()");

	if (!kvm_read_all(kd, (unsigned long)vp, &vn, sizeof(vn))) {
		warnx("can't read vnode at %p", vp);
		return 1;
	}

	if (!kvm_read_all(kd, (unsigned long)vn.v_tag, &tag, sizeof(tag))) {
		warnx("can't read v_tag at %p", vp);
		return 1;
	}
	tag[sizeof(tag) - 1] = 0;

	if (vn.v_type == VNON || vn.v_type == VBAD)
		return 1;

	for (i = 0, found = 0; i < NTYPES; i++)
		if (!strcmp(fstypes[i].tag, tag)) {
                        if (fstypes[i].handler(kd, &vn, &(finfo->stat)) != 0)
				return 1;
			found = 1;
			break;
		}
	if (found == 0)
		return 1;

	finfo->uflags = ufl;
	SLIST_INSERT_HEAD(head, finfo, next);

	return 0;	
}

/*
 * This routine walks through linked list of opened files and gathers
 * informations how given file is used.
 */
static int
get_uflags(rfile, pinfo)
	const reqfile_t	*rfile;
	const pinfo_t	*pinfo;
{
	finfo_t	*fd;
	int	ufl = 0;

	assert(rfile);
	assert(pinfo);

	switch (rfile->type) {
	case REQFILE:
		SLIST_FOREACH(fd, &pinfo->fds, next)
			if (fd->stat.fileid == rfile->ino && \
			    	fd->stat.fsid == rfile->dev)
					ufl |= fd->uflags;

		return ufl;

	case REQMNT:
		SLIST_FOREACH(fd, &pinfo->fds, next)
			if (fd->stat.fsid == rfile->dev)
					ufl |= fd->uflags;

		return ufl;

	case REQDEV:
		SLIST_FOREACH(fd, &pinfo->fds, next)
			if ((fd->stat.fileid == rfile->ino && \
			    fd->stat.fsid == rfile->dev) || \
			    fd->stat.fsid ==  rfile->ino)
					ufl |= fd->uflags;

		return ufl;

	default:
		break;
	}

	return 0;
}

/*
 * Helper routine to free pinfo structure
 */
static void
pinfo_free(pinfo)
	pinfo_t	*pinfo;
{
	
	assert(pinfo);
	SLIST_FREE(&pinfo->fds, next, free);
	free(pinfo);
}

int
do_fuser(argc, argv)
	int	argc;
	char	*argv[];	
{
	reqfile_t	*rfile;
	pinfo_t		*pinfo;
	struct kinfo_proc	*procs;
	char		buf[_POSIX2_LINE_MAX]; /* KVM mandatory */
	char		ch;
	int		ufl, cnt;
	int		sig = SIGKILL; /* Default to kill */
	char		*ep;
	char		*kernimg = NULL; /* We are using curr. sys by default */
	char		*mcore = NULL;

	while ((ch = getopt(argc, argv, "C:K:cfkms:u")) != -1)
		switch(ch) {
		case 'f':
			if ((flags & CFLAG) != 0)
				usage();
			flags |= FFLAG;
			break;

		case 'c':
			if ((flags & FFLAG) != 0)
				usage();
			flags |= CFLAG;
			break;

		case 'K':
			kernimg = optarg;
			break;

		case 'C':
			mcore = optarg;
			break;

		case 'u':
			flags |= UFLAG;
			break;

		case 'm':
			flags |= MFLAG;
			break;

		case 'k':
			flags |= KFLAG;
			break;

		case 's':
			if (isdigit(*optarg)) {
				sig = strtol(optarg, &ep, 10);
				if (*ep != '\0' || sig < 0 || sig >= sys_nsig)
					errx(EX_USAGE, "illegal signal number" \
					    ": %s", optarg);
			}
			else {
				sig = str2sig(optarg);
				if (sig < 0)
					errx(EX_USAGE, "illegal signal name: " \
					    "%s", optarg);
			}
			break;

		default:
			usage();
		}

	argv += optind;
	argc -= optind;

	assert(argc >= 0);
	if (argc == 0)
		usage();

	while (argc--)
		(void)addfile(argv[argc]);

	if (SLIST_EMPTY(&rfiles))
		errx(EX_IOERR, "files not accessible");

	kd = kvm_openfiles(kernimg, mcore, NULL, O_RDONLY, buf);
	if (kd == NULL)
		errx(EX_OSERR, "kvm_openfiles(): %s", buf);

	procs = kvm_getprocs(kd, KERN_PROC_ALL, 0, &cnt);
	if (procs == NULL)
		errx(EX_OSERR, "kvm_getproc(): %s", buf);

	while(cnt--)
		(void)gather_pinfo(procs++);

	SLIST_FOREACH(rfile, &rfiles, next) {
		(void)fprintf(stderr, "%s:", rfile->fname);
		(void)fflush(stderr);
		
		SLIST_FOREACH(pinfo, &prclist, next) {
			ufl = get_uflags(rfile, pinfo);

			if (ufl != 0) {
				print_file_info(pinfo->pid, \
				    pinfo->uid, ufl);
				if ((flags & KFLAG) != 0)
					(void)kill(pinfo->pid, sig);
			}
		}
		(void)fprintf(stderr, "\n");
	}

	SLIST_FREE(&rfiles, next, free);
	SLIST_FREE(&prclist, next, pinfo_free);
	(void)kvm_close(kd);

	return 0;
		
}

int
add_mmapped(p, head)
	const struct kinfo_proc	*p;
	fds_head_t		*head;
{
	vm_map_t map;
	struct vmspace vmspace;
	struct vm_map_entry entry;
	vm_map_entry_t entryp;
	struct vm_object object;
	vm_object_t objp;
	int ufl;

	assert(p);
	if (!kvm_read_all(kd, (unsigned long)p->ki_vmspace, &vmspace,
	    sizeof(vmspace))) {
		warnx("can't read vmspace at %p for pid %d\n",
		    (void *)p->ki_vmspace, p->ki_pid);
		return 1;
	}

	map = &vmspace.vm_map;

	for (entryp = map->header.next;
	    entryp != &p->ki_vmspace->vm_map.header; entryp = entry.next) {
		if (!kvm_read_all(kd, (unsigned long)entryp, &entry,
		    sizeof(entry))) {
			warnx("can't read vm_map_entry at %p for pid %d\n",
			    (void *)entryp, p->ki_pid);
			return 1;
		}

		if (entry.eflags & MAP_ENTRY_IS_SUB_MAP)
			continue;

		if ((objp = entry.object.vm_object) == NULL)
			continue;

		for (; objp; objp = object.backing_object) {
			if (!kvm_read_all(kd, (unsigned long)objp, &object,
			    sizeof(object))) {
				warnx("can't read vm_object at %p for pid %d\n",
				    (void *)objp, p->ki_pid);
				return 1;
			}
		}

		ufl = (entry.protection & VM_PROT_READ ? UFL_FREAD : 0);
		ufl |= (entry.protection & VM_PROT_WRITE ? UFL_FWRITE : 0);
		ufl |= UFL_MMAP;

		if (object.type == OBJT_VNODE)
			(void)vp2finfo((struct vnode *)object.handle, head, \
			    ufl);
	}

	return 0;
}

/*
 * Returns signal number for it's string representation
 */
static int
str2sig(str)
	const char	*str;
{
        int n;

        if (!strncasecmp(str, "sig", (size_t)3))
                str += 3;

        for (n = 1; n < sys_nsig; n++) {
                if (!strcasecmp(sys_signame[n], str))
                        return (n);
        }

        return -1;
}
