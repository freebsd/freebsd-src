/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
static char sccsid[] = "@(#)md-sparc.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <machine/vmparam.h>

#include <err.h>
#include <kvm.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "extern.h"

#ifndef offsetof
#define	offsetof(s, f) ((int)&((s *)0)->f)
#endif

static void
shift_page(fd, off, ssize)
	register int fd;
	register off_t off;
	register int ssize;
{
	char buffer[NBPG];

	(void)lseek(fd, (off_t)-NBPG, SEEK_END);
	for (; ssize > 0; ssize -= NBPG) {
		(void)read(fd, buffer, NBPG);
		(void)write(fd, buffer, NBPG);
		(void)lseek(fd, (off_t)-2 * NBPG, SEEK_CUR);
	}
}

/*
 * Fix up the core image for the sparc.  We need to flush any register
 * windows that are cached in the pcb out to the user stack.
 * Also, we need to get the trap frame and possible floating point state
 * from the top of the kernel stack and store it in the pcb.
 */
void
md_core(kd, fd, ki)
	kvm_t *kd;
	int fd;
	struct kinfo_proc *ki;
{
	register struct rwindow *rw;
	register int nsaved, cc, ssize;
	register off_t off, s;
	register u_long sp;
	struct pcb pcb;
	struct trapframe tf;

	/*
	 * Before anything else read the trapframe.  Synchronizing here
	 * is impossible if the process is running.
	 */
	cc = kvm_read(kd, (u_long)ki->kp_proc.p_md.md_tf,
		      /* XXX */
		      (void *)&tf, sizeof(tf));
	if (cc < 0)
		errx(1, "kvm_read: %s (reading kernel trapframe)",
		      kvm_geterr(kd));
	if (cc != sizeof(tf))
		errx(1, "cannot read kernel trapframe");

	/*
	 * Write out the real trap frame.
	 */
	off = offsetof(struct user, u_md);
	off += offsetof(struct md_coredump, md_tf);
	if (lseek(fd, off, SEEK_SET) == -1)
		err(1, "lseek");
	(void)write(fd, &tf, sizeof(tf));

	if (ki->kp_proc.p_md.md_fpstate != 0) {
		/*
		 * If floating point state is present, write it out too.
		 * It comes right after the trapframe so we don't need to seek.
		 */
		struct fpstate fs;
		cc = kvm_read(kd, (u_long)ki->kp_proc.p_md.md_fpstate,
			      (void *)&fs, sizeof(fs));
		if (cc < 0)
			errx(1, "kvm_read: %s (fpu state)", kvm_geterr(kd));
		if (cc != sizeof(fs))
			errx(1, "cannot read fpu state");
		(void)write(fd, (char *)&fs, sizeof(fs));
	}
	/*
	 * Read pcb.
	 */
	if (lseek(fd, (off_t)offsetof(struct user, u_pcb), SEEK_SET) == -1)
		err(1, "lseek");
	cc = read(fd, (char *)&pcb, sizeof(pcb));
	if (cc != sizeof(pcb)) {
		if (cc < 0)
			err(1, "read");
		errx(1, "couldn't read pcb from core file");
	}

	/*
	 * Write any unsaved windows to the appropriate stack locations.
	 */
	nsaved = pcb.pcb_nsaved;
	if (nsaved == 0)
		return;

	rw = &pcb.pcb_rw[0];
	off = ctob(UPAGES + ki->kp_eproc.e_vm.vm_dsize);
	ssize = ctob(ki->kp_eproc.e_vm.vm_ssize);
	sp = tf.tf_out[6];
	for (; --nsaved >= 0; ++rw) {
		/*
		 * Copy register window into appropriate stack location.
		 */
		s = ssize - (USRSTACK - sp);
		if (s < 0) {
			if (s < -NBPG)
				errx(1, "cannot copy pcb windows to stack");
			/*
			 * It's possible to be missing the bottomost
			 * page because a stack page hasn't been allocated
			 * for the register save area.  Shift over
			 * the stack segment by a page, and update
			 * the u-area to reflect the new stack size.  YECH!
			 */
			shift_page(fd, off, ssize);
			ssize += NBPG;
			s += NBPG;
			++ki->kp_eproc.e_vm.vm_ssize;
			(void)lseek(fd,
			    (off_t)offsetof(struct user, u_kproc.kp_eproc.e_vm),
			    SEEK_SET);
			(void)write(fd,
			    &ki->kp_eproc.e_vm, sizeof(ki->kp_eproc.e_vm));
		}
		if (lseek(fd, off + s, SEEK_SET) == -1)
			errx(1, "cannot copy pcb windows to stack");

		(void)write(fd, rw, sizeof(*rw));
		sp = rw->rw_in[6];
	}
}
