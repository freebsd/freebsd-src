/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 *
 * $FreeBSD$
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)kvm_proc.c	8.3 (Berkeley) 9/23/93";
#endif /* LIBC_SCCS and not lint */

/*
 * Proc traversal interface for kvm.  ps and w are (probably) the exclusive
 * users of this code, so we've factored it out into a separate module.
 * Thus, we keep this grunge out of the other kvm applications (i.e.,
 * most other applications are interested only in open/close/read/nlist).
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/swap_pager.h>

#include <sys/sysctl.h>

#include <limits.h>
#include <memory.h>
#include <paths.h>

#include "kvm_private.h"

#if used
static char *
kvm_readswap(kd, p, va, cnt)
	kvm_t *kd;
	const struct proc *p;
	u_long va;
	u_long *cnt;
{
#ifdef __FreeBSD__
	/* XXX Stubbed out, our vm system is differnet */
	_kvm_err(kd, kd->program, "kvm_readswap not implemented");
	return(0);
#endif	/* __FreeBSD__ */
}
#endif

#define KREAD(kd, addr, obj) \
	(kvm_read(kd, addr, (char *)(obj), sizeof(*obj)) != sizeof(*obj))

/*
 * Read proc's from memory file into buffer bp, which has space to hold
 * at most maxcnt procs.
 */
