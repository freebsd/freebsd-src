/*-
 * Copyright (c) 1995 Steven Wallace
 * All rights reserved.
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
 *      This product includes software developed by Steven Wallace.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
__FBSDID("$FreeBSD: src/sys/i386/ibcs2/ibcs2_sysvec.c,v 1.32 2007/01/17 15:05:52 delphij Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <i386/ibcs2/ibcs2_syscall.h>
#include <i386/ibcs2/ibcs2_signal.h>

MODULE_VERSION(ibcs2, 1);

extern int bsd_to_ibcs2_errno[];
extern struct sysent ibcs2_sysent[IBCS2_SYS_MAXSYSCALL];
extern int szsigcode;
extern char sigcode[];
static int ibcs2_fixup(register_t **, struct image_params *);

struct sysentvec ibcs2_svr3_sysvec = {
        sizeof (ibcs2_sysent) / sizeof (ibcs2_sysent[0]),
        ibcs2_sysent,
        0xFF,
        IBCS2_SIGTBLSZ,
        bsd_to_ibcs2_sig,
        ELAST + 1,
        bsd_to_ibcs2_errno,
	NULL,		/* trap-to-signal translation function */
	ibcs2_fixup,	/* fixup */
	sendsig,
	sigcode,	/* use generic trampoline */
	&szsigcode,	/* use generic trampoline size */
	NULL,		/* prepsyscall */
	"IBCS2 COFF",
	NULL,		/* we don't have a COFF coredump function */
	NULL,
	IBCS2_MINSIGSTKSZ,
	PAGE_SIZE,
	VM_MIN_ADDRESS,
	VM_MAXUSER_ADDRESS,
	USRSTACK,
	PS_STRINGS,
	VM_PROT_ALL,
	exec_copyout_strings,
	exec_setregs,
	NULL
};

static int
ibcs2_fixup(register_t **stack_base, struct image_params *imgp)
{

	return (suword(--(*stack_base), imgp->args->argc));
}

/*
 * Create an "ibcs2" module that does nothing but allow checking for
 * the presence of the subsystem.
 */
static int
ibcs2_modevent(module_t mod, int type, void *unused)
{
	struct proc *p = NULL;
	int rval = 0;

	switch(type) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		/* if this was an ELF module we'd use elf_brand_inuse()... */
		sx_slock(&allproc_lock);
		FOREACH_PROC_IN_SYSTEM(p) {
			if (p->p_sysent == &ibcs2_svr3_sysvec) {
				rval = EBUSY;
				break;
			}
		}
		sx_sunlock(&allproc_lock);
		break;
	default:
	        rval = EOPNOTSUPP;
		break;
	}
	return (rval);
}
static moduledata_t ibcs2_mod = {
	"ibcs2",
	ibcs2_modevent,
	0
};
DECLARE_MODULE(ibcs2, ibcs2_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
