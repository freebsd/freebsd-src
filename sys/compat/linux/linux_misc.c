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
 *  $Id: linux_misc.c,v 1.10 1996/01/14 10:59:57 sos Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/imgact_aout.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/vnode.h>
#include <sys/wait.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_prot.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/psl.h>

#include <i386/linux/linux.h>
#include <i386/linux/sysproto.h>

struct linux_alarm_args {
    unsigned int secs;
};

int
linux_alarm(struct proc *p, struct linux_alarm_args *args, int *retval)
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
    s = splclock();
    old_it = p->p_realtimer;
    tv = time;
    if (timerisset(&old_it.it_value))
	if (timercmp(&old_it.it_value, &tv, <))
	    timerclear(&old_it.it_value);
	else
	    timevalsub(&old_it.it_value, &tv);
    splx(s);
    if (itimerfix(&it.it_value) || itimerfix(&it.it_interval))
	return EINVAL;
    s = splclock();
    untimeout(realitexpire, (caddr_t)p);
    tv = time;
    if (timerisset(&it.it_value)) {
	timevaladd(&it.it_value, &tv);
	timeout(realitexpire, (caddr_t)p, hzto(&it.it_value));
    }
    p->p_realtimer = it;
    splx(s);
    if (old_it.it_value.tv_usec)
	old_it.it_value.tv_sec++;
    *retval = old_it.it_value.tv_sec;
    return 0;
}

struct linux_brk_args {
    linux_caddr_t dsend;
};

int
linux_brk(struct proc *p, struct linux_brk_args *args, int *retval)
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
    *retval = old;
    if ((new-old) > 0) {
	if (swap_pager_full)
	    return ENOMEM;
	error = vm_map_find(&vm->vm_map, NULL, 0, &old, (new-old), FALSE,
			VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error) 
	    return error;
	vm->vm_dsize += btoc((new-old));
	*retval = (int)(vm->vm_daddr + ctob(vm->vm_dsize));
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
    if (((caddr_t)new > vm->vm_daddr) && !obreak(p, &tmp, retval))
	retval[0] = (int)new;
    else
	retval[0] = (int)old;

    return 0;
#endif
}

struct linux_uselib_args {
    char *library;
};

int
linux_uselib(struct proc *p, struct linux_uselib_args *args, int *retval)
{
    struct nameidata ni;
    struct vnode *vp;
    struct exec *a_out = 0;
    struct vattr attr;
    unsigned long vmaddr, virtual_offset, file_offset;
    unsigned long buffer, bss_size;
    char *ptr;
    char path[MAXPATHLEN];
    const char *prefix = "/compat/linux";
    size_t sz, len;
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): uselib(%s)\n", p->p_pid, args->library);
#endif

    for (ptr = path; (*ptr = *prefix) != '\0'; ptr++, prefix++) ;
    sz = MAXPATHLEN - (ptr - path);
    if (error = copyinstr(args->library, ptr, sz, &len))
	return error;
    if (*ptr != '/')
	return EINVAL;

#ifdef DEBUG
    printf("Linux-emul(%d): uselib(%s)\n", p->p_pid, path);
