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
 *  $Id: linux_misc.c,v 1.37 1998/04/06 08:26:01 phk Exp $
 */

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
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_prot.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/frame.h>
#include <machine/psl.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
#include <i386/linux/linux_util.h>

int
linux_alarm(struct proc *p, struct linux_alarm_args *args)
{
    struct itimerval it, old_it;
    struct timeval tv;
    int s;

#ifdef DEBUG
    printf("Linux-emul(%d): alarm(%d)\n", p->p_pid, args->secs);
#endif
    it.it_value.tv_sec = (long)args->secs;
    it.it_value.tv_usec = 0;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    s = splclock(); /* XXX Still needed ? */
    old_it = p->p_realtimer;
    getmicrotime(&tv);
    if (timevalisset(&old_it.it_value))
	if (timevalcmp(&old_it.it_value, &tv, <))
	    timevalclear(&old_it.it_value);
	else
	    timevalsub(&old_it.it_value, &tv);
    splx(s);
    if (itimerfix(&it.it_value) || itimerfix(&it.it_interval))
	return EINVAL;
    s = splclock(); /* XXX Still needed ? */
    if (timevalisset(&p->p_realtimer.it_value))
	    untimeout(realitexpire, (caddr_t)p, p->p_ithandle);
    getmicrotime(&tv);
    if (timevalisset(&it.it_value)) {
	timevaladd(&it.it_value, &tv);
	p->p_ithandle = timeout(realitexpire, (caddr_t)p, hzto(&it.it_value));
    }
    p->p_realtimer = it;
    splx(s);
    if (old_it.it_value.tv_usec)
	old_it.it_value.tv_sec++;
    p->p_retval[0] = old_it.it_value.tv_sec;
    return 0;
}

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
    printf("Linux-emul(%d): brk(%08x)\n", p->p_pid, args->dsend);
#endif
    old = (vm_offset_t)vm->vm_daddr + ctob(vm->vm_dsize);
    new = (vm_offset_t)args->dsend;
    tmp.nsize = (char *) new;
    if (((caddr_t)new > vm->vm_daddr) && !obreak(p, &tmp))
	p->p_retval[0] = (int)new;
    else
	p->p_retval[0] = (int)old;

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
    printf("Linux-emul(%d): uselib(%s)\n", p->p_pid, args->library);
#endif

    a_out = NULL;
    locked = 0;
    vp = NULL;

    NDINIT(&ni, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, args->library, p);
    if (error = namei(&ni))
	goto cleanup;

    vp = ni.ni_vp;
    if (vp == NULL) {
	error = ENOEXEC;	/* ?? */
	goto cleanup;
    }

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
    if (error = VOP_GETATTR(vp, &attr, p->p_ucred, p))
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
    if (error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p))
	goto cleanup;

    if (error = VOP_OPEN(vp, FREAD, p->p_ucred, p))
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
printf("uselib: Non page aligned binary %d\n", file_offset);
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
	error = copyout((caddr_t)(buffer + file_offset), (caddr_t)vmaddr,
			a_out->a_text + a_out->a_data);

	/* release temporary kernel space */
	vm_map_remove(kernel_map, buffer,
		      buffer + round_page(a_out->a_text + a_out->a_data + file_offset));

	if (error)
	    goto cleanup;
    }
    else {
#ifdef DEBUG
printf("uselib: Page aligned binary %d\n", file_offset);
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
printf("mem=%08x = %08x %08x\n", vmaddr, ((int*)vmaddr)[0], ((int*)vmaddr)[1]);
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

/* XXX move */
struct linux_select_argv {
	int nfds;
	fd_set *readfds;
	fd_set *writefds;
	fd_set *exceptfds;
	struct timeval *timeout;
};

int
linux_select(struct proc *p, struct linux_select_args *args)
{
    struct linux_select_argv linux_args;
    struct linux_newselect_args newsel;
    int error;

#ifdef SELECT_DEBUG
    printf("Linux-emul(%d): select(%x)\n",
	   p->p_pid, args->ptr);
#endif
    if ((error = copyin((caddr_t)args->ptr, (caddr_t)&linux_args,
			sizeof(linux_args))))
	return error;

    newsel.nfds = linux_args.nfds;
    newsel.readfds = linux_args.readfds;
    newsel.writefds = linux_args.writefds;
    newsel.exceptfds = linux_args.exceptfds;
    newsel.timeout = linux_args.timeout;

    return linux_newselect(p, &newsel);
}

int
linux_newselect(struct proc *p, struct linux_newselect_args *args)
{
    struct select_args bsa;
    struct timeval tv0, tv1, utv, *tvp;
    caddr_t sg;
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): newselect(%d, %x, %x, %x, %x)\n",
	       p->p_pid, args->nfds, args->readfds, args->writefds,
	       args->exceptfds, args->timeout);
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
	printf("Linux-emul(%d): incoming timeout (%d/%d)\n",
	       p->p_pid, utv.tv_sec, utv.tv_usec);
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
    printf("Linux-emul(%d): real select returns %d\n",
	       p->p_pid, error);
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
	printf("Linux-emul(%d): outgoing timeout (%d/%d)\n",
	       p->p_pid, utv.tv_sec, utv.tv_usec);
