/*-
 * Copyright (c) 1992, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)gcore.c	8.2 (Berkeley) 9/23/93";
#endif /* not lint */

/*
 * Originally written by Eric Cooper in Fall 1981.
 * Inspired by a version 6 program by Len Levin, 1978.
 * Several pieces of code lifted from Bill Joy's 4BSD ps.
 * Most recently, hacked beyond recognition for 4.4BSD by Steven McCanne,
 * Lawrence Berkeley Laboratory.
 *
 * Portions of this software were developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA
 * contract BG 91-66 and contributed to Berkeley.
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/sysctl.h>

#include <machine/vmparam.h>

#include <a.out.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

void	core __P((int, int, struct kinfo_proc *));
void	datadump __P((int, int, struct proc *, u_long, int));
void	usage __P((void));
void	userdump __P((int, struct proc *, u_long, int));

kvm_t *kd;
/* XXX undocumented routine, should be in kvm.h? */
ssize_t kvm_uread __P((kvm_t *, const struct proc *, u_long, char *, size_t));


static int data_offset;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register struct proc *p;
	struct kinfo_proc *ki;
	struct exec exec;
	int ch, cnt, efd, fd, pid, sflag, uid;
	char *corefile, errbuf[_POSIX2_LINE_MAX], fname[MAXPATHLEN + 1];

	sflag = 0;
	corefile = NULL;
        while ((ch = getopt(argc, argv, "c:s")) != EOF) {
                switch (ch) {
                case 'c':
			corefile = optarg;
                        break;
		case 's':
			sflag = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (argc != 2)
		usage();

	kd = kvm_openfiles(0, 0, 0, O_RDONLY, errbuf);
	if (kd == NULL)
		err(1, "%s", errbuf);

	uid = getuid();
	pid = atoi(argv[1]);

	ki = kvm_getprocs(kd, KERN_PROC_PID, pid, &cnt);
	if (ki == NULL || cnt != 1)
		err(1, "%d: not found", pid);

	p = &ki->kp_proc;
	if (ki->kp_eproc.e_pcred.p_ruid != uid && uid != 0)
		err(1, "%d: not owner", pid);

	if (p->p_stat == SZOMB)
		err(1, "%d: zombie", pid);

	if (p->p_flag & P_WEXIT)
		err(0, "process exiting");
	if (p->p_flag & P_SYSTEM)	/* Swapper or pagedaemon. */
		err(1, "%d: system process");

	if (corefile == NULL) {
		(void)snprintf(fname, sizeof(fname), "core.%d", pid);
		corefile = fname;
	}
	fd = open(corefile, O_RDWR|O_CREAT|O_TRUNC, DEFFILEMODE);
	if (fd < 0)
		err(1, "%s: %s\n", corefile, strerror(errno));

	efd = open(argv[0], O_RDONLY, 0);
	if (efd < 0)
		err(1, "%s: %s\n", argv[0], strerror(errno));

	cnt = read(efd, &exec, sizeof(exec));
	if (cnt != sizeof(exec))
		err(1, "%s exec header: %s",
		    argv[0], cnt > 0 ? strerror(EIO) : strerror(errno));

	data_offset = N_DATOFF(exec);

	if (sflag && kill(pid, SIGSTOP) < 0)
		err(0, "%d: stop signal: %s", pid, strerror(errno));

	core(efd, fd, ki);

	if (sflag && kill(pid, SIGCONT) < 0)
		err(0, "%d: continue signal: %s", pid, strerror(errno));
	(void)close(fd);

	exit(0);
}

/*
 * core --
 *	Build the core file.
 */
void
core(efd, fd, ki)
	int efd;
	int fd;
	struct kinfo_proc *ki;
{
	union {
		struct user user;
		char ubytes[ctob(UPAGES)];
	} uarea;
	struct proc *p = &ki->kp_proc;
	int tsize = ki->kp_eproc.e_vm.vm_tsize;
	int dsize = ki->kp_eproc.e_vm.vm_dsize;
	int ssize = ki->kp_eproc.e_vm.vm_ssize;
	int cnt;

	/* Read in user struct */
	cnt = kvm_read(kd, (u_long)p->p_addr, &uarea, sizeof(uarea));
	if (cnt != sizeof(uarea))
		err(1, "read user structure: %s",
		    cnt > 0 ? strerror(EIO) : strerror(errno));

	/*
	 * Fill in the eproc vm parameters, since these are garbage unless
	 * the kernel is dumping core or something.
	 */
	uarea.user.u_kproc = *ki;

	/* Dump user area */
	cnt = write(fd, &uarea, sizeof(uarea));
	if (cnt != sizeof(uarea))
		err(1, "write user structure: %s",
		    cnt > 0 ? strerror(EIO) : strerror(errno));

	/* Dump data segment */
	datadump(efd, fd, p, USRTEXT + ctob(tsize), dsize);

	/* Dump stack segment */
	userdump(fd, p, USRSTACK - ctob(ssize), ssize);

	/* Dump machine dependent portions of the core. */
	md_core(kd, fd, ki);
}

void
datadump(efd, fd, p, addr, npage)
	register int efd;
	register int fd;
	struct proc *p;
	register u_long addr;
	register int npage;
{
	register int cc, delta;
	char buffer[PAGE_SIZE];

	delta = data_offset - addr;
	while (--npage >= 0) {
		cc = kvm_uread(kd, p, addr, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE) {
			/* Try to read the page from the executable. */
			if (lseek(efd, (off_t)addr + delta, SEEK_SET) == -1)
				err(1, "seek executable: %s", strerror(errno));
			cc = read(efd, buffer, sizeof(buffer));
			if (cc != sizeof(buffer))
				if (cc < 0)
					err(1, "read executable: %s",
					    strerror(errno));
				else	/* Assume untouched bss page. */
					bzero(buffer, sizeof(buffer));
		}
		cc = write(fd, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE)
			err(1, "write data segment: %s",
			    cc > 0 ? strerror(EIO) : strerror(errno));
		addr += PAGE_SIZE;
	}
}

void
userdump(fd, p, addr, npage)
	register int fd;
	struct proc *p;
	register u_long addr;
	register int npage;
{
	register int cc;
	char buffer[PAGE_SIZE];

	while (--npage >= 0) {
		cc = kvm_uread(kd, p, addr, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE)
			/* Could be an untouched fill-with-zero page. */
			bzero(buffer, PAGE_SIZE);
		cc = write(fd, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE)
			err(1, "write stack segment: %s",
			    cc > 0 ? strerror(EIO) : strerror(errno));
		addr += PAGE_SIZE;
	}
}

void
usage()
{
	(void)fprintf(stderr, "usage: gcore [-s] [-c core] executable pid\n");
	exit(1);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(int fatal, const char *fmt, ...)
#else
err(fatal, fmt, va_alist)
	int fatal;
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "gcore: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}