#endif

    NDINIT(&ni, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, path, p);
    if (error = namei(&ni))
	return error;

    vp = ni.ni_vp;
    if (vp == NULL)
	    return ENOEXEC;

    if (vp->v_writecount) {
	    VOP_UNLOCK(vp);
	    return ETXTBSY;
    }

    if (error = VOP_GETATTR(vp, &attr, p->p_ucred, p)) {
	    VOP_UNLOCK(vp);
	    return error;
    }

    if ((vp->v_mount->mnt_flag & MNT_NOEXEC)
	|| ((attr.va_mode & 0111) == 0)
	|| (attr.va_type != VREG)) {
	    VOP_UNLOCK(vp);
	    return ENOEXEC;
    }

    if (attr.va_size == 0) {
	    VOP_UNLOCK(vp);
	    return ENOEXEC;
    }

    if (error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p)) {
	VOP_UNLOCK(vp);
	return error;
    }

    if (error = VOP_OPEN(vp, FREAD, p->p_ucred, p)) {
	VOP_UNLOCK(vp);
	return error;
    }

    VOP_UNLOCK(vp);	/* lock no longer needed */

    error = vm_mmap(kernel_map, (vm_offset_t *)&a_out, 1024,
	    	    VM_PROT_READ, VM_PROT_READ, 0, (caddr_t)vp, 0);
    if (error)
	return (error);

    /*
     * Is it a Linux binary ?
     */
    if (((a_out->a_magic >> 16) & 0xff) != 0x64)
	return ENOEXEC;

    /*
     * Set file/virtual offset based on a.out variant.
     */
    switch ((int)(a_out->a_magic & 0xffff)) {
    case 0413:	/* ZMAGIC */
	virtual_offset = 0;	/* actually aout->a_entry */
	file_offset = 1024;
	break;
    case 0314:	/* QMAGIC */
	virtual_offset = 0;	/* actually aout->a_entry */
	file_offset = 0;
	break;
    default:
	return ENOEXEC;
    }

    vp->v_flag |= VTEXT;
    bss_size = round_page(a_out->a_bss);
    /*
     * Check if file_offset page aligned,.
     * Currently we cannot handle misalinged file offsets,
     * and so we read in the entire image (what a waste).
     */
    if (file_offset & PGOFSET) {
#ifdef DEBUG
printf("uselib: Non page aligned binary %d\n", file_offset);
#endif
	/*
	 * Map text+data read/write/execute
	 */
	vmaddr = virtual_offset + round_page(a_out->a_entry);
	error = vm_map_find(&p->p_vmspace->vm_map, NULL, 0, &vmaddr,
		    	    round_page(a_out->a_text + a_out->a_data), FALSE,
				VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error)
	    return error;

	error = vm_mmap(kernel_map, &buffer,
			round_page(a_out->a_text + a_out->a_data + file_offset),
		   	VM_PROT_READ, VM_PROT_READ, MAP_FILE,
			(caddr_t)vp, trunc_page(file_offset));
	if (error)
	    return error;

	error = copyout((caddr_t)(buffer + file_offset), (caddr_t)vmaddr, 
			a_out->a_text + a_out->a_data);
	if (error)
	    return error;

	vm_map_remove(kernel_map, trunc_page(vmaddr),
		      round_page(a_out->a_text + a_out->a_data + file_offset));

	error = vm_map_protect(&p->p_vmspace->vm_map, vmaddr,
		   	       round_page(a_out->a_text + a_out->a_data),
		   	       VM_PROT_ALL, TRUE);
	if (error)
	    return error;
    }
    else {
#ifdef DEBUG
printf("uselib: Page aligned binary %d\n", file_offset);
#endif
	vmaddr = virtual_offset + trunc_page(a_out->a_entry);
	error = vm_mmap(&p->p_vmspace->vm_map, &vmaddr,
			a_out->a_text + a_out->a_data,
			VM_PROT_ALL, VM_PROT_ALL, MAP_PRIVATE | MAP_FIXED,
			(caddr_t)vp, file_offset);
	if (error)
	    return (error);
    }
#ifdef DEBUG
printf("mem=%08x = %08x %08x\n", vmaddr, ((int*)vmaddr)[0], ((int*)vmaddr)[1]);
#endif
    if (bss_size != 0) {
	vmaddr = virtual_offset + round_page(a_out->a_entry) +
		 round_page(a_out->a_text + a_out->a_data);
	error = vm_map_find(&p->p_vmspace->vm_map, NULL, 0, &vmaddr, 
			    bss_size, FALSE,
				VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error)
	    return error;
	error = vm_map_protect(&p->p_vmspace->vm_map, vmaddr, bss_size,
		   	       VM_PROT_ALL, TRUE);
	if (error)
	    return error;
    }
    return 0;
}

struct linux_select_args {
    void *ptr;
};