static int
kvm_proclist(kd, what, arg, p, bp, maxcnt)
	kvm_t *kd;
	int what, arg;
	struct proc *p;
	struct kinfo_proc *bp;
	int maxcnt;
{
	register int cnt = 0;
	struct kinfo_proc kinfo_proc, *kp;
	struct pgrp pgrp;
	struct session sess;
	struct tty tty;
	struct vmspace vmspace;
	struct procsig procsig;
	struct pcred pcred;
	struct pstats pstats;
	struct ucred ucred;
	struct proc proc;
	struct proc pproc;

	kp = &kinfo_proc;
	kp->ki_structsize = sizeof(kinfo_proc);
	for (; cnt < maxcnt && p != NULL; p = LIST_NEXT(&proc, p_list)) {
		if (KREAD(kd, (u_long)p, &proc)) {
			_kvm_err(kd, kd->program, "can't read proc at %x", p);
			return (-1);
		}
		if (KREAD(kd, (u_long)proc.p_cred, &pcred) == 0) {
			kp->ki_ruid = pcred.p_ruid;
			kp->ki_svuid = pcred.p_svuid;
			kp->ki_rgid = pcred.p_rgid;
			kp->ki_svgid = pcred.p_svgid;
			(void)(KREAD(kd, (u_long)pcred.pc_ucred, &ucred));
			kp->ki_ngroups = ucred.cr_ngroups;
			bcopy(ucred.cr_groups, kp->ki_groups,
			    NGROUPS * sizeof(gid_t));
			kp->ki_uid = ucred.cr_uid;
		}

		switch(what) {

		case KERN_PROC_PID:
			if (proc.p_pid != (pid_t)arg)
				continue;
			break;

		case KERN_PROC_UID:
			if (kp->ki_uid != (uid_t)arg)
				continue;
			break;

		case KERN_PROC_RUID:
			if (kp->ki_ruid != (uid_t)arg)
				continue;
			break;
		}
		/*
		 * We're going to add another proc to the set.  If this
		 * will overflow the buffer, assume the reason is because
		 * nprocs (or the proc list) is corrupt and declare an error.
		 */
		if (cnt >= maxcnt) {
			_kvm_err(kd, kd->program, "nprocs corrupt");
			return (-1);
		}
		/*
		 * gather kinfo_proc
		 */
		kp->ki_paddr = p;
		kp->ki_addr = proc.p_addr;
		kp->ki_args = proc.p_args;
		kp->ki_tracep = proc.p_tracep;
		kp->ki_textvp = proc.p_textvp;
		kp->ki_fd = proc.p_fd;
		kp->ki_vmspace = proc.p_vmspace;
		if (proc.p_procsig != NULL) {
			if (KREAD(kd, (u_long)proc.p_procsig, &procsig)) {
				_kvm_err(kd, kd->program,
				    "can't read procsig at %x", proc.p_procsig);
				return (-1);
			}
			kp->ki_sigignore = procsig.ps_sigignore;
			kp->ki_sigcatch = procsig.ps_sigcatch;
		}
		if ((proc.p_sflag & PS_INMEM) && proc.p_stats != NULL) {
			if (KREAD(kd, (u_long)proc.p_stats, &pstats)) {
				_kvm_err(kd, kd->program,
				    "can't read stats at %x", proc.p_stats);
				return (-1);
			}
			kp->ki_start = pstats.p_start;
			kp->ki_rusage = pstats.p_ru;
			kp->ki_childtime.tv_sec = pstats.p_cru.ru_utime.tv_sec +
			    pstats.p_cru.ru_stime.tv_sec;
			kp->ki_childtime.tv_usec =
			    pstats.p_cru.ru_utime.tv_usec +
			    pstats.p_cru.ru_stime.tv_usec;
		}
		if (KREAD(kd, (u_long)proc.p_pgrp, &pgrp)) {
			_kvm_err(kd, kd->program, "can't read pgrp at %x",
				 proc.p_pgrp);
			return (-1);
		}
		if (proc.p_oppid)
			kp->ki_ppid = proc.p_oppid;
		else if (proc.p_pptr) {
			if (KREAD(kd, (u_long)proc.p_pptr, &pproc)) {
				_kvm_err(kd, kd->program,
				    "can't read pproc at %x", proc.p_pptr);
				return (-1);
			}
			kp->ki_ppid = pproc.p_pid;
		} else 
			kp->ki_ppid = 0;
		kp->ki_pgid = pgrp.pg_id;
		kp->ki_jobc = pgrp.pg_jobc;
		if (KREAD(kd, (u_long)pgrp.pg_session, &sess)) {
			_kvm_err(kd, kd->program, "can't read session at %x",
				pgrp.pg_session);
			return (-1);
		}
		kp->ki_sid = sess.s_sid;
		(void)memcpy(kp->ki_login, sess.s_login,
						sizeof(kp->ki_login));
		kp->ki_kiflag = sess.s_ttyvp ? KI_CTTY : 0;
		if (sess.s_leader == p)
			kp->ki_kiflag |= KI_SLEADER;
		if ((proc.p_flag & P_CONTROLT) && sess.s_ttyp != NULL) {
			if (KREAD(kd, (u_long)sess.s_ttyp, &tty)) {
				_kvm_err(kd, kd->program,
					 "can't read tty at %x", sess.s_ttyp);
				return (-1);
			}
			kp->ki_tdev = tty.t_dev;
			if (tty.t_pgrp != NULL) {
				if (KREAD(kd, (u_long)tty.t_pgrp, &pgrp)) {
					_kvm_err(kd, kd->program,
						 "can't read tpgrp at &x",
						tty.t_pgrp);
					return (-1);
				}
				kp->ki_tpgid = pgrp.pg_id;
			} else
				kp->ki_tpgid = -1;
			if (tty.t_session != NULL) {
				if (KREAD(kd, (u_long)tty.t_session, &sess)) {
					_kvm_err(kd, kd->program,
					    "can't read session at %x",
					    tty.t_session);
					return (-1);
				}
				kp->ki_tsid = sess.s_sid;
			}
		} else
			kp->ki_tdev = NODEV;
		if (proc.p_wmesg)
			(void)kvm_read(kd, (u_long)proc.p_wmesg,
			    kp->ki_wmesg, WMESGLEN);

#ifdef sparc
		(void)kvm_read(kd, (u_long)&proc.p_vmspace->vm_rssize,
		    (char *)&kp->ki_rssize,
		    sizeof(kp->ki_rssize));
		(void)kvm_read(kd, (u_long)&proc.p_vmspace->vm_tsize,
		    (char *)&kp->ki_tsize,
		    3 * sizeof(kp->ki_rssize));	/* XXX */
#else
		(void)kvm_read(kd, (u_long)proc.p_vmspace,
		    (char *)&vmspace, sizeof(vmspace));
		kp->ki_size = vmspace.vm_map.size;
		kp->ki_rssize = vmspace.vm_swrss; /* XXX */
		kp->ki_swrss = vmspace.vm_swrss;
		kp->ki_tsize = vmspace.vm_tsize;
		kp->ki_dsize = vmspace.vm_dsize;
		kp->ki_ssize = vmspace.vm_ssize;
#endif

		switch (what) {

		case KERN_PROC_PGRP:
			if (kp->ki_pgid != (pid_t)arg)
				continue;
			break;

		case KERN_PROC_TTY:
			if ((proc.p_flag & P_CONTROLT) == 0 ||
			     kp->ki_tdev != (dev_t)arg)
				continue;
			break;
		}
		if (proc.p_comm[0] != 0) {
			strncpy(kp->ki_comm, proc.p_comm, MAXCOMLEN);
			kp->ki_comm[MAXCOMLEN] = 0;
		}
		if (proc.p_blocked != 0) {
			kp->ki_kiflag |= KI_MTXBLOCK;
			if (proc.p_mtxname)
				(void)kvm_read(kd, (u_long)proc.p_mtxname,
				    kp->ki_mtxname, MTXNAMELEN);
			kp->ki_mtxname[MTXNAMELEN] = 0;
		}
		kp->ki_runtime = proc.p_runtime;
		kp->ki_pid = proc.p_pid;
		kp->ki_siglist = proc.p_siglist;
		kp->ki_sigmask = proc.p_sigmask;
		kp->ki_xstat = proc.p_xstat;
		kp->ki_acflag = proc.p_acflag;
		kp->ki_pctcpu = proc.p_pctcpu;
		kp->ki_estcpu = proc.p_estcpu;
		kp->ki_slptime = proc.p_slptime;
		kp->ki_swtime = proc.p_swtime;
		kp->ki_flag = proc.p_flag;
		kp->ki_sflag = proc.p_sflag;
		kp->ki_wchan = proc.p_wchan;
		kp->ki_traceflag = proc.p_traceflag;
		kp->ki_stat = proc.p_stat;
		kp->ki_pri = proc.p_pri;
		kp->ki_nice = proc.p_nice;
		kp->ki_lock = proc.p_lock;
		kp->ki_rqindex = proc.p_rqindex;
		kp->ki_oncpu = proc.p_oncpu;
		kp->ki_lastcpu = proc.p_lastcpu;
		bcopy(&kinfo_proc, bp, sizeof(kinfo_proc));
		++bp;
		++cnt;
	}
	return (cnt);
}

