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

#if 0
#ifndef lint
static char sccsid[] = "@(#)gcore.c	8.2 (Berkeley) 9/23/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/linker_set.h>

#include <arpa/inet.h>
#include <machine/elf.h>
#include <machine/vmparam.h>

#include <a.out.h>
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

static void	datadump(int, int, struct kinfo_proc *, u_long, int);
static void	userdump(int, struct kinfo_proc *, u_long, int);

static kvm_t *kd;

static int data_offset;
static struct kinfo_proc *ki;

static int
aoutident(int efd, pid_t pid, char *binfile)
{
	struct exec exec;
	int cnt;
	uid_t uid;
	char errbuf[_POSIX2_LINE_MAX];

	cnt = read(efd, &exec, sizeof(exec));
	if (cnt != sizeof(exec))
		return (0);
	if (!N_BADMAG(exec)) {
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
		return (1);
	}
	return (0);
}

/*
 * core --
 *	Build the core file.
 */
void
aoutcore(int efd, int fd, pid_t pid)
{
	union {
		struct user user;
		struct {
			char uabytes[ctob(UAREA_PAGES)];
			char ksbytes[ctob(KSTACK_PAGES)];
		} bytes;
	} uarea;
	int tsize = ki->ki_tsize;
	int dsize = ki->ki_dsize;
	int ssize = ki->ki_ssize;
	int cnt;

	/* Read in user struct */
	cnt = kvm_read(kd, (u_long)ki->ki_addr, uarea.bytes.uabytes,
	    ctob(UAREA_PAGES));
	if (cnt != ctob(UAREA_PAGES))
		errx(1, "read upages structure: %s",
		    cnt > 0 ? strerror(EIO) : strerror(errno));

	cnt = kvm_read(kd, (u_long)ki->ki_kstack, uarea.bytes.ksbytes,
	    ctob(KSTACK_PAGES));
	if (cnt != ctob(KSTACK_PAGES))
		errx(1, "read kstack structure: %s",
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
}

void
datadump(int efd, int fd, struct kinfo_proc *kp, u_long addr, int npage)
{
	int cc, delta;
	char buffer[PAGE_SIZE];

	delta = data_offset - addr;
	while (--npage >= 0) {
		cc = kvm_uread(kd, kp, addr, buffer, PAGE_SIZE);
		if (cc != PAGE_SIZE) {
			/* Try to read the page from the executable. */
			if (lseek(efd, (off_t)addr + delta, SEEK_SET) == -1)
				err(1, "seek executable");
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

void
userdump(int fd, struct kinfo_proc *kp, u_long addr, int npage)
{
	int cc;
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

struct dumpers aoutdump = { aoutident, aoutcore };
TEXT_SET(dumpset, aoutdump);
