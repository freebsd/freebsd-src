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

#include "common.h"
#include "functions.h"
#include "libprocstat.h"

int 	fsflg,	/* show files on same filesystem as file(s) argument */
	pflg,	/* show files open by a particular pid */
	uflg;	/* show files open by a particular (effective) user */
int 	checkfile; /* true if restricting to particular files or filesystems */
int	nflg;	/* (numerical) display f.s. and rdev as dev_t */
int	mflg;	/* include memory-mapped files */

typedef struct devs {
	struct devs	*next;
	long		fsid;
	long		ino;
	const char	*name;
} DEVS;

DEVS *devs;
char *memf, *nlistf;

static void fstat1(int what, int arg);
static void dofiles(struct procstat *procstat, struct kinfo_proc *p);
void dofiles_kinfo(struct kinfo_proc *kp);
void dommap(struct kinfo_proc *kp);
void vtrans(struct vnode *vp, int i, int flag, const char *uname, const char *cmd, int pid);
char *getmnton(struct mount *m);
void pipetrans(struct pipe *pi, int i, int flag, const char *uname, const char *cmd, int pid);
void socktrans(struct socket *sock, int i, const char *uname, const char *cmd, int pid);
void ptstrans(struct tty *tp, int i, int flag, const char *uname, const char *cmd, int pid);
void getinetproto(int number);
int  getfname(const char *filename);
void usage(void);
void vtrans_kinfo(struct kinfo_file *, int i, int flag, const char *uname, const char *cmd, int pid);
static void print_file_info(struct procstat *procstat, struct filestat *fst, const char *uname, const char *cmd, int pid);

static void
print_socket_info(struct procstat *procstat, struct filestat *fst);
static void
print_pipe_info(struct procstat *procstat, struct filestat *fst);
static void
print_pts_info(struct procstat *procstat, struct filestat *fst);
static void
print_vnode_info(struct procstat *procstat, struct filestat *fst);

int
do_fstat(int argc, char **argv)
{
	struct passwd *passwd;
	int arg, ch, what;

	arg = 0;
	what = KERN_PROC_PROC;
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

		fstat1(what, arg);
	exit(0);
}

static void
fstat1(int what, int arg)
{
	struct kinfo_proc *p;
	struct procstat *procstat;
	int cnt;
	int i;

	procstat = procstat_open(nlistf, memf);
	if (procstat == NULL)
		errx(1, "procstat_open()");
	p = procstat_getprocs(procstat, what, arg, &cnt);
	if (p == NULL)
		errx(1, "procstat_getprocs()");

	/*
	 * Print header.
	 */
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

	/*
	 * Go through the process list.
	 */
	for (i = 0; i < cnt; i++) {
		if (p[i].ki_stat == SZOMB)
			continue;
		dofiles(procstat, &p[i]);
/*
		if (mflg)
			dommap(procstat, &p[i]);
*/
	}
	free(p);
	procstat_close(procstat);
}

static void
dofiles(struct procstat *procstat, struct kinfo_proc *kp)
{
	struct filestat *fst;
	unsigned int count;
	const char *cmd, *uname;
	int pid;
	unsigned int i;

	uname = user_from_uid(kp->ki_uid, 0);
	pid = kp->ki_pid;
	cmd = kp->ki_comm;

	fst = procstat_getfiles(procstat, kp, &count);
	if (fst == NULL)
		return;

	for (i = 0; i < count; i++)
		print_file_info(procstat, &fst[i], uname, cmd, pid);
}


