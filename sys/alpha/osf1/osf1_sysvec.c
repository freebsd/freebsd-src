/*-
 * Copyright (c) 1998-1999 Andrew Gallatin
 * All rights reserved.
 *
 * Based heavily on linux_sysvec.c
 * Which is  Copyright (c) 1994-1996 Søren Schmidt
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
 * $FreeBSD: src/sys/alpha/osf1/osf1_sysvec.c,v 1.1 1999/12/14 22:35:33 gallatin Exp $
 */

/* XXX we use functions that might not exist. */
#include "opt_compat.h"

#ifndef COMPAT_43
#error "Unable to compile Osf1-emulator due to missing COMPAT_43 option!"
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/imgact.h>
#include <sys/imgact_aout.h>
#include <sys/imgact_elf.h>
#include <sys/malloc.h>
#include <alpha/osf1/exec_ecoff.h>
#include <alpha/osf1/osf1_signal.h>
#include <alpha/osf1/osf1_syscall.h>
#include <alpha/osf1/osf1_util.h>
#include <alpha/osf1/osf1.h>

int osf1_szsigcode;
extern char sigcode[];
static int osf1_freebsd_fixup __P((long **stack_base,
				   struct image_params *imgp));

struct sysentvec osf1_sysvec = {
	OSF1_SYS_MAXSYSCALL,
	osf1_sysent,
        0x0,
	NSIG,
	bsd_to_osf1_sig,
	0,
	0,
	0,			/* trap-to-signal translation function */
        osf1_freebsd_fixup,	/* fixup */
	osf1_sendsig,
        sigcode,		/* use generic trampoline */
        &osf1_szsigcode,	/* use generic trampoline size */
        0,			/* prepsyscall */
	"OSF/1 ECOFF",
	NULL			/* we don't have an ECOFF coredump function */
	
};

/*
 * Do some magic to setup the stack properly for the osf1 dynamic loader
 * OSF/1 binaries need an auxargs vector describing the name of the
 * executable (must be a full path).
 *
 * If we're executing a dynamic binary, the loader will expect its
 * name, /sbin/loader, to be in the auxargs vectore as well.  
 * Bear in mind that when we execute a dynamic binary, we begin by
 * executing the loader.  The loader then takes care of mapping
 * executable (which is why it needs the full path) 
 * and its requisite shared libs, then it transfers control
 * to the executable after calling set_program_attributes().
 */ 

#define	AUXARGS_ENTRY(pos, id, val) {suword(pos++, id); suword(pos++, val);}

static int
osf1_freebsd_fixup(long **stack_base, struct image_params *imgp)
{
	char *destp;
	int sz;
	long *pos;
	struct ps_strings *arginfo;
	Osf_Auxargs *args;

	args = (Osf_Auxargs *)imgp->auxargs;
	pos = *stack_base + (imgp->argc + imgp->envc + 2);

	arginfo = (struct ps_strings *)PS_STRINGS;

	sz = *(imgp->proc->p_sysent->sv_szsigcode);
	destp =	(caddr_t)arginfo - szsigcode - SPARE_USRSPACE -
		roundup((ARG_MAX - imgp->stringspace), sizeof(char *));

	destp -= imgp->stringspace;

	destp -= strlen(args->executable)+2;
	copyout(args->executable, destp, strlen(args->executable)+1);

	AUXARGS_ENTRY(pos, OSF1_EXEC_NAME, (long)destp);
	if (args->loader) {
	/* the loader seems to want the name here, then it overwrites it with
   	   the FD of the executable.  I have NFC what's going on here.. */
		AUXARGS_ENTRY(pos, OSF1_EXEC_NAME, (long)destp);
		destp-= (strlen("/sbin/loader")+1);
		copyout("/sbin/loader", destp, strlen("/sbin/loader")+1);
		AUXARGS_ENTRY(pos, OSF1_LOADER_NAME, (long)destp);
		AUXARGS_ENTRY(pos, OSF1_LOADER_FLAGS, 0);
	}
	free(imgp->auxargs, M_TEMP);
	imgp->auxargs = NULL;
	(*stack_base)--;
	**stack_base = (long)imgp->argc;
	return 0;
}
