/*-
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/imgact_aout.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/reboot.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/frame.h>
#include <machine/limits.h>
#include <machine/psl.h>
#include <machine/sysarch.h>
#ifdef __i386__
#include <machine/segments.h>
#endif

#include <posix4/sched.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>

#ifdef __alpha__
#define BSD_TO_LINUX_SIGNAL(sig)       (sig)
#else
#define BSD_TO_LINUX_SIGNAL(sig)	\
	(((sig) <= LINUX_SIGTBLSZ) ? bsd_to_linux_signal[_SIG_IDX(sig)] : sig)
#endif

struct linux_rlimit {
	unsigned long rlim_cur;
	unsigned long rlim_max;
};

#ifndef __alpha__
static unsigned int linux_to_bsd_resource[LINUX_RLIM_NLIMITS] =
{ RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_DATA, RLIMIT_STACK,
  RLIMIT_CORE, RLIMIT_RSS, RLIMIT_NPROC, RLIMIT_NOFILE,
  RLIMIT_MEMLOCK, -1
};
#endif /*!__alpha__*/

#ifndef __alpha__
int
linux_alarm(struct proc *p, struct linux_alarm_args *args)
{
    struct itimerval it, old_it;
    struct timeval tv;
    int s;

#ifdef DEBUG
	if (ldebug(alarm))
		printf(ARGS(alarm, "%u"), args->secs);
#endif
    if (args->secs > 100000000)
	return EINVAL;
    it.it_value.tv_sec = (long)args->secs;
    it.it_value.tv_usec = 0;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    s = splsoftclock();
    old_it = p->p_realtimer;
    getmicrouptime(&tv);
    if (timevalisset(&old_it.it_value))
	callout_stop(&p->p_itcallout);
    if (it.it_value.tv_sec != 0) {
	callout_reset(&p->p_itcallout, tvtohz(&it.it_value), realitexpire, p);
	timevaladd(&it.it_value, &tv);
    }
    p->p_realtimer = it;
    splx(s);
    if (timevalcmp(&old_it.it_value, &tv, >)) {
	timevalsub(&old_it.it_value, &tv);
	if (old_it.it_value.tv_usec != 0)
	    old_it.it_value.tv_sec++;
	p->p_retval[0] = old_it.it_value.tv_sec;
    }
    return 0;
}
#endif /*!__alpha__*/

int
linux_brk(struct proc *p, struct linux_brk_args *args)
{
#if 0
    struct vmspace *vm = p->p_vmspace;
    vm_offset_t new, old;
    int error;

    if ((vm_offset_t)args->dsend < (vm_offset_t)vm->vm_daddr)
	return EINVAL;
    if (((caddr_t)args->dsend - (caddr_t)vm->vm_daddr)
	> p->p_rlimit[RLIMIT_DATA].rlim_cur)
	return ENOMEM;

    old = round_page((vm_offset_t)vm->vm_daddr) + ctob(vm->vm_dsize);
    new = round_page((vm_offset_t)args->dsend);
    p->p_retval[0] = old;
    if ((new-old) > 0) {
	if (swap_pager_full)
	    return ENOMEM;
	error = vm_map_find(&vm->vm_map, NULL, 0, &old, (new-old), FALSE,
			VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error)
	    return error;
	vm->vm_dsize += btoc((new-old));
	p->p_retval[0] = (int)(vm->vm_daddr + ctob(vm->vm_dsize));
    }
    return 0;
#else
    struct vmspace *vm = p->p_vmspace;
    vm_offset_t new, old;
    struct obreak_args /* {
	char * nsize;
    } */ tmp;

#ifdef DEBUG
	if (ldebug(brk))
		printf(ARGS(brk, "%p"), (void *)args->dsend);
#endif
    old = (vm_offset_t)vm->vm_daddr + ctob(vm->vm_dsize);
    new = (vm_offset_t)args->dsend;
    tmp.nsize = (char *) new;
    if (((caddr_t)new > vm->vm_daddr) && !obreak(p, &tmp))
	p->p_retval[0] = (long)new;
    else
	p->p_retval[0] = (long)old;

    return 0;
#endif
}

