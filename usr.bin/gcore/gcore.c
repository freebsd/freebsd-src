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
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)gcore.c	8.2 (Berkeley) 9/23/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
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
#include <sys/lock.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/sysctl.h>

#include <machine/vmparam.h>

#include <a.out.h>
#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static void	core __P((int, int, struct kinfo_proc *));
static void	datadump __P((int, int, struct kinfo_proc *, u_long, int));
static void	killed __P((int));
static void	restart_target __P((void));
static void	usage __P((void)) __dead2;
static void	userdump __P((int, struct kinfo_proc *, u_long, int));

kvm_t *kd;

static int data_offset;
static pid_t pid;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct kinfo_proc *ki = NULL;
	struct exec exec;
	int ch, cnt, efd, fd, sflag, uid;
	char *binfile, *corefile;
	char errbuf[_POSIX2_LINE_MAX], fname[MAXPATHLEN];
	int is_aout;

	sflag = 0;
	corefile = NULL;
        while ((ch = getopt(argc, argv, "c:s")) != -1) {
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

	/* XXX we should check that the pid argument is really a number */
	switch (argc) {
	case 1:
		pid = atoi(argv[0]);
		asprintf(&binfile, "/proc/%d/file", pid);
		if (binfile == NULL)
			errx(1, "allocation failure");
		break;
	case 2:
		pid = atoi(argv[1]);
		binfile = argv[0];
		break;
	default:
		usage();
	}

	efd = open(binfile, O_RDONLY, 0);
	if (efd < 0)
		err(1, "%s", binfile);

	cnt = read(efd, &exec, sizeof(exec));
	if (cnt != sizeof(exec))
		errx(1, "%s exec header: %s",
		    binfile, cnt > 0 ? strerror(EIO) : strerror(errno));
	if (!N_BADMAG(exec)) {
		is_aout = 1;
		/*
		 * This legacy a.out support uses the kvm interface instead
		 * of procfs.
		 */
		kd = kvm_openfiles(0, 0, 0, O_RDONLY, errbuf);
		if (kd == NULL)
			errx(1, "%s", errbuf);

		uid = getuid();

		ki = kvm_getprocs(kd, KERN_PROC_PID, pid, &cnt);
		if (ki == NULL || cnt != 1)
			errx(1, "%d: not found", pid);

		if (ki->ki_ruid != uid && uid != 0)
			errx(1, "%d: not owner", pid);

		if (ki->ki_stat == SZOMB)
			errx(1, "%d: zombie", pid);

		if (ki->ki_flag & P_WEXIT)
			errx(1, "%d: process exiting", pid);
		if (ki->ki_flag & P_SYSTEM)	/* Swapper or pagedaemon. */
			errx(1, "%d: system process", pid);
		if (exec.a_text != ptoa(ki->ki_tsize))
			errx(1, "The executable %s does not belong to"
			    " process %d!\n"
			    "Text segment size (in bytes): executable %ld,"
			    " process %d", binfile, pid, exec.a_text, 
			     ptoa(ki->ki_tsize));
		data_offset = N_DATOFF(exec);
	} else if (IS_ELF(*(Elf_Ehdr *)&exec)) {
		is_aout = 0;
		close(efd);
	} else
		errx(1, "Invalid executable file");

	if (corefile == NULL) {
		(void)snprintf(fname, sizeof(fname), "core.%d", pid);
		corefile = fname;
	}
	fd = open(corefile, O_RDWR|O_CREAT|O_TRUNC, DEFFILEMODE);
	if (fd < 0)
		err(1, "%s", corefile);

	if (sflag) {
		signal(SIGHUP, killed);
		signal(SIGINT, killed);
		signal(SIGTERM, killed);
		if (kill(pid, SIGSTOP) == -1)
			err(1, "%d: stop signal", pid);
		atexit(restart_target);
	}

	if (is_aout)
		core(efd, fd, ki);
	else
		elf_coredump(fd, pid);

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
	int tsize = ki->ki_tsize;
	int dsize = ki->ki_dsize;
	int ssize = ki->ki_ssize;
	int cnt;

	/* Read in user struct */
	cnt = kvm_read(kd, (u_long)ki->ki_addr, &uarea, sizeof(uarea));
	if (cnt != sizeof(uarea))
		errx(1, "read user structure: %s",
		    cnt > 0 ? strerror(EIO) : strerror(errno));

	/*
	 * Fill in the eproc vm parameters, since these are garbage unless
	 * the kernel is dumping core or something.
	 */
	uarea.user.u_kproc = *ki;

	/* Dump user area */
	cnt = write(fd, &uarea, sizeof(uarea));
	if (cnt != sizeof(uarea))
		errx(1, "write user structure: %s",
		    cnt > 0 ? strerror(EIO) : strerror(errno));

	/* Dump data segment */
	datadump(efd, fd, ki, USRTEXT + ctob(tsize), dsize);

	/* Dump stack segment */
	userdump(fd, ki, USRSTACK - ctob(ssize), ssize);

	/* Dump machine dependent portions of the core. */
	md_core(kd, fd, ki);
}

void
datadump(efd, fd, kp, addr, npage)
	register int efd;
	register int fd;
	struct kinfo_proc *kp;
	register u_long addr;
	register int npage;
{
	register int cc, delta;
	char buffer[PAGE_SIZE];

	delta = data_offset - addr;
	while (--npage >= 0) {
		cc = kvm_uread(kd, kp, addr, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE) {
			/* Try to read the page from the executable. */
			if (lseek(efd, (off_t)addr + delta, SEEK_SET) == -1)
				err(1, "seek executable: %s", strerror(errno));
			cc = read(efd, buffer, sizeof(buffer));
			if (cc != sizeof(buffer)) {
				if (cc < 0)
					err(1, "read executable");
				else	/* Assume untouched bss page. */
					bzero(buffer, sizeof(buffer));
			}
		}
		cc = write(fd, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE)
			errx(1, "write data segment: %s",
			    cc > 0 ? strerror(EIO) : strerror(errno));
		addr += PAGE_SIZE;
	}
}

static void
killed(sig)
	int sig;
{
	restart_target();
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

static void
restart_target()
{
	kill(pid, SIGCONT);
}

void
userdump(fd, kp, addr, npage)
	register int fd;
	struct kinfo_proc *kp;
	register u_long addr;
	register int npage;
{
	register int cc;
	char buffer[PAGE_SIZE];

	while (--npage >= 0) {
		cc = kvm_uread(kd, kp, addr, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE)
			/* Could be an untouched fill-with-zero page. */
			bzero(buffer, PAGE_SIZE);
		cc = write(fd, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE)
			errx(1, "write stack segment: %s",
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