int
linux_select(struct proc *p, struct linux_select_args *args, int *retval)
{
    struct {
	int nfds;
	fd_set *readfds;
	fd_set *writefds;
	fd_set *exceptfds;
	struct timeval *timeout; 
    } linux_args;
    struct select_args /* {
	unsigned int nd;
	fd_set *in;
	fd_set *ou;
	fd_set *ex;
	struct timeval *tv;
    } */ bsd_args;
    int error;

    if ((error = copyin((caddr_t)args->ptr, (caddr_t)&linux_args,
			sizeof(linux_args))))
	return error;
#ifdef DEBUG
    printf("Linux-emul(%d): select(%d, %d, %d, %d, %d)\n", 
	   p->p_pid, linux_args.nfds, linux_args.readfds,
	   linux_args.writefds, linux_args.exceptfds, 
	   linux_args.timeout);
#endif
    bsd_args.nd = linux_args.nfds;
    bsd_args.in = linux_args.readfds;
    bsd_args.ou = linux_args.writefds;
    bsd_args.ex = linux_args.exceptfds;
    bsd_args.tv = linux_args.timeout;
    return select(p, &bsd_args, retval);
}

struct linux_getpgid_args {
    int pid;
};

int
linux_getpgid(struct proc *p, struct linux_getpgid_args *args, int *retval)
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
    *retval = curproc->p_pgid;
    return 0;
}

int
linux_fork(struct proc *p, void *args, int *retval)
{
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): fork()\n", p->p_pid);
#endif
    if (error = fork(p, args, retval))
	return error;
    if (retval[1] == 1)
	retval[0] = 0;
    return 0;
}

struct linux_mmap_args {
    void *ptr;
};

int
linux_mmap(struct proc *p, struct linux_mmap_args *args, int *retval)
{
    struct {
	linux_caddr_t addr;
	int len;
	int prot;
	int flags;
	int fd;
	int pos;
    } linux_args;
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
    bsd_args.prot = linux_args.prot;
    bsd_args.fd = linux_args.fd;
    bsd_args.pos = linux_args.pos;
    bsd_args.pad = 0;
    return mmap(p, &bsd_args, retval);
}

struct linux_pipe_args {
    int *pipefds;
};

int
linux_pipe(struct proc *p, struct linux_pipe_args *args, int *retval)
{
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): pipe(*)\n", p->p_pid);
#endif
    if (error = pipe(p, 0, retval))
	return error;
    if (error = copyout(retval, args->pipefds, 2*sizeof(int)))
	return error;
    *retval = 0;
    return 0;
}

struct linux_time_args {
    linux_time_t *tm;
};

int
linux_time(struct proc *p, struct linux_time_args *args, int *retval)
{
    struct timeval tv;
    linux_time_t tm;
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): time(*)\n", p->p_pid);
#endif
    microtime(&tv);
    tm = tv.tv_sec;
    if (error = copyout(&tm, args->tm, sizeof(linux_time_t)))
	return error;
    *retval = tv.tv_sec;
    return 0;
}

struct linux_tms {
    long    tms_utime;
    long    tms_stime;
    long    tms_cutime;
    long    tms_cstime;
};

struct linux_tms_args {
    char *buf;
};

int
linux_times(struct proc *p, struct linux_tms_args *args, int *retval)
{
    struct timeval tv;
    struct linux_tms tms;

#ifdef DEBUG
    printf("Linux-emul(%d): times(*)\n", p->p_pid);
#endif
    tms.tms_utime = p->p_uticks;
    tms.tms_stime = p->p_sticks;
    tms.tms_cutime = p->p_stats->p_cru.ru_utime.tv_sec * hz +
	    ((p->p_stats->p_cru.ru_utime.tv_usec * hz)/1000000);
    tms.tms_cstime = p->p_stats->p_cru.ru_stime.tv_sec * hz +
	    ((p->p_stats->p_cru.ru_stime.tv_usec * hz)/1000000);
    microtime(&tv);
    *retval = tv.tv_sec * hz + (tv.tv_usec * hz)/1000000;
    return (copyout((caddr_t)&tms, (caddr_t)args->buf,
	    	    sizeof(struct linux_tms)));
}