int
linux_uselib(struct proc *p, struct linux_uselib_args *args)
{
    struct nameidata ni;
    struct vnode *vp;
    struct exec *a_out;
    struct vattr attr;
    vm_offset_t vmaddr;
    unsigned long file_offset;
    vm_offset_t buffer;
    unsigned long bss_size;
    int error;
    caddr_t sg;
    int locked;

    sg = stackgap_init();
    CHECKALTEXIST(p, &sg, args->library);

#ifdef DEBUG
	if (ldebug(uselib))
		printf(ARGS(uselib, "%s"), args->library);
#endif

    a_out = NULL;
    locked = 0;
    vp = NULL;

    NDINIT(&ni, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, args->library, p);
    error = namei(&ni);
    if (error)
	goto cleanup;

    vp = ni.ni_vp;
    /*
     * XXX This looks like a bogus check - a LOCKLEAF namei should not succeed
     * without returning a vnode.
     */
    if (vp == NULL) {
	error = ENOEXEC;	/* ?? */
	goto cleanup;
    }
    NDFREE(&ni, NDF_ONLY_PNBUF);

    /*
     * From here on down, we have a locked vnode that must be unlocked.
     */
    locked++;

    /*
     * Writable?
     */
    if (vp->v_writecount) {
	error = ETXTBSY;
	goto cleanup;
    }

    /*
     * Executable?
     */
    error = VOP_GETATTR(vp, &attr, p->p_ucred, p);
    if (error)
	goto cleanup;

    if ((vp->v_mount->mnt_flag & MNT_NOEXEC) ||
	((attr.va_mode & 0111) == 0) ||
	(attr.va_type != VREG)) {
	    error = ENOEXEC;
	    goto cleanup;
    }

    /*
     * Sensible size?
     */
    if (attr.va_size == 0) {
	error = ENOEXEC;
	goto cleanup;
    }

    /*
     * Can we access it?
     */
    error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
    if (error)
	goto cleanup;

    error = VOP_OPEN(vp, FREAD, p->p_ucred, p);
    if (error)
	goto cleanup;

    /*
     * Lock no longer needed
     */
    VOP_UNLOCK(vp, 0, p);
    locked = 0;

    /*
     * Pull in executable header into kernel_map
     */
    error = vm_mmap(kernel_map, (vm_offset_t *)&a_out, PAGE_SIZE,
	    	    VM_PROT_READ, VM_PROT_READ, 0, (caddr_t)vp, 0);
    if (error)
	goto cleanup;

    /*
     * Is it a Linux binary ?
     */
    if (((a_out->a_magic >> 16) & 0xff) != 0x64) {
	error = ENOEXEC;
	goto cleanup;
    }

    /* While we are here, we should REALLY do some more checks */

    /*
     * Set file/virtual offset based on a.out variant.
     */
    switch ((int)(a_out->a_magic & 0xffff)) {
    case 0413:	/* ZMAGIC */
	file_offset = 1024;
	break;
    case 0314:	/* QMAGIC */
	file_offset = 0;
	break;
    default:
	error = ENOEXEC;
	goto cleanup;
    }

    bss_size = round_page(a_out->a_bss);

    /*
     * Check various fields in header for validity/bounds.
     */
    if (a_out->a_text & PAGE_MASK || a_out->a_data & PAGE_MASK) {
	error = ENOEXEC;
	goto cleanup;
    }

    /* text + data can't exceed file size */
    if (a_out->a_data + a_out->a_text > attr.va_size) {
	error = EFAULT;
	goto cleanup;
    }

    /* To protect p->p_rlimit in the if condition. */
    mtx_assert(&Giant, MA_OWNED);

    /*
     * text/data/bss must not exceed limits
     * XXX: this is not complete. it should check current usage PLUS
     * the resources needed by this library.
     */
    if (a_out->a_text > MAXTSIZ ||
	a_out->a_data + bss_size > p->p_rlimit[RLIMIT_DATA].rlim_cur) {
	error = ENOMEM;
	goto cleanup;
    }

    /*
     * prevent more writers
     */
    vp->v_flag |= VTEXT;

    /*
     * Check if file_offset page aligned,.
     * Currently we cannot handle misalinged file offsets,
     * and so we read in the entire image (what a waste).
     */
    if (file_offset & PAGE_MASK) {
#ifdef DEBUG
printf("uselib: Non page aligned binary %lu\n", file_offset);
#endif
	/*
	 * Map text+data read/write/execute
	 */

	/* a_entry is the load address and is page aligned */
	vmaddr = trunc_page(a_out->a_entry);

	/* get anon user mapping, read+write+execute */
	error = vm_map_find(&p->p_vmspace->vm_map, NULL, 0, &vmaddr,
		    	    a_out->a_text + a_out->a_data, FALSE,
			    VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error)
	    goto cleanup;

	/* map file into kernel_map */
	error = vm_mmap(kernel_map, &buffer,
			round_page(a_out->a_text + a_out->a_data + file_offset),
		   	VM_PROT_READ, VM_PROT_READ, 0,
			(caddr_t)vp, trunc_page(file_offset));
	if (error)
	    goto cleanup;

	/* copy from kernel VM space to user space */
	error = copyout((caddr_t)(void *)(uintptr_t)(buffer + file_offset),
			(caddr_t)vmaddr, a_out->a_text + a_out->a_data);

	/* release temporary kernel space */
	vm_map_remove(kernel_map, buffer,
		      buffer + round_page(a_out->a_text + a_out->a_data + file_offset));

	if (error)
	    goto cleanup;
    }
    else {
#ifdef DEBUG
printf("uselib: Page aligned binary %lu\n", file_offset);
#endif
	/*
	 * for QMAGIC, a_entry is 20 bytes beyond the load address
	 * to skip the executable header
	 */
	vmaddr = trunc_page(a_out->a_entry);

	/*
	 * Map it all into the process's space as a single copy-on-write
	 * "data" segment.
	 */
	error = vm_mmap(&p->p_vmspace->vm_map, &vmaddr,
		   	a_out->a_text + a_out->a_data,
			VM_PROT_ALL, VM_PROT_ALL, MAP_PRIVATE | MAP_FIXED,
			(caddr_t)vp, file_offset);
	if (error)
	    goto cleanup;
    }
#ifdef DEBUG
printf("mem=%08lx = %08lx %08lx\n", vmaddr, ((long*)vmaddr)[0], ((long*)vmaddr)[1]);
#endif
    if (bss_size != 0) {
        /*
	 * Calculate BSS start address
	 */
	vmaddr = trunc_page(a_out->a_entry) + a_out->a_text + a_out->a_data;

	/*
	 * allocate some 'anon' space
	 */
	error = vm_map_find(&p->p_vmspace->vm_map, NULL, 0, &vmaddr,
			    bss_size, FALSE,
			    VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error)
	    goto cleanup;
    }

cleanup:
    /*
     * Unlock vnode if needed
     */
    if (locked)
	VOP_UNLOCK(vp, 0, p);

    /*
     * Release the kernel mapping.
     */
    if (a_out)
	vm_map_remove(kernel_map, (vm_offset_t)a_out, (vm_offset_t)a_out + PAGE_SIZE);

    return error;
}

