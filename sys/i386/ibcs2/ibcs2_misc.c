/*-
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1994 Sean Eric Fagan
 * All rights reserved.
 *
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
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
 *	$Id: ibcs2_misc.c,v 1.1 1994/10/14 08:53:06 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <vm/vm.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>

int ibcs2_trace = 0;

struct ibcs2_traceemu_args {
	int options;
};

int
ibcs2_traceemu(struct proc *p, struct ibcs2_traceemu_args *args, int *retval)
{
	*retval = ibcs2_trace;
	ibcs2_trace = args->options;
	return 0;
}

int
ibcs2_access(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'access'\n");
	return access(p, args, retval);
}

struct ibcs2_alarm_args {
	unsigned int secs;
};

int
ibcs2_alarm(struct proc *p, struct ibcs2_alarm_args *args, int *retval)
{
	extern struct timeval time;
	struct itimerval it, oit;
	int s;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'alarm' secs=%d\n", args->secs);
	it.it_value.tv_sec = (long)args->secs;
	it.it_value.tv_usec = 0;
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 0;
	s = splclock();
	oit = p->p_realtimer;
	if (timerisset(&oit.it_value))
		if (timercmp(&oit.it_value, &time, <))
			timerclear(&oit.it_value);
		else
			timevalsub(&oit.it_value, &time);
	splx(s);
	if (itimerfix(&it.it_value) || itimerfix(&it.it_interval))
		return EINVAL;
	s = splclock();
	untimeout(realitexpire, (caddr_t)p);
	if (timerisset(&it.it_value)) {
		timevaladd(&it.it_value, &time);
		timeout(realitexpire, (caddr_t)p, hzto(&it.it_value));
	}
	p->p_realtimer = it;
	splx(s);
	if (oit.it_value.tv_usec)
		oit.it_value.tv_sec++;
	*retval = oit.it_value.tv_sec;
	return 0;
}

struct ibcs2_break_args {
  	char *dsend;
};

int
ibcs2_break(struct proc *p, struct ibcs2_break_args *args, int *retval)
{
	struct vmspace *vm = p->p_vmspace;
	vm_offset_t new, old;
	int rv, diff;
	extern int swap_pager_full;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'break' dsend=%x\n",
			(unsigned int)args->dsend);

	if ((vm_offset_t)args->dsend < (vm_offset_t)vm->vm_daddr)
		return EINVAL;
	if (((caddr_t)args->dsend - (caddr_t)vm->vm_daddr)
	    > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		return ENOMEM;

	old = round_page((vm_offset_t)vm->vm_daddr) + ctob(vm->vm_dsize);
	new = round_page((vm_offset_t)args->dsend);

	diff = new - old;
	if (diff > 0) {
		if (swap_pager_full) {
			return ENOMEM;
		}
		rv = vm_map_find(&vm->vm_map, NULL, 0, &old, diff, FALSE);
		if (rv != KERN_SUCCESS) {
			return ENOMEM;
		}
		vm->vm_dsize += btoc(diff);
	}
	else if (diff < 0) {
		diff = -diff;
		rv = vm_map_remove(&vm->vm_map, new, new + diff);
		if (rv != KERN_SUCCESS)
			return ENOMEM;
		vm->vm_dsize -= btoc(diff);
	}
	return 0;
}

int
ibcs2_chdir(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'chdir'\n");
	return chdir(p, args, retval);
}

int
ibcs2_chmod(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'chmod'\n");
	return chmod(p, args, retval);
}

int
ibcs2_chown(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'chown'\n");
	return chown(p, args, retval);
}

int
ibcs2_chroot(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'chroot'\n");
	return chroot(p, args, retval);
}

struct ibcs2_close_args {
	int fd;
};

int
ibcs2_close(struct proc *p, struct ibcs2_close_args *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'close' fd=%d\n", args->fd);
	return close(p, args, retval);
}

struct exec_args {
	char *name;
	char **argv;
};

int
ibcs2_exec(struct proc *p, struct exec_args *args, int *retval)
{
	struct execve_args {
		char *name;
		char **argv;
		char **envp;
	} execve_args;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'exec' name=%s\n", args->name);
	execve_args.name = args->name;
	execve_args.argv = args->argv;
	execve_args.envp = 0;
	return execve(p, &execve_args, retval);
}

struct ibcs2_exece_args {
		char *name;
		char **argv;
		char **envp;
};

int
ibcs2_exece(struct proc *p, struct ibcs2_exece_args *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'exece' name=%s\n", args->name);
	return execve(p, args, retval);
}

int
ibcs2_exit(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'exit'\n");
	return exit(p, args, retval);
}

int
ibcs2_fork(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'fork'\n");
	return fork(p, args, retval);
}

int
ibcs2_fsync(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'fsync'\n");
	return fsync(p, args, retval);
}

int
ibcs2_getgid(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'getgid'\n");
	return getgid(p, args, retval);
}

struct ibcs2_getgroups_args {
	int gidsetsize;
	ibcs2_gid_t *gidset;
};

int
ibcs2_getgroups(struct proc *p, struct ibcs2_getgroups_args *args, int *retval)
{
	struct getgroups_args {
		u_int	gidsetsize;
		gid_t	*gidset;
	} tmp;
	ibcs2_gid_t *ibcs2_gidset;
	int i, error;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'getgroups'\n");

	tmp.gidsetsize = args->gidsetsize;
	tmp.gidset = (gid_t *)UA_ALLOC();
	ibcs2_gidset = (ibcs2_gid_t *)&tmp.gidset[NGROUPS_MAX];
	if (error = getgroups(p, &tmp, retval))
		return error;
	for (i = 0; i < retval[0]; i++)
		ibcs2_gidset[i] = (ibcs2_gid_t)tmp.gidset[i];
	return copyout((caddr_t)ibcs2_gidset, (caddr_t)args->gidset,
		       sizeof(ibcs2_gid_t) * retval[0]);
}

int
ibcs2_getpid(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'getpid'\n");
	return getpid(p, args, retval);
}

int
ibcs2_getuid(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'getuid'\n");
	return getuid(p, args, retval);
}

struct gtime_args {
	long *timeptr;
};

int
ibcs2_gtime(struct proc *p, struct gtime_args *args, int *retval)
{
	int error = 0;
	struct timeval tv;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'gtime'\n");
	microtime(&tv);
	*retval = tv.tv_sec;
	if (args)
		(long)args->timeptr = tv.tv_sec;
	return error;
}

int
ibcs2_link(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'link'\n");
	return link(p, args, retval);
}

int
ibcs2_mkdir(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'mkdir'\n");
	return mkdir(p, args, retval);
}

struct ibcs2_mknod_args {
	char		*fname;
	int		fmode;
	ibcs2_dev_t	dev;
};

int
ibcs2_mknod(struct proc *p, struct ibcs2_mknod_args *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'mknod'\n");
	if (S_ISFIFO(args->fmode))
		return mkfifo(p, args, retval);
	return mknod(p, args, retval);
}

struct ibcs2_nice_args {
	int niceval;
};

int
ibcs2_nice(struct proc *p, struct ibcs2_nice_args *args, int *retval)
{
	int error;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'nice'\n");
	error = donice(p, p, args->niceval);
	*retval = p->p_nice;
	return error;
}

struct ibcs2_pathconf_args {
	long unused;
	int cmd;
};
int
ibcs2_pathconf(struct proc *p, struct ibcs2_pathconf_args *args, int *retval)
{
		if (ibcs2_trace & IBCS2_TRACE_MISC) 
			printf("IBCS2: '(f)pathconf'\n"); 
	    	switch (args->cmd) {
	    	case 0:	/* _PC_LINK_MAX */
			*retval = (LINK_MAX);
			break;
	    	case 1:	/* _PC_MAX_CANON */
			*retval = (MAX_CANON);
			break;
	    	case 2:	/* _PC_MAX_INPUT */
			*retval = (MAX_INPUT);
			break;
	    	case 5:	/* _PC_PATH_MAX */
			*retval = (PATH_MAX);
			break;
	    	case 8:	/* _PC_VDISABLE */
	      		*retval = (_POSIX_VDISABLE);
	      		break;
	    	case 3:	/* _PC_NAME_MAX */
			*retval = (NAME_MAX);
			break;
	    	case 4:	/* _PC_PATH_MAX */
	      		*retval = (PATH_MAX);
	      		break;
	    	case 6:	/* _PC_CHOWN_RESTRICTED */
