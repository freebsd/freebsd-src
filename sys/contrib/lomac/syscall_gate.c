/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: syscall_gate.c,v 1.4 2001/11/05 21:23:12 bfeldman Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysent.h>

#include <machine/frame.h>

#include "syscall_gate.h"
void syscall_gate_init(void);
int syscall_gate(struct thread *td, caddr_t params);

static struct syscall_gate sg;

void
syscall_gate_init(void) {

	sg.sg_table = curthread->td_proc->p_sysent->sv_table;
	bzero(sg.sg_oldsyscalls, sizeof(sg.sg_oldsyscalls));
}

int
syscall_gate_register(int offset, sy_call_t *call, int narg, int mpsafe) {
	struct sysent *se;
	int error = 0;

	if (offset <= 0 || offset >= SYS_MAXSYSCALL) {
		error = EINVAL;
		goto out;
	}
	if (sg.sg_oldsyscalls[offset].sy_call != NULL) {
		error = EEXIST;
		goto out;
	}
	se = &sg.sg_table[offset];
	sg.sg_oldsyscalls[offset] = *se;
	se->sy_call = (sy_call_t *)call;
	se->sy_narg = narg | (mpsafe ? SYF_MPSAFE : 0);
out:
	return (error);
}

void
syscall_gate_deregister(int offset) {
	KASSERT(offset > 0 && offset < SYS_MAXSYSCALL, ("syscall offset %d out of range",
	    offset));
	KASSERT(sg.sg_oldsyscalls[offset].sy_call != NULL, ("deregistering nonexistant syscall %d",
	    offset));
	sg.sg_table[offset] = sg.sg_oldsyscalls[offset];
	sg.sg_oldsyscalls[offset].sy_call = NULL;
	sg.sg_oldsyscalls[offset].sy_narg = 0;
}

static int
syscall_gate_modevent(module_t module, int event, void *unused) {
	int i;

	switch ((enum modeventtype)event) {
	case MOD_LOAD:
		syscall_gate_init();
		break;
	case MOD_UNLOAD:
		for (i = 1; i < SYS_MAXSYSCALL; i++) {
			struct sysent *se;

			se = &sg.sg_oldsyscalls[i];
			if (se->sy_call != NULL)
				sg.sg_table[i] = *se;
		}
		break;
	case MOD_SHUTDOWN:
		break;
	}
	return (0);
}

static moduledata_t syscall_gate_moduledata = {
	"syscall_gate",
	&syscall_gate_modevent,
	NULL
};
DECLARE_MODULE(syscall_gate, syscall_gate_moduledata, SI_SUB_DRIVERS, SI_ORDER_ANY);
MODULE_VERSION(syscall_gate, 1);