int
linux_newselect(struct proc *p, struct linux_newselect_args *args)
{
    struct select_args bsa;
    struct timeval tv0, tv1, utv, *tvp;
    caddr_t sg;
    int error;

#ifdef DEBUG
	if (ldebug(newselect))
		printf(ARGS(newselect, "%d, %p, %p, %p, %p"),
		    args->nfds, (void *)args->readfds,
		    (void *)args->writefds, (void *)args->exceptfds,
		    (void *)args->timeout);
#endif
    error = 0;
    bsa.nd = args->nfds;
    bsa.in = args->readfds;
    bsa.ou = args->writefds;
    bsa.ex = args->exceptfds;
    bsa.tv = args->timeout;

    /*
     * Store current time for computation of the amount of
     * time left.
     */
    if (args->timeout) {
	if ((error = copyin(args->timeout, &utv, sizeof(utv))))
	    goto select_out;
#ifdef DEBUG
	if (ldebug(newselect))
		printf(LMSG("incoming timeout (%ld/%ld)"),
		    utv.tv_sec, utv.tv_usec);
#endif
	if (itimerfix(&utv)) {
	    /*
	     * The timeval was invalid.  Convert it to something
	     * valid that will act as it does under Linux.
	     */
	    sg = stackgap_init();
	    tvp = stackgap_alloc(&sg, sizeof(utv));
	    utv.tv_sec += utv.tv_usec / 1000000;
	    utv.tv_usec %= 1000000;
	    if (utv.tv_usec < 0) {
		utv.tv_sec -= 1;
		utv.tv_usec += 1000000;
	    }
	    if (utv.tv_sec < 0)
		timevalclear(&utv);
	    if ((error = copyout(&utv, tvp, sizeof(utv))))
		goto select_out;
	    bsa.tv = tvp;
	}
	microtime(&tv0);
    }

    error = select(p, &bsa);
#ifdef DEBUG
	if (ldebug(newselect))
		printf(LMSG("real select returns %d"), error);
#endif

    if (error) {
	/*
	 * See fs/select.c in the Linux kernel.  Without this,
	 * Maelstrom doesn't work.
	 */
	if (error == ERESTART)
	    error = EINTR;
	goto select_out;
    }

    if (args->timeout) {
	if (p->p_retval[0]) {
	    /*
	     * Compute how much time was left of the timeout,
	     * by subtracting the current time and the time
	     * before we started the call, and subtracting
	     * that result from the user-supplied value.
	     */
	    microtime(&tv1);
	    timevalsub(&tv1, &tv0);
	    timevalsub(&utv, &tv1);
	    if (utv.tv_sec < 0)
		timevalclear(&utv);
	} else
	    timevalclear(&utv);
#ifdef DEBUG
	if (ldebug(newselect))
		printf(LMSG("outgoing timeout (%ld/%ld)"),
		    utv.tv_sec, utv.tv_usec);
#endif
	if ((error = copyout(&utv, args->timeout, sizeof(utv))))
	    goto select_out;
    }

select_out:
#ifdef DEBUG
	if (ldebug(newselect))
		printf(LMSG("newselect_out -> %d"), error);
#endif
    return error;
}