#endif
	if ((error = copyout(&utv, args->timeout, sizeof(utv))))
	    goto select_out;
    }

select_out:
#ifdef DEBUG
    printf("Linux-emul(%d): newselect_out -> %d\n",
	       p->p_pid, error);
#endif
    return error;
}

int
linux_getpgid(struct proc *p, struct linux_getpgid_args *args)
{
    struct proc *curproc;

#ifdef DEBUG
    printf("Linux-emul(%d): getpgid(%d)\n", p->p_pid, args->pid);
#endif
    if (args->pid != p->p_pid) {
	if (!(curproc = pfind(args->pid)))
	    return ESRCH;
    }
    else
	curproc = p;
    p->p_retval[0] = curproc->p_pgid;
    return 0;
}

int
linux_fork(struct proc *p, struct linux_fork_args *args)
{
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): fork()\n", p->p_pid);
#endif
    if (error = fork(p, (struct fork_args *)args))
	return error;
    if (p->p_retval[1] == 1)
	p->p_retval[0] = 0;
    return 0;
}

/* XXX move */
struct linux_mmap_argv {
	linux_caddr_t addr;
	int len;
	int prot;
	int flags;
	int fd;
	int pos;
};

int
linux_mmap(struct proc *p, struct linux_mmap_args *args)
{
    struct mmap_args /* {
	caddr_t addr;
	size_t len;
	int prot;
	int flags;
	int fd;
	long pad;
	off_t pos;
    } */ bsd_args;
    int error;
    struct linux_mmap_argv linux_args;

    if ((error = copyin((caddr_t)args->ptr, (caddr_t)&linux_args,
			sizeof(linux_args))))
	return error;
#ifdef DEBUG
    printf("Linux-emul(%d): mmap(%08x, %d, %d, %08x, %d, %d)\n",
	   p->p_pid, linux_args.addr, linux_args.len, linux_args.prot,
	   linux_args.flags, linux_args.fd, linux_args.pos);
#endif
    bsd_args.flags = 0;
    if (linux_args.flags & LINUX_MAP_SHARED)
	bsd_args.flags |= MAP_SHARED;
    if (linux_args.flags & LINUX_MAP_PRIVATE)
	bsd_args.flags |= MAP_PRIVATE;
    if (linux_args.flags & LINUX_MAP_FIXED)
	bsd_args.flags |= MAP_FIXED;
    if (linux_args.flags & LINUX_MAP_ANON)
	bsd_args.flags |= MAP_ANON;
    bsd_args.addr = linux_args.addr;
    bsd_args.len = linux_args.len;
    bsd_args.prot = linux_args.prot | PROT_READ;	/* always required */
    bsd_args.fd = linux_args.fd;
    bsd_args.pos = linux_args.pos;
    bsd_args.pad = 0;
    return mmap(p, &bsd_args);
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

int
linux_pipe(struct proc *p, struct linux_pipe_args *args)
{
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): pipe(*)\n", p->p_pid);
#endif
    if (error = pipe(p, 0))
	return error;
    if (error = copyout(p->p_retval, args->pipefds, 2*sizeof(int)))
	return error;
    p->p_retval[0] = 0;
    return 0;
}

int
linux_time(struct proc *p, struct linux_time_args *args)
{
    struct timeval tv;
    linux_time_t tm;
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): time(*)\n", p->p_pid);
#endif
    microtime(&tv);
    tm = tv.tv_sec;
    if (args->tm && (error = copyout(&tm, args->tm, sizeof(linux_time_t))))
	return error;
    p->p_retval[0] = tm;
    return 0;
}

struct linux_times_argv {
    long    tms_utime;
    long    tms_stime;
    long    tms_cutime;
    long    tms_cstime;
};

#define CLK_TCK 100	/* Linux uses 100 */
#define CONVTCK(r)	(r.tv_sec * CLK_TCK + r.tv_usec / (1000000 / CLK_TCK))

int
linux_times(struct proc *p, struct linux_times_args *args)
{
    struct timeval tv;
    struct linux_times_argv tms;
    struct rusage ru;
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): times(*)\n", p->p_pid);
#endif
    calcru(p, &ru.ru_utime, &ru.ru_stime, NULL);

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