#ifdef _POSIX_CHOWN_RESTRICTED
	      		*retval = _POSIX_CHOWN_RESTRICTED;
#else
			*retval = (0);
#endif
	      		break;
	    	case 7:	/* _PC_NO_TRUNC */
#ifdef _POSIX_NO_TRUNC
	      		*retval = _POSIX_NO_TRUNC;
#else
			*retval = (1);
#endif
	      		break;
		default:
			*retval = -1;
	      		return EINVAL;
		}
	      	return 0;
}

int
ibcs2_pause(struct proc *p, void *args, int *retval)
{
	int mask = 0;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'pause'\n");
	return sigsuspend(p, &mask, retval);
}

int
ibcs2_pipe(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'pipe'\n");
	return pipe(p, args, retval);
}

struct ibcs2_poll {
	int fd;
	short events;
	short revents;
};

struct ibcs2_poll_args {
	struct ibcs2_poll *fds;
	unsigned long nfds;
	int timeout;
};

int
ibcs2_poll(struct proc *p, struct ibcs2_poll_args *args, int *retval)
{
	struct ibcs2_poll conv;
	fd_set *readfds, *writefds, *exceptfds;
	struct timeval *timeout;
	struct select_args {
		u_int	nd;
		fd_set	*in, *ou, *ex;
		struct	timeval *tv;
	} tmp_select;
	int i, error;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'poll'\n");
	if (args->nfds > FD_SETSIZE)
		return EINVAL;
	readfds = (fd_set *)UA_ALLOC(); 
	FD_ZERO(readfds);
	writefds = (fd_set *)UA_ALLOC() + sizeof(fd_set *);
	FD_ZERO(writefds);
	exceptfds = (fd_set *)UA_ALLOC() + 2*sizeof(fd_set *);
	FD_ZERO(exceptfds);
	timeout = (struct timeval *)UA_ALLOC() + 3*sizeof(fd_set *);
	if (args->timeout == -1)
		timeout = NULL;
	else {
		timeout->tv_usec = (args->timeout % 1000)*1000;
		timeout->tv_sec  = args->timeout / 1000;
	}
	tmp_select.nd = 0;
	tmp_select.in = readfds;
	tmp_select.ou = writefds;
	tmp_select.ex = exceptfds;
	tmp_select.tv = timeout;
	for (i = 0; i < args->nfds; i++) {
		if (error = copyin(args->fds + i*sizeof(struct ibcs2_poll),
				   &conv, sizeof(conv)))
			return error;
		conv.revents = 0;
		if (conv.fd < 0 || conv.fd > FD_SETSIZE)
			continue;
		if (conv.fd >= tmp_select.nd)
			tmp_select.nd = conv.fd + 1;
		if (conv.events & IBCS2_READPOLL)
			FD_SET(conv.fd, readfds);
		if (conv.events & IBCS2_WRITEPOLL)
			FD_SET(conv.fd, writefds);
		FD_SET(conv.fd, exceptfds);
	}
	if (error = select(p, &tmp_select, retval))
		return error;
	if (*retval == 0)
		return 0;
	*retval = 0;
	for (*retval = 0, i = 0; i < args->nfds; i++) {
		copyin(args->fds + i*sizeof(struct ibcs2_poll), 
		       &conv, sizeof(conv));
		conv.revents = 0;
		if (conv.fd < 0 || conv.fd > FD_SETSIZE) 
			/* should check for open as well */
			conv.revents |= IBCS2_POLLNVAL; 
		else {
			if (FD_ISSET(conv.fd, readfds))
				conv.revents |= IBCS2_POLLIN;
			if (FD_ISSET(conv.fd, writefds))
				conv.revents |= IBCS2_POLLOUT;
			if (FD_ISSET(conv.fd, exceptfds))
				conv.revents |= IBCS2_POLLERR; 
			if (conv.revents)
				++*retval;
		}
		if (error = copyout(&conv,
				    args->fds + i*sizeof(struct ibcs2_poll), 
				    sizeof(conv)))
			return error;
	}
	return 0;
}