int
linux_getpgid(struct proc *p, struct linux_getpgid_args *args)
{
    struct proc *curp;

#ifdef DEBUG
	if (ldebug(getpgid))
		printf(ARGS(getpgid, "%d"), args->pid);
#endif
    if (args->pid != p->p_pid) {
	if (!(curp = pfind(args->pid)))
	    return ESRCH;
	p->p_retval[0] = curp->p_pgid;
	PROC_UNLOCK(curp);
    }
    else
	p->p_retval[0] = p->p_pgid;
    return 0;
}

int     
linux_mremap(struct proc *p, struct linux_mremap_args *args)
{
	struct munmap_args /* {
		void *addr;
		size_t len;
	} */ bsd_args; 
	int error = 0;
 
#ifdef DEBUG
	if (ldebug(mremap))
		printf(ARGS(mremap, "%p, %08lx, %08lx, %08lx"),
		    (void *)args->addr, 
		    (unsigned long)args->old_len, 
		    (unsigned long)args->new_len,
		    (unsigned long)args->flags);
#endif
	args->new_len = round_page(args->new_len);
	args->old_len = round_page(args->old_len);

	if (args->new_len > args->old_len) {
		p->p_retval[0] = 0;
		return ENOMEM;
	}

	if (args->new_len < args->old_len) {
		bsd_args.addr = args->addr + args->new_len;
		bsd_args.len = args->old_len - args->new_len;
		error = munmap(p, &bsd_args);
	}

	p->p_retval[0] = error ? 0 : (u_long)args->addr;
	return error;
}

int
linux_msync(struct proc *p, struct linux_msync_args *args)
{
	struct msync_args bsd_args;

	bsd_args.addr = args->addr;
	bsd_args.len = args->len;
	bsd_args.flags = 0;	/* XXX ignore */

	return msync(p, &bsd_args);
}

#ifndef __alpha__
int
linux_time(struct proc *p, struct linux_time_args *args)
{
    struct timeval tv;
    linux_time_t tm;
    int error;

#ifdef DEBUG
	if (ldebug(time))
		printf(ARGS(time, "*"));
#endif
    microtime(&tv);
    tm = tv.tv_sec;
    if (args->tm && (error = copyout(&tm, args->tm, sizeof(linux_time_t))))
	return error;
    p->p_retval[0] = tm;
    return 0;
}
#endif	/*!__alpha__*/

struct linux_times_argv {
    long    tms_utime;
    long    tms_stime;
    long    tms_cutime;
    long    tms_cstime;
};

#ifdef __alpha__
#define CLK_TCK 1024	/* Linux uses 1024 on alpha */
#else
#define CLK_TCK 100	/* Linux uses 100 */
#endif

#define CONVTCK(r)	(r.tv_sec * CLK_TCK + r.tv_usec / (1000000 / CLK_TCK))