/*
 * Build proc info array by reading in proc list from a crash dump.
 * Return number of procs read.  maxcnt is the max we will read.
 */
static int
kvm_deadprocs(kd, what, arg, a_allproc, a_zombproc, maxcnt)
	kvm_t *kd;
	int what, arg;
	u_long a_allproc;
	u_long a_zombproc;
	int maxcnt;
{
	register struct kinfo_proc *bp = kd->procbase;
	register int acnt, zcnt;
	struct proc *p;

	if (KREAD(kd, a_allproc, &p)) {
		_kvm_err(kd, kd->program, "cannot read allproc");
		return (-1);
	}
	acnt = kvm_proclist(kd, what, arg, p, bp, maxcnt);
	if (acnt < 0)
		return (acnt);

	if (KREAD(kd, a_zombproc, &p)) {
		_kvm_err(kd, kd->program, "cannot read zombproc");
		return (-1);
	}
	zcnt = kvm_proclist(kd, what, arg, p, bp + acnt, maxcnt - acnt);
	if (zcnt < 0)
		zcnt = 0;

	return (acnt + zcnt);
}

struct kinfo_proc *
kvm_getprocs(kd, op, arg, cnt)
	kvm_t *kd;
	int op, arg;
	int *cnt;
{
	int mib[4], st, nprocs;
	size_t size;

	if (kd->procbase != 0) {
		free((void *)kd->procbase);
		/*
		 * Clear this pointer in case this call fails.  Otherwise,
		 * kvm_close() will free it again.
		 */
		kd->procbase = 0;
	}
	if (ISALIVE(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = op;
		mib[3] = arg;
		st = sysctl(mib, op == KERN_PROC_ALL ? 3 : 4, NULL, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (0);
		}
		do {
			size += size / 10;
			kd->procbase = (struct kinfo_proc *)
			    _kvm_realloc(kd, kd->procbase, size);
			if (kd->procbase == 0)
				return (0);
			st = sysctl(mib, op == KERN_PROC_ALL ? 3 : 4,
			    kd->procbase, &size, NULL, 0);
		} while (st == -1 && errno == ENOMEM);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (0);
		}
		if (size > 0 &&
		    kd->procbase->ki_structsize != sizeof(struct kinfo_proc)) {
			_kvm_err(kd, kd->program,
			    "kinfo_proc size mismatch (expected %d, got %d)",
			    sizeof(struct kinfo_proc),
			    kd->procbase->ki_structsize);
			return (0);
		}
		nprocs = size / kd->procbase->ki_structsize;
	} else {
		struct nlist nl[4], *p;

		nl[0].n_name = "_nprocs";
		nl[1].n_name = "_allproc";
		nl[2].n_name = "_zombproc";
		nl[3].n_name = 0;

		if (kvm_nlist(kd, nl) != 0) {
			for (p = nl; p->n_type != 0; ++p)
				;
			_kvm_err(kd, kd->program,
				 "%s: no such symbol", p->n_name);
			return (0);
		}
		if (KREAD(kd, nl[0].n_value, &nprocs)) {
			_kvm_err(kd, kd->program, "can't read nprocs");
			return (0);
		}
		size = nprocs * sizeof(struct kinfo_proc);
		kd->procbase = (struct kinfo_proc *)_kvm_malloc(kd, size);
		if (kd->procbase == 0)
			return (0);

		nprocs = kvm_deadprocs(kd, op, arg, nl[1].n_value,
				      nl[2].n_value, nprocs);
#ifdef notdef
		size = nprocs * sizeof(struct kinfo_proc);
		(void)realloc(kd->procbase, size);
#endif
	}
	*cnt = nprocs;
	return (kd->procbase);
}