struct ibcs2_procids_args {
  	int req;
  	int eax;
};

int
ibcs2_procids(struct proc *p, struct ibcs2_procids_args *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'procids' request=%d,  eax=%x\n",
  			args->req, args->eax);
  	switch (args->req) {
  	case 0:	/* getpgrp */
    		return getpgrp(p, args, retval);
  	case 1:	/* setpgrp */
		{
    			struct setpgid_args { 
				int pid; 
				int pgid;
			} tmp;
    			tmp.pid = tmp.pgid = 0;
    			return setpgid(p, &tmp, retval);
  		}
  	case 2:	/* setpgid */
    		return setpgid(p, args, retval);
  	case 3:	/* setsid */
    		return setsid(p, args, retval);
  	default:
    		return EINVAL;
  	}
}

int
ibcs2_profil(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'profil'\n");
	return profil(p, args, retval);
}

int
ibcs2_ptrace(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'ptrace'\n");
	return ptrace(p, args, retval);
}

int
ibcs2_readlink(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'readlink'\n");
	return readlink(p, args, retval);
}

int
ibcs2_rename(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'rename'\n");
	return rename(p, args, retval);
}

int
ibcs2_rmdir(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'rmdir'\n");
	return rmdir(p, args, retval);
}

struct ibcs2_secure_args {
	int	cmd;
	int	arg1;
	int	arg2;
	int	arg3;
	int	arg4;
	int	arg5;
};