int
linux_times(struct proc *p, struct linux_times_args *args)
{
    struct timeval tv;
    struct linux_times_argv tms;
    struct rusage ru;
    int error;

#ifdef DEBUG
	if (ldebug(times))
		printf(ARGS(times, "*"));
#endif
    mtx_lock_spin(&sched_lock);
    calcru(p, &ru.ru_utime, &ru.ru_stime, NULL);
    mtx_unlock_spin(&sched_lock);

    tms.tms_utime = CONVTCK(ru.ru_utime);
    tms.tms_stime = CONVTCK(ru.ru_stime);

    tms.tms_cutime = CONVTCK(p->p_stats->p_cru.ru_utime);
    tms.tms_cstime = CONVTCK(p->p_stats->p_cru.ru_stime);

    if ((error = copyout((caddr_t)&tms, (caddr_t)args->buf,
	    	    sizeof(struct linux_times_argv))))
	return error;

    microuptime(&tv);
    p->p_retval[0] = (int)CONVTCK(tv);
    return 0;
}

int
linux_newuname(struct proc *p, struct linux_newuname_args *args)
{
	struct linux_new_utsname utsname;
	char *osrelease, *osname;

#ifdef DEBUG
	if (ldebug(newuname))
		printf(ARGS(newuname, "*"));
#endif

	osname = linux_get_osname(p);
	osrelease = linux_get_osrelease(p);

	bzero(&utsname, sizeof(struct linux_new_utsname));
	strncpy(utsname.sysname, osname, LINUX_MAX_UTSNAME-1);
	strncpy(utsname.nodename, hostname, LINUX_MAX_UTSNAME-1);
	strncpy(utsname.release, osrelease, LINUX_MAX_UTSNAME-1);
	strncpy(utsname.version, version, LINUX_MAX_UTSNAME-1);
	strncpy(utsname.machine, machine, LINUX_MAX_UTSNAME-1);
	strncpy(utsname.domainname, domainname, LINUX_MAX_UTSNAME-1);

	return (copyout((caddr_t)&utsname, (caddr_t)args->buf,
			sizeof(struct linux_new_utsname)));
}

struct linux_utimbuf {
	linux_time_t l_actime;
	linux_time_t l_modtime;
};

int
linux_utime(struct proc *p, struct linux_utime_args *args)
{
    struct utimes_args /* {
	char	*path;
	struct	timeval *tptr;
    } */ bsdutimes;
    struct timeval tv[2], *tvp;
    struct linux_utimbuf lut;
    int error;
    caddr_t sg;

    sg = stackgap_init();
    CHECKALTEXIST(p, &sg, args->fname);

#ifdef DEBUG
	if (ldebug(utime))
		printf(ARGS(utime, "%s, *"), args->fname);
#endif
    if (args->times) {
	if ((error = copyin(args->times, &lut, sizeof lut)))
	    return error;
	tv[0].tv_sec = lut.l_actime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = lut.l_modtime;
	tv[1].tv_usec = 0;
	/* so that utimes can copyin */
	tvp = (struct timeval *)stackgap_alloc(&sg, sizeof(tv));
	if (tvp == NULL)
		return (ENAMETOOLONG);
	if ((error = copyout(tv, tvp, sizeof(tv))))
	    return error;
	bsdutimes.tptr = tvp;
    } else
	bsdutimes.tptr = NULL;

    bsdutimes.path = args->fname;
    return utimes(p, &bsdutimes);
}

#define __WCLONE 0x80000000

#ifndef __alpha__
int
linux_waitpid(struct proc *p, struct linux_waitpid_args *args)
{
    struct wait_args /* {
	int pid;
	int *status;
	int options;
	struct	rusage *rusage;
    } */ tmp;
    int error, tmpstat;

#ifdef DEBUG
	if (ldebug(waitpid))
		printf(ARGS(waitpid, "%d, %p, %d"),
		    args->pid, (void *)args->status, args->options);
#endif
    tmp.pid = args->pid;
    tmp.status = args->status;
    tmp.options = (args->options & (WNOHANG | WUNTRACED));
    /* WLINUXCLONE should be equal to __WCLONE, but we make sure */
    if (args->options & __WCLONE)
	tmp.options |= WLINUXCLONE;
    tmp.rusage = NULL;

    if ((error = wait4(p, &tmp)) != 0)
	return error;

    if (args->status) {
	if ((error = copyin(args->status, &tmpstat, sizeof(int))) != 0)
	    return error;
	tmpstat &= 0xffff;
	if (WIFSIGNALED(tmpstat))
	    tmpstat = (tmpstat & 0xffffff80) |
		      BSD_TO_LINUX_SIGNAL(WTERMSIG(tmpstat));
	else if (WIFSTOPPED(tmpstat))
	    tmpstat = (tmpstat & 0xffff00ff) |
		      (BSD_TO_LINUX_SIGNAL(WSTOPSIG(tmpstat)) << 8);
	return copyout(&tmpstat, args->status, sizeof(int));
    } else
	return 0;
}
#endif	/*!__alpha__*/