void
_kvm_freeprocs(kd)
	kvm_t *kd;
{
	if (kd->procbase) {
		free(kd->procbase);
		kd->procbase = 0;
	}
}

void *
_kvm_realloc(kd, p, n)
	kvm_t *kd;
	void *p;
	size_t n;
{
	void *np = (void *)realloc(p, n);

	if (np == 0) {
		free(p);
		_kvm_err(kd, kd->program, "out of memory");
	}
	return (np);
}

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/*
 * Read in an argument vector from the user address space of process kp.
 * addr if the user-space base address of narg null-terminated contiguous
 * strings.  This is used to read in both the command arguments and
 * environment strings.  Read at most maxcnt characters of strings.
 */
static char **
kvm_argv(kd, kp, addr, narg, maxcnt)
	kvm_t *kd;
	struct kinfo_proc *kp;
	register u_long addr;
	register int narg;
	register int maxcnt;
{
	register char *np, *cp, *ep, *ap;
	register u_long oaddr = -1;
	register int len, cc;
	register char **argv;

	/*
	 * Check that there aren't an unreasonable number of agruments,
	 * and that the address is in user space.
	 */
	if (narg > 512 || addr < VM_MIN_ADDRESS || addr >= VM_MAXUSER_ADDRESS)
		return (0);

	/*
	 * kd->argv : work space for fetching the strings from the target 
	 *            process's space, and is converted for returning to caller
	 */
	if (kd->argv == 0) {
		/*
		 * Try to avoid reallocs.
		 */
		kd->argc = MAX(narg + 1, 32);
		kd->argv = (char **)_kvm_malloc(kd, kd->argc *
						sizeof(*kd->argv));
		if (kd->argv == 0)
			return (0);
	} else if (narg + 1 > kd->argc) {
		kd->argc = MAX(2 * kd->argc, narg + 1);
		kd->argv = (char **)_kvm_realloc(kd, kd->argv, kd->argc *
						sizeof(*kd->argv));
		if (kd->argv == 0)
			return (0);
	}
	/*
	 * kd->argspc : returned to user, this is where the kd->argv
	 *              arrays are left pointing to the collected strings.
	 */
	if (kd->argspc == 0) {
		kd->argspc = (char *)_kvm_malloc(kd, PAGE_SIZE);
		if (kd->argspc == 0)
			return (0);
		kd->arglen = PAGE_SIZE;
	}
	/*
	 * kd->argbuf : used to pull in pages from the target process.
	 *              the strings are copied out of here.
	 */
	if (kd->argbuf == 0) {
		kd->argbuf = (char *)_kvm_malloc(kd, PAGE_SIZE);
		if (kd->argbuf == 0)
			return (0);
	}

	/* Pull in the target process'es argv vector */
	cc = sizeof(char *) * narg;
	if (kvm_uread(kd, kp, addr, (char *)kd->argv, cc) != cc)
		return (0);
	/*
	 * ap : saved start address of string we're working on in kd->argspc
	 * np : pointer to next place to write in kd->argspc
	 * len: length of data in kd->argspc
	 * argv: pointer to the argv vector that we are hunting around the
	 *       target process space for, and converting to addresses in
	 *       our address space (kd->argspc).
	 */
	ap = np = kd->argspc;
	argv = kd->argv;
	len = 0;
	/*
	 * Loop over pages, filling in the argument vector.
	 * Note that the argv strings could be pointing *anywhere* in
	 * the user address space and are no longer contiguous.
	 * Note that *argv is modified when we are going to fetch a string
	 * that crosses a page boundary.  We copy the next part of the string
	 * into to "np" and eventually convert the pointer.
	 */
	while (argv < kd->argv + narg && *argv != 0) {

		/* get the address that the current argv string is on */
		addr = (u_long)*argv & ~(PAGE_SIZE - 1);

		/* is it the same page as the last one? */
		if (addr != oaddr) {
			if (kvm_uread(kd, kp, addr, kd->argbuf, PAGE_SIZE) !=
			    PAGE_SIZE)
				return (0);
			oaddr = addr;
		}

		/* offset within the page... kd->argbuf */
		addr = (u_long)*argv & (PAGE_SIZE - 1);

		/* cp = start of string, cc = count of chars in this chunk */
		cp = kd->argbuf + addr;
		cc = PAGE_SIZE - addr;

		/* dont get more than asked for by user process */
		if (maxcnt > 0 && cc > maxcnt - len)
			cc = maxcnt - len;

		/* pointer to end of string if we found it in this page */
		ep = memchr(cp, '\0', cc);
		if (ep != 0)
			cc = ep - cp + 1;
		/*
		 * at this point, cc is the count of the chars that we are
		 * going to retrieve this time. we may or may not have found
		 * the end of it.  (ep points to the null if the end is known)
		 */

		/* will we exceed the malloc/realloced buffer? */
		if (len + cc > kd->arglen) {
			register int off;
			register char **pp;
			register char *op = kd->argspc;

			kd->arglen *= 2;
			kd->argspc = (char *)_kvm_realloc(kd, kd->argspc,
							  kd->arglen);
			if (kd->argspc == 0)
				return (0);
			/*
			 * Adjust argv pointers in case realloc moved
			 * the string space.
			 */
			off = kd->argspc - op;
			for (pp = kd->argv; pp < argv; pp++)
				*pp += off;
			ap += off;
			np += off;
		}
		/* np = where to put the next part of the string in kd->argspc*/
		/* np is kinda redundant.. could use "kd->argspc + len" */
		memcpy(np, cp, cc);
		np += cc;	/* inc counters */
		len += cc;

		/*
		 * if end of string found, set the *argv pointer to the
		 * saved beginning of string, and advance. argv points to
		 * somewhere in kd->argv..  This is initially relative
		 * to the target process, but when we close it off, we set
		 * it to point in our address space.
		 */
		if (ep != 0) {
			*argv++ = ap;
			ap = np;
		} else {
			/* update the address relative to the target process */
			*argv += cc;
		}

		if (maxcnt > 0 && len >= maxcnt) {
			/*
			 * We're stopping prematurely.  Terminate the
			 * current string.
			 */
			if (ep == 0) {
				*np = '\0';
				*argv++ = ap;
			}
			break;
		}
	}
	/* Make sure argv is terminated. */
	*argv = 0;
	return (kd->argv);
}

