/*-
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1994 Sean Eric Fagan
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
 *	$Id: ibcs2_isc.c,v 1.1 1994/10/14 08:53:05 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/syslimits.h>
#include <sys/timeb.h>
#include <sys/unistd.h>
#include <sys/utsname.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>

int
ibcs2_cisc(struct proc *p, void *args, int *retval)
{
	struct trapframe *tf = (struct trapframe *)p->p_md.md_regs;
	
	switch ((tf->tf_eax & 0xffffff00) >> 8) {

	case 0x00:
		printf("IBCS2: 'cisc #0' what is this ??\n");
		return 0;

	case 0x02:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc rename'\n");
	      	return rename(p, args, retval);

	case 0x03:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc sigaction'\n");
	      	return ibcs2_sigaction(p, args, retval);

	case 0x04:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc sigprocmask'\n");
	      	return ibcs2_sigprocmask(p, args, retval);

	case 0x05:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc sigpending'\n");
	      	return ibcs2_sigpending(p, args, retval);

	case 0x06:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc getgroups'\n");
	      	return getgroups(p, args, retval);

	case 0x07:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc setgroups'\n");
	      	return setgroups(p, args, retval);

	case 0x08:	/* pathconf */
	case 0x09:	/* fpathconf */
		if (ibcs2_trace & IBCS2_TRACE_ISC) 
			printf("IBCS2: 'cisc (f)pathconf'"); 
	      	return ibcs2_pathconf(p, args, retval);

	case 0x10: {	/* sysconf */
	    	struct ibcs2_sysconf_args {
	      		int num;
	    	} *sysconf_args = args;

		if (ibcs2_trace & IBCS2_TRACE_ISC) 
			printf("IBCS2: 'cisc sysconf'"); 
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

	case 0x0b:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc waitpid'\n");
	      	return ibcs2_wait(p, args, retval);

	case 0x0c:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc setsid'\n");
	      	return setsid(p, args, retval);

	case 0x0d:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc setpgid'\n");
	      	return setpgid(p, args, retval);

	case 0x11:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc sigsuspend'\n");
	      	return ibcs2_sigsuspend(p, args, retval);

	case 0x12:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc symlink'\n");
	      	return symlink(p, args, retval);

	case 0x13:
		if (ibcs2_trace & IBCS2_TRACE_ISC)
			printf("IBCS2: 'cisc readlink'\n");
	      	return readlink(p, args, retval);

	/* Here needs more work to be done */
	case 0x01:
		printf("IBCS2: 'cisc setostype'");
		break;
	case 0x0e:
		printf("IBCS2: 'cisc adduser'");
		break;
	case 0x0f:
		printf("IBCS2: 'cisc setuser'");
		break;
	case 0x14:
		printf("IBCS2: 'cisc getmajor'");
		break;
	default:
		printf("IBCS2: 'cisc' function %d(0x%x)", 
			tf->tf_eax>>8, tf->tf_eax>>8);
		break;
	}
	printf(" not implemented yet\n");
	return EINVAL;
}
