/*-
 * Copyright (c) 1994 Sean Eric Fagan
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1995 Steven Wallace
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
 *    derived from this software without specific prior written permission
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/namei.h> 
#include <sys/sysproto.h>
#include <sys/clock.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/filio.h>
#include <sys/vnode.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/unistd.h>

#include <machine/cpu.h>

#include <i386/ibcs2/ibcs2_types.h>
#include <i386/ibcs2/ibcs2_unistd.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_xenix.h>
#include <i386/ibcs2/ibcs2_xenix_syscall.h>


extern struct sysent xenix_sysent[];

int
ibcs2_xenix(struct thread *td, struct ibcs2_xenix_args *uap)
{
	struct trapframe *tf = td->td_frame;
        struct sysent *callp;
        u_int code;
	int error;

	code = (tf->tf_eax & 0xff00) >> 8;
	callp = &xenix_sysent[code];

	if (code < IBCS2_XENIX_MAXSYSCALL)
		error = ((*callp->sy_call)(td, (void *)uap));
	else
		error = ENOSYS;
	return (error);
}

int
xenix_rdchk(td, uap)
	struct thread *td;
	struct xenix_rdchk_args *uap;
{
	int data, error;

	DPRINTF(("IBCS2: 'xenix rdchk'\n"));
	
	error = kern_ioctl(td, uap->fd, FIONREAD, (caddr_t)&data);
	if (error)
		return (error);
	td->td_retval[0] = data ? 1 : 0;
	return (0);
}

int
xenix_chsize(td, uap)
	struct thread *td;
	struct xenix_chsize_args *uap;
{
	struct ftruncate_args sa;

	DPRINTF(("IBCS2: 'xenix chsize'\n"));
	sa.fd = uap->fd;
	sa.length = uap->size;
	return sys_ftruncate(td, &sa);
}


int
xenix_ftime(td, uap)
	struct thread *td;
	struct xenix_ftime_args *uap;
{
	struct timeval tv;
	struct ibcs2_timeb {
		unsigned long time __packed;
		unsigned short millitm;
		short timezone;
		short dstflag;
	} itb;

	DPRINTF(("IBCS2: 'xenix ftime'\n"));
	microtime(&tv);
	itb.time = tv.tv_sec;
	itb.millitm = (tv.tv_usec / 1000);
	itb.timezone = tz_minuteswest;
	itb.dstflag = tz_dsttime != DST_NONE;

	return copyout((caddr_t)&itb, (caddr_t)uap->tp,
		       sizeof(struct ibcs2_timeb));
}

int
xenix_nap(struct thread *td, struct xenix_nap_args *uap)
{
	long period;

	DPRINTF(("IBCS2: 'xenix nap %d ms'\n", uap->millisec));
	period = (long)uap->millisec / (1000/hz);
	if (period)
		pause("nap", period);
	return 0;
}

int
xenix_utsname(struct thread *td, struct xenix_utsname_args *uap)
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

	DPRINTF(("IBCS2: 'xenix sco_utsname'\n"));
	bzero(&ibcs2_sco_uname, sizeof(struct ibcs2_sco_utsname));
	strncpy(ibcs2_sco_uname.sysname, ostype,
		sizeof(ibcs2_sco_uname.sysname) - 1);
	getcredhostname(td->td_ucred, ibcs2_sco_uname.nodename,
	    sizeof(ibcs2_sco_uname.nodename) - 1);
	strncpy(ibcs2_sco_uname.release, osrelease,
		sizeof(ibcs2_sco_uname.release) - 1);
	strncpy(ibcs2_sco_uname.kernelid, version,
		sizeof(ibcs2_sco_uname.kernelid) - 1);
	strncpy(ibcs2_sco_uname.machine, machine,
		sizeof(ibcs2_sco_uname.machine) - 1);
	strncpy(ibcs2_sco_uname.bustype, "ISA/EISA",
		sizeof(ibcs2_sco_uname.bustype) - 1);
	strncpy(ibcs2_sco_uname.sysserial, "no charge",
		sizeof(ibcs2_sco_uname.sysserial) - 1);
	strncpy(ibcs2_sco_uname.numusers, "unlim",
		sizeof(ibcs2_sco_uname.numusers) - 1);
	ibcs2_sco_uname.sysorigin = 0xFFFF;
	ibcs2_sco_uname.sysoem = 0xFFFF;
	ibcs2_sco_uname.numcpu = 1;
	return copyout((caddr_t)&ibcs2_sco_uname,
		       (caddr_t)(void *)(intptr_t)uap->addr,
		       sizeof(struct ibcs2_sco_utsname));
}

int
xenix_scoinfo(struct thread *td, struct xenix_scoinfo_args *uap)
{
  /* scoinfo (not documented) */
  td->td_retval[0] = 0;
  return 0;
}

int     
xenix_eaccess(struct thread *td, struct xenix_eaccess_args *uap)
{
	char *path;
        int error, bsd_flags;

	bsd_flags = 0;
	if (uap->flags & IBCS2_R_OK)
		bsd_flags |= R_OK;
	if (uap->flags & IBCS2_W_OK)
		bsd_flags |= W_OK;
	if (uap->flags & IBCS2_X_OK)
		bsd_flags |= X_OK;

	CHECKALTEXIST(td, uap->path, &path);
	error = kern_accessat(td, AT_FDCWD, path, UIO_SYSSPACE,
	    AT_EACCESS, bsd_flags);
	free(path, M_TEMP);
        return (error);
}