struct linux_newuname_t {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct linux_newuname_args {
    char *buf;
};

int
linux_newuname(struct proc *p, struct linux_newuname_args *args, int *retval)
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

struct linux_utime_args {
    char	*fname;
    linux_time_t    *timeptr;
};

int
linux_utime(struct proc *p, struct linux_utime_args *args, int *retval)
{
    struct utimes_args /* {
	char	*path;
	struct	timeval *tptr;
    } */ bsdutimes;
    struct timeval tv;

#ifdef DEBUG
    printf("Linux-emul(%d): utime(%s, *)\n", p->p_pid, args->fname);
#endif
    tv.tv_sec = (long)args->timeptr;
    tv.tv_usec = 0;
    bsdutimes.tptr = &tv;
    bsdutimes.path = args->fname;
    return utimes(p, &bsdutimes, retval);
}

struct linux_waitpid_args {
    int pid;
    int *status;
    int options;
};

int
linux_waitpid(struct proc *p, struct linux_waitpid_args *args, int *retval)
{
    struct wait_args /* {
	int pid;
	int *status;
	int options;
	struct	rusage *rusage;
    } */ tmp;
    int error, tmpstat;

#ifdef DEBUG
    printf("Linux-emul(%d): waitpid(%d, *, %d)\n", 
	   p->p_pid, args->pid, args->options);
#endif
    tmp.pid = args->pid;
    tmp.status = args->status;
    tmp.options = args->options;
    tmp.rusage = NULL;

    if (error = wait4(p, &tmp, retval))
	return error;
    if (error = copyin(args->status, &tmpstat, sizeof(int)))
	return error;
    if (WIFSIGNALED(tmpstat))
	tmpstat = (tmpstat & 0xffffff80) |
		  bsd_to_linux_signal[WTERMSIG(tmpstat)];
    else if (WIFSTOPPED(tmpstat))
	tmpstat = (tmpstat & 0xffff00ff) |
	      	  (bsd_to_linux_signal[WSTOPSIG(tmpstat)]<<8);
    return copyout(&tmpstat, args->status, sizeof(int));
}

struct linux_wait4_args {
    int pid;
    int *status;
    int options;
    struct rusage *rusage;
};

int 
linux_wait4(struct proc *p, struct linux_wait4_args *args, int *retval)
{
    struct wait_args /* {
	int pid;
	int *status;
	int options;
	struct	rusage *rusage;
    } */ tmp;
    int error, tmpstat;

#ifdef DEBUG
    printf("Linux-emul(%d): wait4(%d, *, %d, *)\n", 
	   p->p_pid, args->pid, args->options);
#endif
    tmp.pid = args->pid;
    tmp.status = args->status;
    tmp.options = args->options;
    tmp.rusage = args->rusage;

    if (error = wait4(p, &tmp, retval))
	return error;
    if (error = copyin(args->status, &tmpstat, sizeof(int)))
	return error;
    if (WIFSIGNALED(tmpstat))
	tmpstat = (tmpstat & 0xffffff80) |
	      bsd_to_linux_signal[WTERMSIG(tmpstat)];
    else if (WIFSTOPPED(tmpstat))
	tmpstat = (tmpstat & 0xffff00ff) |
	      (bsd_to_linux_signal[WSTOPSIG(tmpstat)]<<8);
    return copyout(&tmpstat, args->status, sizeof(int));
}

struct linux_mknod_args {
	char *path;
	int mode;
	int dev;
};

int 
linux_mknod(struct proc *p, struct linux_mknod_args *args, int *retval)
{
	if (args->mode & S_IFIFO)
		return mkfifo(p, (struct mkfifo_args *)args, retval);
	else
		return mknod(p, (struct mknod_args *)args, retval);
}