int
linux_wait4(struct proc *p, struct linux_wait4_args *args)
{
    struct wait_args /* {
	int pid;
	int *status;
	int options;
	struct	rusage *rusage;
    } */ tmp;
    int error, tmpstat;

#ifdef DEBUG
	if (ldebug(wait4))
		printf(ARGS(wait4, "%d, %p, %d, %p"),
		    args->pid, (void *)args->status, args->options,
		    (void *)args->rusage);
#endif
    tmp.pid = args->pid;
    tmp.status = args->status;
    tmp.options = (args->options & (WNOHANG | WUNTRACED));
    /* WLINUXCLONE should be equal to __WCLONE, but we make sure */
    if (args->options & __WCLONE)
	tmp.options |= WLINUXCLONE;
    tmp.rusage = args->rusage;

    if ((error = wait4(p, &tmp)) != 0)
	return error;

    SIGDELSET(p->p_siglist, SIGCHLD);

    if (args->status) {
	if ((error = copyin(args->status, &tmpstat, sizeof(int))) != 0)
	    return error;
	tmpstat &= 0xffff;
	if (WIFSIGNALED(tmpstat))
	    tmpstat = (tmpstat & 0xffffff80) |
		  BSD_TO_LINUX_SIGNAL(WTERMSIG(tmpstat));
	else if (WIFSTOPPED(tmpstat))
	    tmpstat = (tmpstat & 0xffff00ff) |
		  (BSD_TO_LINUX_SIGNAL(WSTOPSIG(tmpstat)) << 8);
	return copyout(&tmpstat, args->status, sizeof(int));
    } else
	return 0;
}

int
linux_mknod(struct proc *p, struct linux_mknod_args *args)
{
	caddr_t sg;
	struct mknod_args bsd_mknod;
	struct mkfifo_args bsd_mkfifo;

	sg = stackgap_init();

	CHECKALTCREAT(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(mknod))
		printf(ARGS(mknod, "%s, %d, %d"),
		    args->path, args->mode, args->dev);
#endif

	if (args->mode & S_IFIFO) {
		bsd_mkfifo.path = args->path;
		bsd_mkfifo.mode = args->mode;
		return mkfifo(p, &bsd_mkfifo);
	} else {
		bsd_mknod.path = args->path;
		bsd_mknod.mode = args->mode;
		bsd_mknod.dev = args->dev;
		return mknod(p, &bsd_mknod);
	}
}

/*
 * UGH! This is just about the dumbest idea I've ever heard!!
 */
int
linux_personality(struct proc *p, struct linux_personality_args *args)
{
#ifdef DEBUG
	if (ldebug(personality))
		printf(ARGS(personality, "%d"), args->per);
#endif
#ifndef __alpha__
	if (args->per != 0)
		return EINVAL;
#endif

	/* Yes Jim, it's still a Linux... */
	p->p_retval[0] = 0;
	return 0;
}

/*
 * Wrappers for get/setitimer for debugging..
 */
int
linux_setitimer(struct proc *p, struct linux_setitimer_args *args)
{
	struct setitimer_args bsa;
	struct itimerval foo;
	int error;

#ifdef DEBUG
	if (ldebug(setitimer))
		printf(ARGS(setitimer, "%p, %p"),
		    (void *)args->itv, (void *)args->oitv);
#endif
	bsa.which = args->which;
	bsa.itv = args->itv;
	bsa.oitv = args->oitv;
	if (args->itv) {
	    if ((error = copyin((caddr_t)args->itv, (caddr_t)&foo,
			sizeof(foo))))
		return error;
#ifdef DEBUG
	    if (ldebug(setitimer)) {
	        printf("setitimer: value: sec: %ld, usec: %ld\n",
		    foo.it_value.tv_sec, foo.it_value.tv_usec);
	        printf("setitimer: interval: sec: %ld, usec: %ld\n",
		    foo.it_interval.tv_sec, foo.it_interval.tv_usec);
	    }
#endif
	}
	return setitimer(p, &bsa);
}

int
linux_getitimer(struct proc *p, struct linux_getitimer_args *args)
{
	struct getitimer_args bsa;
#ifdef DEBUG
	if (ldebug(getitimer))
		printf(ARGS(getitimer, "%p"), (void *)args->itv);
#endif
	bsa.which = args->which;
	bsa.itv = args->itv;
	return getitimer(p, &bsa);
}