static void
print_file_info(struct procstat *procstat, struct filestat *fst,
    const char *uname, const char *cmd, int pid)
{
	const char *badtype, *filename;

	badtype = NULL;
	filename = NULL;
	if (fst->type == PS_FST_TYPE_VNODE || fst->type == PS_FST_TYPE_FIFO) {
		if (fst->flags & PS_FST_FLAG_ERROR)
			badtype = "error";
		else if (fst->flags == PS_FST_FLAG_UNKNOWNFS)
			badtype = "unknown";
		else if (fst->vtype == PS_FST_VTYPE_VNON)
			badtype = "none";
		else if (fst->vtype == PS_FST_VTYPE_VBAD)
			badtype = "bad";

		if (checkfile) {
			int fsmatch = 0;
			DEVS *d;

			if (badtype)
				return;
			for (d = devs; d != NULL; d = d->next)
				if (d->fsid == fst->fsid) {
					fsmatch = 1;
					if (d->ino == fst->fileid) {
						filename = d->name;
						break;
					}
				}
			if (fsmatch == 0 || (filename == NULL && fsflg == 0))
				return;
		}
	} else if (checkfile != 0)
		return;

	/*
	 * Print entry prefix.
	 */
	printf("%-8.8s %-10s %5d", uname, cmd, pid);
	switch(fst->fd) {
	case PS_FST_FD_TEXT:
		printf(" text");
		break;
	case PS_FST_FD_CDIR:
		printf("   wd");
		break;
	case PS_FST_FD_RDIR:
		printf(" root");
		break;
	case PS_FST_FD_TRACE:
		printf("   tr");
		break;
	case PS_FST_FD_MMAP:
		printf(" mmap");
		break;
	case PS_FST_FD_JAIL:
		printf(" jail");
		break;
	default:
		printf(" %4d", fst->fd);
		break;
	}
	if (badtype) {
		(void)printf(" -         -  %10s    -\n", badtype);
		return;
	}

	/*
	 * Print type-specific data.
	 */
	switch (fst->type) {
	case PS_FST_TYPE_FIFO:
	case PS_FST_TYPE_VNODE:
		print_vnode_info(procstat, fst);
		break;
	case PS_FST_TYPE_SOCKET:
		print_socket_info(procstat, fst);
		break;
	case PS_FST_TYPE_PIPE:
		print_pipe_info(procstat, fst);
		break;
	case PS_FST_TYPE_PTS:
		print_pts_info(procstat, fst);
		break;
	default:	
		dprintf(stderr,
		    "unknown file type %d for file %d of pid %d\n",
		    fst->type, fst->fd, pid);
	}
	if (filename && !fsflg)
		printf("  %s", filename);
	putchar('\n');
}

static void
print_socket_info(struct procstat *procstat __unused, struct filestat *fst __unused)
{

	printf(" not implemented\n");
}

static void
print_pipe_info(struct procstat *procstat __unused, struct filestat *fst __unused)
{

	printf(" not implemented\n");
}

static void
print_pts_info(struct procstat *procstat __unused, struct filestat *fst __unused)
{

	printf(" not implemented\n");
}

static void
print_vnode_info(struct procstat *procstat __unused, struct filestat *fst)
{
	char mode[15];
	char rw[3];

	if (nflg)
		(void)printf(" %2d,%-2d", major(fst->fsid), minor(fst->fsid));
	else if (fst->mntdir != NULL)
		(void)printf(" %-8s", fst->mntdir);
	if (nflg)
		(void)sprintf(mode, "%o", fst->mode);
	else {
		strmode(fst->mode, mode);
	}
	(void)printf(" %6ld %10s", fst->fileid, mode);
	switch (fst->vtype) {
	case PS_FST_VTYPE_VBLK:
	case PS_FST_VTYPE_VCHR: {
		char *name;

#if 0
		name = procstat_devname(procstat, fst->rdev,
		    fst->vtype = PS_FST_VTYPE_VBLK ? S_IFBLK : S_IFCHR);
#else
		name = NULL;
#endif
		if (nflg || !name)
			printf("  %2d,%-2d", major(fst->rdev), minor(fst->rdev));
		else {
			printf(" %6s", name);
		}
		break;
	}
	default:
		printf(" %6lu", fst->size);
	}
	rw[0] = '\0';
	if (fst->fflags & PS_FST_FFLAG_READ)
		strcat(rw, "r");
	if (fst->fflags & PS_FST_FFLAG_WRITE)
		strcat(rw, "w");
	printf(" %2s", rw);
}

int
getfname(const char *filename)
{
	struct stat statbuf;
	DEVS *cur;

	if (stat(filename, &statbuf)) {
		warn("%s", filename);
		return (0);
	}
	if ((cur = malloc(sizeof(DEVS))) == NULL)
		err(1, NULL);
	cur->next = devs;
	devs = cur;

	cur->ino = statbuf.st_ino;
	cur->fsid = statbuf.st_dev;
	cur->name = filename;
	return (1);
}

void
usage(void)
{
	(void)fprintf(stderr,
 "usage: fstat [-fmnv] [-M core] [-N system] [-p pid] [-u user] [file ...]\n");
	exit(1);
}