int
ibcs2_secure(struct proc *p, struct ibcs2_secure_args *args, int *retval)
{
  	struct trapframe *tf = (struct trapframe *)p->p_md.md_regs;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'secure'\n");

	switch (args->cmd) {

	case 1:		/* get login uid */
		*retval = p->p_ucred->cr_uid;
		return EPERM;

	case 2:		/* set login uid */

	default:
		printf("IBCS2: 'secure' cmd=%d not implemented\n",args->cmd);
	}
	return EINVAL;
}

struct ibcs2_setgid_args {
	ibcs2_gid_t gid;
};

int
ibcs2_setgid(struct proc *p, struct ibcs2_setgid_args *args, int *retval)
{
	struct setgid_args {
		gid_t gid;
	} tmp;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'setgid'\n");
	tmp.gid = (gid_t) args->gid;
	return setgid(p, &tmp, retval);
}

struct ibcs2_setgroups_args {
	int gidsetsize;
	ibcs2_gid_t *gidset;
};

int
ibcs2_setgroups(struct proc *p, struct ibcs2_setgroups_args *args, int *retval)
{
	struct setgroups_args {
		u_int	gidsetsize;
		gid_t	*gidset;
	} tmp;
	ibcs2_gid_t *ibcs2_gidset;
	int i, error;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'setgroups'\n");
	tmp.gidsetsize = args->gidsetsize;
	tmp.gidset = (gid_t *)UA_ALLOC();
	ibcs2_gidset = (ibcs2_gid_t *)&tmp.gidset[NGROUPS_MAX];
	if (error = copyin((caddr_t)args->gidset, (caddr_t)ibcs2_gidset, 
		           sizeof(ibcs2_gid_t) * tmp.gidsetsize))
		return error;
	for (i = 0; i < tmp.gidsetsize; i++)
		tmp.gidset[i] = (gid_t)ibcs2_gidset[i];
	return setgroups(p, &tmp, retval);
}

struct ibcs2_setuid_args {
	ibcs2_uid_t uid;
};

int
ibcs2_setuid(struct proc *p, struct ibcs2_setuid_args *args, int *retval)
{
	struct setuid_args {
		uid_t uid;
	} tmp;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'setuid'\n");
	tmp.uid = (uid_t) args->uid;
	return setuid(p, &tmp, retval);
}

int
ibcs2_smount(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'smount'\n");
	return mount(p, args, retval);
}

struct ibcs2_stime_args {
	long *timeptr;
};

int
ibcs2_stime(struct proc *p, struct ibcs2_stime_args *args, int *retval)
{
	int error;
	
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'stime'\n");
	if (error = suser(p->p_ucred, &p->p_acflag))
		return error;
	if (args->timeptr) {
#if 0
		/* WHAT DO WE DO ABOUT PENDING REAL-TIME TIMEOUTS??? */
		boottime.tv_sec += (long)args->timeptr - time.tv_sec;
		s = splhigh();
		time.tv_sec = (long)args->timeptr; 
		time.tv_usec = 0;
		splx(s);
		resettodr();
#else
		printf("IBCS2: trying to set system time %d\n", 
		       (long)args->timeptr);
#endif
	}
	return 0;
}

int
ibcs2_sumount(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'sumount'\n");
	return unmount(p, args, retval);
}

int
ibcs2_symlink(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'symlink'\n");
	return symlink(p, args, retval);
}

int
ibcs2_sync(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'sync'\n");
	return sync(p, args, retval);
}

int
ibcs2_sysacct(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'sysacct'\n");
	return acct(p, args, retval);
}

struct ibcs2_tms {
	long	tms_utime;
	long	tms_stime;
	long	tms_cutime;
	long	tms_cstime;
};

int
ibcs2_times(struct proc *p, struct ibcs2_tms *args, int *retval)
{
	extern int hz;
	struct timeval tv;
	struct ibcs2_tms tms;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'times'\n");
	tms.tms_utime = p->p_uticks;
	tms.tms_stime = p->p_sticks;
	tms.tms_cutime = p->p_stats->p_cru.ru_utime.tv_sec * hz +
			((p->p_stats->p_cru.ru_utime.tv_usec * hz)/1000000);
	tms.tms_cstime = p->p_stats->p_cru.ru_stime.tv_sec * hz +
			((p->p_stats->p_cru.ru_stime.tv_usec * hz)/1000000);
	microtime(&tv);
	*retval = tv.tv_sec * hz + (tv.tv_usec * hz)/1000000;
	return (copyout((caddr_t)&tms,
				    (caddr_t)args->tms_utime,
				    sizeof(struct ibcs2_tms)));
}