#ifndef __alpha__
int
linux_nice(struct proc *p, struct linux_nice_args *args)
{
	struct setpriority_args	bsd_args;

	bsd_args.which = PRIO_PROCESS;
	bsd_args.who = 0;	/* current process */
	bsd_args.prio = args->inc;
	return setpriority(p, &bsd_args);
}
#endif	/*!__alpha__*/

int
linux_setgroups(p, uap)
	struct proc *p;
	struct linux_setgroups_args *uap;
{
	struct pcred *pc;
	linux_gid_t linux_gidset[NGROUPS];
	gid_t *bsd_gidset;
	int ngrp, error;

	pc = p->p_cred;
	ngrp = uap->gidsetsize;

	/*
	 * cr_groups[0] holds egid. Setting the whole set from
	 * the supplied set will cause egid to be changed too.
	 * Keep cr_groups[0] unchanged to prevent that.
	 */

	if ((error = suser_xxx(NULL, p, PRISON_ROOT)) != 0)
		return (error);

	if (ngrp >= NGROUPS)
		return (EINVAL);

	pc->pc_ucred = crcopy(pc->pc_ucred);
	if (ngrp > 0) {
		error = copyin((caddr_t)uap->gidset, (caddr_t)linux_gidset,
			       ngrp * sizeof(linux_gid_t));
		if (error)
			return (error);

		pc->pc_ucred->cr_ngroups = ngrp + 1;

		bsd_gidset = pc->pc_ucred->cr_groups;
		ngrp--;
		while (ngrp >= 0) {
			bsd_gidset[ngrp + 1] = linux_gidset[ngrp];
			ngrp--;
		}
	}
	else
		pc->pc_ucred->cr_ngroups = 1;

	setsugid(p);
	return (0);
}

int
linux_getgroups(p, uap)
	struct proc *p;
	struct linux_getgroups_args *uap;
{
	struct pcred *pc;
	linux_gid_t linux_gidset[NGROUPS];
	gid_t *bsd_gidset;
	int bsd_gidsetsz, ngrp, error;

	pc = p->p_cred;
	bsd_gidset = pc->pc_ucred->cr_groups;
	bsd_gidsetsz = pc->pc_ucred->cr_ngroups - 1;

	/*
	 * cr_groups[0] holds egid. Returning the whole set
	 * here will cause a duplicate. Exclude cr_groups[0]
	 * to prevent that.
	 */

	if ((ngrp = uap->gidsetsize) == 0) {
		p->p_retval[0] = bsd_gidsetsz;
		return (0);
	}

	if (ngrp < bsd_gidsetsz)
		return (EINVAL);

	ngrp = 0;
	while (ngrp < bsd_gidsetsz) {
		linux_gidset[ngrp] = bsd_gidset[ngrp + 1];
		ngrp++;
	}

	if ((error = copyout((caddr_t)linux_gidset, (caddr_t)uap->gidset,
	    ngrp * sizeof(linux_gid_t))))
		return (error);

	p->p_retval[0] = ngrp;
	return (0);
}

#ifndef __alpha__
int
linux_setrlimit(p, uap)
	struct proc *p;
	struct linux_setrlimit_args *uap;
{
	struct __setrlimit_args bsd;
	struct linux_rlimit rlim;
	int error;
	caddr_t sg = stackgap_init();

#ifdef DEBUG
	if (ldebug(setrlimit))
		printf(ARGS(setrlimit, "%d, %p"),
		    uap->resource, (void *)uap->rlim);
#endif

	if (uap->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	bsd.which = linux_to_bsd_resource[uap->resource];
	if (bsd.which == -1)
		return (EINVAL);

	error = copyin(uap->rlim, &rlim, sizeof(rlim));
	if (error)
		return (error);

	bsd.rlp = stackgap_alloc(&sg, sizeof(struct rlimit));
	bsd.rlp->rlim_cur = (rlim_t)rlim.rlim_cur;
	bsd.rlp->rlim_max = (rlim_t)rlim.rlim_max;
	return (setrlimit(p, &bsd));
}

int
linux_getrlimit(p, uap)
	struct proc *p;
	struct linux_getrlimit_args *uap;
{
	struct __getrlimit_args bsd;
	struct linux_rlimit rlim;
	int error;
	caddr_t sg = stackgap_init();

#ifdef DEBUG
	if (ldebug(getrlimit))
		printf(ARGS(getrlimit, "%d, %p"),
		    uap->resource, (void *)uap->rlim);
#endif

