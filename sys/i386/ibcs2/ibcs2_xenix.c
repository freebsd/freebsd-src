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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h> 
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/errno.h>
#include <sys/timeb.h>
#include <sys/unistd.h>

#include <vm/vm.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>

#include <i386/ibcs2/ibcs2_types.h>
#include <i386/ibcs2/ibcs2_unistd.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_xenix.h>
#include <i386/ibcs2/ibcs2_xenix_syscall.h>

extern struct sysent xenix_sysent[];

int
ibcs2_xenix(struct proc *p, struct ibcs2_xenix_args *uap, int *retval)
{
	struct trapframe *tf = (struct trapframe *)p->p_md.md_regs;
        struct sysent *callp;
        u_int code;             

	code = (tf->tf_eax & 0xff00) >> 8;
	callp = &xenix_sysent[code];

	if(code < IBCS2_XENIX_MAXSYSCALL)
	  return((*callp->sy_call)(p, (void *)uap, retval));
	else
	  return ENOSYS;
}

int
xenix_rdchk(p, uap, retval)
	struct proc *p;
	struct xenix_rdchk_args *uap;
	int *retval;
{
	int error;
	struct ioctl_args sa;
	caddr_t sg = stackgap_init();

	DPRINTF(("IBCS2: 'xenix rdchk'\n"));
	SCARG(&sa, fd) = SCARG(uap, fd);
	SCARG(&sa, com) = FIONREAD;
	SCARG(&sa, data) = stackgap_alloc(&sg, sizeof(int));
	if (error = ioctl(p, &sa, retval))
		return error;
	*retval = (*((int*)SCARG(&sa, data))) ? 1 : 0;
	return 0;
}

int
xenix_chsize(p, uap, retval)
	struct proc *p;
	struct xenix_chsize_args *uap;
	int *retval;
{
	struct ftruncate_args sa;

	DPRINTF(("IBCS2: 'xenix chsize'\n"));
	SCARG(&sa, fd) = SCARG(uap, fd);
	SCARG(&sa, pad) = 0;
	SCARG(&sa, length) = SCARG(uap, size);
	return ftruncate(p, &sa, retval);
}


int
xenix_ftime(p, uap, retval)
	struct proc *p;
	struct xenix_ftime_args *uap;
	int *retval;
{
	struct timeval tv;
	struct timeb itb;

	DPRINTF(("IBCS2: 'xenix ftime'\n"));
	microtime(&tv);
	itb.time = tv.tv_sec;
	itb.millitm = (tv.tv_usec / 1000);
	itb.timezone = tz.tz_minuteswest;
	itb.dstflag = tz.tz_dsttime != DST_NONE;

	return copyout((caddr_t)&itb, (caddr_t)SCARG(uap, tp),
		       sizeof(struct timeb));
}

int
xenix_nap(struct proc *p, struct xenix_nap_args *uap, int *retval)
{
	long period;

	DPRINTF(("IBCS2: 'xenix nap %d ms'\n", SCARG(uap, millisec)));
	period = (long)SCARG(uap, millisec) / (1000/hz);
	if (period)
		while (tsleep(&period, PUSER, "nap", period) 
		       != EWOULDBLOCK) ;
	return 0;
}

int
xenix_utsname(struct proc *p, struct xenix_utsname_args *uap, int *retval)
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
	return copyout((caddr_t)&ibcs2_sco_uname, (caddr_t)uap->addr,
		       sizeof(struct ibcs2_sco_utsname));
}

int
xenix_scoinfo(struct proc *p, struct xenix_scoinfo_args *uap, int *retval)
{
  /* scoinfo (not documented) */
  *retval = 0;
  return 0;
}

int     
xenix_eaccess(struct proc *p, struct xenix_eaccess_args *uap, int *retval)
{
	struct ucred *cred = p->p_ucred;
	struct vnode *vp;
        struct nameidata nd;
        int error, flags;
	caddr_t sg = stackgap_init();

	CHECKALTEXIST(p, &sg, SCARG(uap, path));

        NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
            SCARG(uap, path), p);
        if (error = namei(&nd))
                return error;
        vp = nd.ni_vp;

        /* Flags == 0 means only check for existence. */
        if (SCARG(uap, flags)) {
                flags = 0;
                if (SCARG(uap, flags) & IBCS2_R_OK)
                        flags |= VREAD;
                if (SCARG(uap, flags) & IBCS2_W_OK)
                        flags |= VWRITE;
                if (SCARG(uap, flags) & IBCS2_X_OK)
                        flags |= VEXEC;
                if ((flags & VWRITE) == 0 || (error = vn_writechk(vp)) == 0)
                        error = VOP_ACCESS(vp, flags, cred, p);
        }
        vput(vp);
        return error;
}