struct ibcs2_ulimit_args {
	int cmd;
	long limit;
};

int
ibcs2_ulimit(struct proc *p, struct ibcs2_ulimit_args *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'ulimit'\n");
	switch (args->cmd) {
	case IBCS2_GETFSIZE:
		*retval = p->p_rlimit[RLIMIT_FSIZE].rlim_cur;
		return 0;

	case IBCS2_SETFSIZE:
		return EINVAL;

	case IBCS2_GETPSIZE:
		*retval = p->p_rlimit[RLIMIT_RSS].rlim_cur;
		return 0;
	case IBCS2_GETMOPEN:
		*retval = p->p_rlimit[RLIMIT_NOFILE].rlim_cur;
		return 0;
	}
	return EINVAL;
}

int
ibcs2_umask(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'umask'\n");
	return umask(p, args, retval);
}

int
ibcs2_unlink(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'unlink'\n");
	return unlink(p, args, retval);
}

struct ibcs2_utime_args {
	char		*fname;
	ibcs2_time_t	*timeptr;
};

int
ibcs2_utime(struct proc *p, struct ibcs2_utime_args *args, int *retval)
{
	struct bsd_utimes_args {
		char	*fname;
		struct	timeval *tptr;
	} bsdutimes;
	struct timeval tv;

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'utime'\n");
	tv.tv_sec = (long)args->timeptr;
	tv.tv_usec = 0;
	bsdutimes.tptr = &tv;
	bsdutimes.fname = args->fname;
	return utimes(p, &bsdutimes, retval);
}

struct ibcs2_utssys_args {
	char *buf;
	int mv;
	int cmd;
};

int
ibcs2_utssys(struct proc *p, struct ibcs2_utssys_args *args, int *retval)
{
	struct ibcs2_utsname {
		char	sysname[9];
		char	nodename[9];
		char	release[9];
		char	version[9];
		char	machine[9];
	} ibcs2_uname;
	extern char ostype[], hostname[], osrelease[], machine[];

	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'utssys' cmd=%d\n", args->cmd);
	switch(args->cmd) {
	case 0:	/* uname */
		bzero(&ibcs2_uname, sizeof(struct ibcs2_utsname));
		strncpy(ibcs2_uname.sysname, ostype, 8);
		strncpy(ibcs2_uname.nodename, hostname, 8);
		strncpy(ibcs2_uname.release, osrelease, 8);
		strncpy(ibcs2_uname.version, version, 8);
		strncpy(ibcs2_uname.machine, machine, 8);
		return (copyout((caddr_t)&ibcs2_uname,
				    (caddr_t)args->buf,
				    sizeof(struct ibcs2_utsname)));

	case 2: /* ustat */
		printf("IBCS2: utssys(ustat) not implemented yet\n");
		return EINVAL;

	case 1: /* umask, obsolete */
	default:
		printf("IBCS2: 'utssys cmd (%d) not implemented yet'\n",
			args->cmd);
		return EINVAL;
	}
}

int
ibcs2_wait(struct proc *p, void *args, int *retval)
{
  	struct trapframe *tf = (struct trapframe *)p->p_md.md_regs;
    	struct ibcs2_waitpid_args {
      		int pid;
      		int *status;
      		int options;
    	} *t = args;
    	struct wait4_args {
      		int pid;
      		int *status;
      		int options;
      		struct rusage *rusage;
      		int compat;
    	} tmp;

	tmp.compat = 1;
	tmp.rusage = 0;
	if (ibcs2_trace & IBCS2_TRACE_MISC) 
		printf("IBCS2: 'wait'\n");

  	if ((tf->tf_eflags & (PSL_Z|PSL_PF|PSL_N|PSL_V))
      	    == (PSL_Z|PSL_PF|PSL_N|PSL_V)) {
    		tmp.pid = t->pid;
    		tmp.status = t->status;
		tmp.options = 0;
		if (t->options & 02)
		  	tmp.options |= WUNTRACED;
		if (t->options & 01)
		  	tmp.options |= WNOHANG;
    		tmp.options = t->options;
  	} else {
		tmp.pid = WAIT_ANY;
		tmp.status = (int*)t->pid;
		tmp.options = 0;
	}
	return wait1(p, &tmp, retval);
}