	if (uap->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	bsd.which = linux_to_bsd_resource[uap->resource];
	if (bsd.which == -1)
		return (EINVAL);

	bsd.rlp = stackgap_alloc(&sg, sizeof(struct rlimit));
	error = getrlimit(p, &bsd);
	if (error)
		return (error);

	rlim.rlim_cur = (unsigned long)bsd.rlp->rlim_cur;
	if (rlim.rlim_cur == ULONG_MAX)
		rlim.rlim_cur = LONG_MAX;
	rlim.rlim_max = (unsigned long)bsd.rlp->rlim_max;
	if (rlim.rlim_max == ULONG_MAX)
		rlim.rlim_max = LONG_MAX;
	return (copyout(&rlim, uap->rlim, sizeof(rlim)));
}
#endif /*!__alpha__*/

int
linux_sched_setscheduler(p, uap)
	struct proc *p;
	struct linux_sched_setscheduler_args *uap;
{
	struct sched_setscheduler_args bsd;

#ifdef DEBUG
	if (ldebug(sched_setscheduler))
		printf(ARGS(sched_setscheduler, "%d, %d, %p"),
		    uap->pid, uap->policy, (const void *)uap->param);
#endif

	switch (uap->policy) {
	case LINUX_SCHED_OTHER:
		bsd.policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		bsd.policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		bsd.policy = SCHED_RR;
		break;
	default:
		return EINVAL;
	}

	bsd.pid = uap->pid;
	bsd.param = uap->param;
	return sched_setscheduler(p, &bsd);
}

int
linux_sched_getscheduler(p, uap)
	struct proc *p;
	struct linux_sched_getscheduler_args *uap;
{
	struct sched_getscheduler_args bsd;
	int error;

#ifdef DEBUG
	if (ldebug(sched_getscheduler))
		printf(ARGS(sched_getscheduler, "%d"), uap->pid);
#endif

	bsd.pid = uap->pid;
	error = sched_getscheduler(p, &bsd);

	switch (p->p_retval[0]) {
	case SCHED_OTHER:
		p->p_retval[0] = LINUX_SCHED_OTHER;
		break;
	case SCHED_FIFO:
		p->p_retval[0] = LINUX_SCHED_FIFO;
		break;
	case SCHED_RR:
		p->p_retval[0] = LINUX_SCHED_RR;
		break;
	}

	return error;
}

int
linux_sched_get_priority_max(p, uap)
	struct proc *p;
	struct linux_sched_get_priority_max_args *uap;
{
	struct sched_get_priority_max_args bsd;

#ifdef DEBUG
	if (ldebug(sched_get_priority_max))
		printf(ARGS(sched_get_priority_max, "%d"), uap->policy);
#endif

	switch (uap->policy) {
	case LINUX_SCHED_OTHER:
		bsd.policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		bsd.policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		bsd.policy = SCHED_RR;
		break;
	default:
		return EINVAL;
	}
	return sched_get_priority_max(p, &bsd);
}

int
linux_sched_get_priority_min(p, uap)
	struct proc *p;
	struct linux_sched_get_priority_min_args *uap;
{
	struct sched_get_priority_min_args bsd;

#ifdef DEBUG
	if (ldebug(sched_get_priority_min))
		printf(ARGS(sched_get_priority_min, "%d"), uap->policy);
#endif

	switch (uap->policy) {
	case LINUX_SCHED_OTHER:
		bsd.policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		bsd.policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		bsd.policy = SCHED_RR;
		break;
	default:
		return EINVAL;
	}
	return sched_get_priority_min(p, &bsd);
}

#define REBOOT_CAD_ON	0x89abcdef
#define REBOOT_CAD_OFF	0
#define REBOOT_HALT	0xcdef0123

int
linux_reboot(struct proc *p, struct linux_reboot_args *args)
{
	struct reboot_args bsd_args;

#ifdef DEBUG
	if (ldebug(reboot))
		printf(ARGS(reboot, "0x%x"), args->opt);
#endif
	if (args->opt == REBOOT_CAD_ON || args->opt == REBOOT_CAD_OFF)
		return (0);
	bsd_args.opt = args->opt == REBOOT_HALT ? RB_HALT : 0;
	return (reboot(p, &bsd_args));
}