static void
ps_str_a(p, addr, n)
	struct ps_strings *p;
	u_long *addr;
	int *n;
{
	*addr = (u_long)p->ps_argvstr;
	*n = p->ps_nargvstr;
}

static void
ps_str_e(p, addr, n)
	struct ps_strings *p;
	u_long *addr;
	int *n;
{
	*addr = (u_long)p->ps_envstr;
	*n = p->ps_nenvstr;
}

/*
 * Determine if the proc indicated by p is still active.
 * This test is not 100% foolproof in theory, but chances of
 * being wrong are very low.
 */
static int
proc_verify(curkp)
	struct kinfo_proc *curkp;
{
	struct kinfo_proc newkp;
	int mib[4];
	size_t len;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = curkp->ki_pid;
	len = sizeof(newkp);
	if (sysctl(mib, 4, &newkp, &len, NULL, 0) == -1)
		return (0);
	return (curkp->ki_pid == newkp.ki_pid &&
	    (newkp.ki_stat != SZOMB || curkp->ki_stat == SZOMB));
}

static char **
kvm_doargv(kd, kp, nchr, info)
	kvm_t *kd;
	struct kinfo_proc *kp;
	int nchr;
	void (*info)(struct ps_strings *, u_long *, int *);
{
	char **ap;
	u_long addr;
	int cnt;
	static struct ps_strings arginfo;
	static u_long ps_strings;
	size_t len;

	if (ps_strings == NULL) {
		len = sizeof(ps_strings);
		if (sysctlbyname("kern.ps_strings", &ps_strings, &len, NULL,
		    0) == -1)
			ps_strings = PS_STRINGS;
	}

	/*
	 * Pointers are stored at the top of the user stack.
	 */
	if (kp->ki_stat == SZOMB ||
	    kvm_uread(kd, kp, ps_strings, (char *)&arginfo,
		      sizeof(arginfo)) != sizeof(arginfo))
		return (0);

	(*info)(&arginfo, &addr, &cnt);
	if (cnt == 0)
		return (0);
	ap = kvm_argv(kd, kp, addr, cnt, nchr);
	/*
	 * For live kernels, make sure this process didn't go away.
	 */
	if (ap != 0 && ISALIVE(kd) && !proc_verify(kp))
		ap = 0;
	return (ap);
}

