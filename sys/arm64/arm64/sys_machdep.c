/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysproto.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/pcb.h>
#include <machine/sysarch.h>
#include <machine/vmparam.h>

#include <security/audit/audit.h>

int
sysarch(struct thread *td, struct sysarch_args *uap)
{
	struct arm64_guard_page_args gp_args;
	struct pcb *pcb;
	vm_offset_t eva;
	unsigned long sve_len;
	int error;

	switch (uap->op) {
	case ARM64_GUARD_PAGE:
		error = copyin(uap->parms, &gp_args, sizeof(gp_args));
		if (error != 0)
			return (error);

		/* Only accept canonical addresses, no PAC or TBI */
		if (!ADDR_IS_CANONICAL(gp_args.addr))
			return (EINVAL);

		eva = gp_args.addr + gp_args.len;

		/* Check for a length overflow */
		if (gp_args.addr > eva)
			return (EINVAL);

		/* Check in the correct address space */
		if (eva >= VM_MAX_USER_ADDRESS)
			return (EINVAL);

		/* Nothing to do */
		if (gp_args.len == 0)
			return (0);

		error = pmap_bti_set(vmspace_pmap(td->td_proc->p_vmspace),
		    trunc_page(gp_args.addr), round_page(eva));
		break;
	case ARM64_GET_SVE_VL:
		pcb = td->td_pcb;
		sve_len = pcb->pcb_sve_len;
		error = EINVAL;
		if (sve_len != 0)
			error = copyout(&sve_len, uap->parms, sizeof(sve_len));
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}
