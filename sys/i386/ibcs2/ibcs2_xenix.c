/*-
 * Copyright (c) 1994 Sean Eric Fagan
 * Copyright (c) 1994 Søren Schmidt
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
 *	$Id: ibcs2_xenix.c,v 1.1 1994/10/14 08:53:12 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/syslimits.h>
#include <sys/unistd.h>
#include <sys/timeb.h>
#include <vm/vm.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>

struct ibcs2_sco_chsize_args {
	int fd;
	ibcs2_off_t size;
};

static int
sco_chsize(struct proc *p, struct ibcs2_sco_chsize_args *args, int *retval)
{
	struct ftruncate_args {
		int fd;
		int pad;
		off_t length;
	} tmp;

	if (ibcs2_trace & IBCS2_TRACE_XENIX)
		printf("IBCS2: 'cxenix chsize'\n"); 
	tmp.fd = args->fd;
	tmp.pad = 0;
	tmp.length = args->size;
	return ftruncate(p, &tmp, retval);
}

struct ibcs2_sco_ftime_args {
	struct timeb *tp;
};

static int
sco_ftime(struct proc *p, struct ibcs2_sco_ftime_args *args, int *retval)
{
	struct timeval atv;
	extern struct timezone tz;
	struct timeb tb;

	if (ibcs2_trace & IBCS2_TRACE_XENIX)
		printf("IBCS2: 'cxenix ftime'\n"); 
	microtime(&atv);
	tb.time = atv.tv_sec;
	tb.millitm = atv.tv_usec / 1000;
	tb.timezone = tz.tz_minuteswest;
	tb.dstflag = tz.tz_dsttime != DST_NONE;

	return copyout((caddr_t)&tb, (caddr_t)args->tp, sizeof(struct timeb));
}

struct ibcs2_sco_nap_args {
	long time;
};

static int
sco_nap(struct proc *p, struct ibcs2_sco_nap_args *args, int *retval)
{
	long period;
	extern int hz;

	if (ibcs2_trace & IBCS2_TRACE_XENIX)
		printf("IBCS2: 'cxenix nap %d ms'\n", args->time); 
	period = (long)args->time / (1000/hz);
	if (period)
		while (tsleep(&period, PUSER, "nap", period) 
		       != EWOULDBLOCK) ;
	return 0;
}

struct ibcs2_sco_rdchk_args {
	int fd;
};

static int
sco_rdchk(struct proc *p, struct ibcs2_sco_rdchk_args *args, int *retval)
{
	struct ioctl_arg {
		int fd;
		int cmd;
		caddr_t arg;
	} tmp;
	int error;

	if (ibcs2_trace & IBCS2_TRACE_XENIX)
		printf("IBCS2: 'cxenix rdchk'\n");
	tmp.fd = args->fd;
	tmp.cmd = FIONREAD;
	tmp.arg = (caddr_t)UA_ALLOC();
	error = ioctl(p, &tmp, retval);
	if (!error)
		*retval = *retval <= 0 ? 0 : 1;
  	return error;
}

struct ibcs2_sco_utsname_args {
	long addr;
};

static int
sco_utsname(struct proc *p, struct ibcs2_sco_utsname_args *args, int *retval)
{
	struct ibcs2_sco_utsname {
		char sysname[9];
		char nodename[9];
		char release[16];
		char kernelid[20];
		char machine[9];
		char bustype[9];
		char sysserial[10];
		unsigned short sysorigin;
		unsigned short sysoem;
		char numusers[9];
		unsigned short numcpu;
	} ibcs2_sco_uname;
	extern char ostype[], hostname[], osrelease[], version[], machine[];

	if (ibcs2_trace & IBCS2_TRACE_XENIX) 
		printf("IBCS2: 'cxenix sco_utsname'\n"); 
	bzero(&ibcs2_sco_uname, sizeof(struct ibcs2_sco_utsname));
	strncpy(ibcs2_sco_uname.sysname, ostype, 8);
	strncpy(ibcs2_sco_uname.nodename, hostname, 8);
	strncpy(ibcs2_sco_uname.release, osrelease, 15);
	strncpy(ibcs2_sco_uname.kernelid, version, 19);
	strncpy(ibcs2_sco_uname.machine, machine, 8);
	bcopy("ISA/EISA", ibcs2_sco_uname.bustype, 8);
	bcopy("no charge", ibcs2_sco_uname.sysserial, 9);
	bcopy("unlim", ibcs2_sco_uname.numusers, 8);
	ibcs2_sco_uname.sysorigin = 0xFFFF;
	ibcs2_sco_uname.sysoem = 0xFFFF;
	ibcs2_sco_uname.numcpu = 1;
	return copyout((caddr_t)&ibcs2_sco_uname, (caddr_t)args->addr,
		       sizeof(struct ibcs2_sco_utsname));
}

int
ibcs2_cxenix(struct proc *p, void *args, int *retval)
{
	struct trapframe *tf = (struct trapframe *)p->p_md.md_regs;
	
	switch ((tf->tf_eax & 0xff00) >> 8) {

	case 0x07:	/* rdchk */
		return sco_rdchk(p, args, retval);

	case 0x0a:	/* chsize */
		return sco_chsize(p, args, retval);

	case 0x0b: 	/* ftime */
		return sco_ftime(p, args, retval);

	case 0x0c:	/* nap */
		return sco_nap(p, args, retval);

	case 0x15:	/* scoinfo (not documented) */
		*retval = 0;
		return 0;

	case 0x24:	/* select */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix select'\n"); 
		return select(p, args, retval);

	case 0x25:	/* eaccess */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix eaccess'\n"); 
		return ibcs2_access(p, args, retval);

	case 0x27:	/* sigaction */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix sigaction'\n"); 
	  	return ibcs2_sigaction (p, args, retval);

	case 0x28:	/* sigprocmask */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix sigprocmask'\n"); 
	  	return ibcs2_sigprocmask (p, args, retval);

	case 0x29:	/* sigpending */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix sigpending'\n"); 
	  	return ibcs2_sigpending (p, args, retval);

	case 0x2a:	/* sigsuspend */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix sigsuspend'\n"); 
	  	return ibcs2_sigsuspend (p, args, retval);

	case 0x2b:	/* getgroups */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix getgroups'\n"); 
	      	return ibcs2_getgroups(p, args, retval);

	case 0x2c:	/* setgroups */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix setgroups'\n"); 
	      	return ibcs2_setgroups(p, args, retval);

	case 0x2d: {	/* sysconf */
	    	struct ibcs2_sysconf_args {
	      		int num;
	    	} *sysconf_args = args;

		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix sysconf'"); 
		switch (sysconf_args->num) {
		case 0: 	/* _SC_ARG_MAX */
			*retval = (ARG_MAX);
			break;
		case 1:		/* _SC_CHILD_MAX */
			*retval = (CHILD_MAX); 
			break;
		case 2:		/* _SC_CLK_TCK */
			*retval = (_BSD_CLK_TCK_);
			break;
		case 3:		/* _SC_NGROUPS_MAX */
			*retval = (NGROUPS_MAX);
			break;
		case 4:		/* _SC_OPEN_MAX */
			*retval = (OPEN_MAX);
			break;
		case 5:		/* _SC_JOB_CONTROL */
#ifdef _POSIX_JOB_CONTORL
			*retval = _POSIX_JOB_CONTORL;
#else
			*retval = (0);
#endif
			break;
		case 6:		/* _SC_SAVED_IDS */
#ifdef _POSIX_SAVED_IDS
			*retval = (_POSIX_SAVED_IDS);
#else
			*retval = (0);
#endif
			break;
		case 7:		/* _SC_VERSION */
			*retval = (_POSIX_VERSION);
			break;
		default:
			*retval = -1;
	      		return EINVAL;
		}
	      	return 0;
	}

	case 0x2e:	/* pathconf */
	case 0x2f:	/* fpathconf */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix (f)pathconf'\n"); 
	      	return ibcs2_pathconf(p, args, retval);

	case 0x30:	/* rename */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix rename'\n"); 
	      	return ibcs2_rename(p, args, retval);

	case 0x32: 	/* sco_utsname */
		return sco_utsname(p, args, retval);

	case 0x37:	/* getitimer */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix getitimer'\n"); 
	      	return getitimer(p, args, retval);
		
	case 0x38:	/* setitimer */
		if (ibcs2_trace & IBCS2_TRACE_XENIX) 
			printf("IBCS2: 'cxenix setitimer'\n"); 
	      	return setitimer(p, args, retval);


	/* Not implemented yet SORRY */
	case 0x01:	/* xlocking */
		printf("IBCS2: 'cxenix xlocking'"); 
		break;
	case 0x02:	/* creatsem */
		printf("IBCS2: 'cxenix creatsem'"); 
		break;
	case 0x03:	/* opensem */
		printf("IBCS2: 'cxenix opensem'"); 
		break;
	case 0x04:	/* sigsem */
		printf("IBCS2: 'cxenix sigsem'"); 
		break;
	case 0x05:	/* waitsem */
		printf("IBCS2: 'cxenix waitsem'"); 
		break;
	case 0x06:	/* nbwaitsem */
		printf("IBCS2: 'cxenix nbwaitsem'"); 
		break;
	case 0x0d:	/* sdget */
		printf("IBCS2: 'cxenix sdget'"); 
		break;
	case 0x0e:	/* sdfree */
		printf("IBCS2: 'cxenix sdfree'"); 
		break;
	case 0x0f:	/* sdenter */
		printf("IBCS2: 'cxenix sdenter'"); 
		break;
	case 0x10:	/* sdleave */
		printf("IBCS2: 'cxenix sdleave'"); 
		break;
	case 0x11:	/* sdgetv */
		printf("IBCS2: 'cxenix sdgetv'"); 
		break;
	case 0x12:	/* sdwaitv */
		printf("IBCS2: 'cxenix sdwaitv'"); 
		break;
	case 0x20:	/* proctl */
		printf("IBCS2: 'cxenix proctl'"); 
		break;
	case 0x21:	/* execseg */
		printf("IBCS2: 'cxenix execseg'"); 
		break;
	case 0x22:	/* unexecseg */
		printf("IBCS2: 'cxenix unexecseg'"); 
		break;
	case 0x26:	/* paccess */
		printf("IBCS2: 'cxenix paccess'"); 
		break;
	default:
		printf("IBCS2: 'cxenix' function %d(0x%x)", 
			tf->tf_eax>>8, tf->tf_eax>>8);
		break;
	}
	printf(" not implemented yet\n");
	return EINVAL;
}