/* XXX move */
struct linux_newuname_t {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

int
linux_newuname(struct proc *p, struct linux_newuname_args *args)
{
    struct linux_newuname_t linux_newuname;

#ifdef DEBUG
    printf("Linux-emul(%d): newuname(*)\n", p->p_pid);
#endif
    bzero(&linux_newuname, sizeof(struct linux_newuname_args));
    strncpy(linux_newuname.sysname, ostype, 64);
    strncpy(linux_newuname.nodename, hostname, 64);
    strncpy(linux_newuname.release, osrelease, 64);
    strncpy(linux_newuname.version, version, 64);
    strncpy(linux_newuname.machine, machine, 64);
    strncpy(linux_newuname.domainname, domainname, 64);
    return (copyout((caddr_t)&linux_newuname, (caddr_t)args->buf,
	    	    sizeof(struct linux_newuname_t)));
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
    printf("Linux-emul(%d): utime(%s, *)\n", p->p_pid, args->fname);
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
	if ((error = copyout(tv, tvp, sizeof(tv))))
	    return error;
	bsdutimes.tptr = tvp;
    } else
	bsdutimes.tptr = NULL;

    bsdutimes.path = args->fname;
    return utimes(p, &bsdutimes);
}

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
    printf("Linux-emul(%d): waitpid(%d, 0x%x, %d)\n",
	   p->p_pid, args->pid, args->status, args->options);
#endif
    tmp.pid = args->pid;
    tmp.status = args->status;
    tmp.options = args->options;
    tmp.rusage = NULL;

    if (error = wait4(p, &tmp))
	return error;
    if (args->status) {
	if (error = copyin(args->status, &tmpstat, sizeof(int)))
	    return error;
	if (WIFSIGNALED(tmpstat))
	    tmpstat = (tmpstat & 0xffffff80) |
		      bsd_to_linux_signal[WTERMSIG(tmpstat)];
	else if (WIFSTOPPED(tmpstat))
	    tmpstat = (tmpstat & 0xffff00ff) |
		      (bsd_to_linux_signal[WSTOPSIG(tmpstat)]<<8);
	return copyout(&tmpstat, args->status, sizeof(int));
    } else
	return 0;
}

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
    printf("Linux-emul(%d): wait4(%d, 0x%x, %d, 0x%x)\n",
	   p->p_pid, args->pid, args->status, args->options, args->rusage);
#endif
    tmp.pid = args->pid;
    tmp.status = args->status;
    tmp.options = args->options;
    tmp.rusage = args->rusage;

    if (error = wait4(p, &tmp))
	return error;

    p->p_siglist &= ~sigmask(SIGCHLD);

    if (args->status) {
	if (error = copyin(args->status, &tmpstat, sizeof(int)))
	    return error;
	if (WIFSIGNALED(tmpstat))
	    tmpstat = (tmpstat & 0xffffff80) |
		  bsd_to_linux_signal[WTERMSIG(tmpstat)];
	else if (WIFSTOPPED(tmpstat))
	    tmpstat = (tmpstat & 0xffff00ff) |
		  (bsd_to_linux_signal[WSTOPSIG(tmpstat)]<<8);
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
	printf("Linux-emul(%d): mknod(%s, %d, %d)\n",
	   p->p_pid, args->path, args->mode, args->dev);
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
	printf("Linux-emul(%d): personality(%d)\n",
	   p->p_pid, args->per);
#endif
	if (args->per != 0)
		return EINVAL;

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
	printf("Linux-emul(%d): setitimer(%08x, %08x)\n",
	   p->p_pid, args->itv, args->oitv);
#endif
	bsa.which = args->which;
	bsa.itv = args->itv;
	bsa.oitv = args->oitv;
	if (args->itv) {
	    if ((error = copyin((caddr_t)args->itv, (caddr_t)&foo,
			sizeof(foo))))
		return error;
#ifdef DEBUG
	    printf("setitimer: value: sec: %d, usec: %d\n", foo.it_value.tv_sec, foo.it_value.tv_usec);
	    printf("setitimer: interval: sec: %d, usec: %d\n", foo.it_interval.tv_sec, foo.it_interval.tv_usec);
#endif
	}
	return setitimer(p, &bsa);
}

int
linux_getitimer(struct proc *p, struct linux_getitimer_args *args)
{
	struct getitimer_args bsa;
#ifdef DEBUG
	printf("Linux-emul(%d): getitimer(%08x)\n",
	   p->p_pid, args->itv);
#endif
	bsa.which = args->which;
	bsa.itv = args->itv;
	return getitimer(p, &bsa);
}

int
linux_iopl(struct proc *p, struct linux_iopl_args *args)
{
	int error;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error != 0)
		return error;
	if (securelevel > 0)
		return EPERM;
	p->p_md.md_regs->tf_eflags |= PSL_IOPL;
	return 0;
}

int
linux_nice(struct proc *p, struct linux_nice_args *args)
{
	struct setpriority_args	bsd_args;

	bsd_args.which = PRIO_PROCESS;
	bsd_args.who = 0;	/* current process */
	bsd_args.prio = args->inc;
	return setpriority(p, &bsd_args);
}