/*
 * Get the command args.  This code is now machine independent.
 */
char **
kvm_getargv(kd, kp, nchr)
	kvm_t *kd;
	const struct kinfo_proc *kp;
	int nchr;
{
	int oid[4];
	int i;
	size_t bufsz;
	static int buflen;
	static char *buf, *p;
	static char **bufp;
	static int argc;

	if (!ISALIVE(kd)) {
		_kvm_err(kd, kd->program,
		    "cannot read user space from dead kernel");
		return (0);
	}

	if (!buflen) {
		bufsz = sizeof(buflen);
		i = sysctlbyname("kern.ps_arg_cache_limit", 
		    &buflen, &bufsz, NULL, 0);
		if (i == -1) {
			buflen = 0;
		} else {
			buf = malloc(buflen);
			if (buf == NULL)
				buflen = 0;
			argc = 32;
			bufp = malloc(sizeof(char *) * argc);
		}
	}
	if (buf != NULL) {
		oid[0] = CTL_KERN;
		oid[1] = KERN_PROC;
		oid[2] = KERN_PROC_ARGS;
		oid[3] = kp->ki_pid;
		bufsz = buflen;
		i = sysctl(oid, 4, buf, &bufsz, 0, 0);
		if (i == 0 && bufsz > 0) {
			i = 0;
			p = buf;
			do {
				bufp[i++] = p;
				p += strlen(p) + 1;
				if (i >= argc) {
					argc += argc;
					bufp = realloc(bufp,
					    sizeof(char *) * argc);
				}
			} while (p < buf + bufsz);
			bufp[i++] = 0;
			return (bufp);
		}
	}
	if (kp->ki_flag & P_SYSTEM)
		return (NULL);
	return (kvm_doargv(kd, kp, nchr, ps_str_a));
}

char **
kvm_getenvv(kd, kp, nchr)
	kvm_t *kd;
	const struct kinfo_proc *kp;
	int nchr;
{
	return (kvm_doargv(kd, kp, nchr, ps_str_e));
}

/*
 * Read from user space.  The user context is given by p.
 */
ssize_t
kvm_uread(kd, kp, uva, buf, len)
	kvm_t *kd;
	struct kinfo_proc *kp;
	register u_long uva;
	register char *buf;
	register size_t len;
{
	register char *cp;
	char procfile[MAXPATHLEN];
	ssize_t amount;
	int fd;

	if (!ISALIVE(kd)) {
		_kvm_err(kd, kd->program,
		    "cannot read user space from dead kernel");
		return (0);
	}

	sprintf(procfile, "/proc/%d/mem", kp->ki_pid);
	fd = open(procfile, O_RDONLY, 0);
	if (fd < 0) {
		_kvm_err(kd, kd->program, "cannot open %s", procfile);
		close(fd);
		return (0);
	}

	cp = buf;
	while (len > 0) {
		errno = 0;
		if (lseek(fd, (off_t)uva, 0) == -1 && errno != 0) {
			_kvm_err(kd, kd->program, "invalid address (%x) in %s",
			    uva, procfile);
			break;
		}
		amount = read(fd, cp, len);
		if (amount < 0) {
			_kvm_syserr(kd, kd->program, "error reading %s",
			    procfile);
			break;
		}
		if (amount == 0) {
			_kvm_err(kd, kd->program, "EOF reading %s", procfile);
			break;
		}
		cp += amount;
		uva += amount;
		len -= amount;
	}

	close(fd);
	return ((ssize_t)(cp - buf));
}
