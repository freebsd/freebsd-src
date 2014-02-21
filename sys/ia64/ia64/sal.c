/*-
 * Copyright (c) 2001 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <machine/efi.h>
#include <machine/intr.h>
#include <machine/md_var.h>
#include <machine/sal.h>
#include <machine/smp.h>

int ia64_ipi_wakeup;

static struct ia64_fdesc sal_fdesc;
static sal_entry_t	fake_sal;

extern u_int64_t	ia64_pal_entry;
sal_entry_t		*ia64_sal_entry = fake_sal;

static struct uuid sal_table = EFI_TABLE_SAL;
static struct sal_system_table *sal_systbl;

static struct ia64_sal_result
fake_sal(u_int64_t a1, u_int64_t a2, u_int64_t a3, u_int64_t a4,
	 u_int64_t a5, u_int64_t a6, u_int64_t a7, u_int64_t a8)
{
	struct ia64_sal_result res;
	res.sal_status = -3;
	res.sal_result[0] = 0;
	res.sal_result[1] = 0;
	res.sal_result[2] = 0;
	return res;
}

void
ia64_sal_init(void)
{
	static int sizes[6] = {
		48, 32, 16, 32, 16, 16
	};
	u_int8_t *p;
	int error, i;

	sal_systbl = efi_get_table(&sal_table);
	if (sal_systbl == NULL)
		return;

	if (bcmp(sal_systbl->sal_signature, SAL_SIGNATURE, 4)) {
		printf("Bad signature for SAL System Table\n");
		return;
	}

	p = (u_int8_t *) (sal_systbl + 1);
	for (i = 0; i < sal_systbl->sal_entry_count; i++) {
		switch (*p) {
		case 0: {
			struct sal_entrypoint_descriptor *dp;

			dp = (struct sal_entrypoint_descriptor*)p;
			ia64_pal_entry = IA64_PHYS_TO_RR7(dp->sale_pal_proc);
			if (bootverbose)
				printf("PAL Proc at 0x%lx\n", ia64_pal_entry);
			sal_fdesc.func = IA64_PHYS_TO_RR7(dp->sale_sal_proc);
			sal_fdesc.gp = IA64_PHYS_TO_RR7(dp->sale_sal_gp);
			if (bootverbose)
				printf("SAL Proc at 0x%lx, GP at 0x%lx\n",
				    sal_fdesc.func, sal_fdesc.gp);
			ia64_sal_entry = (sal_entry_t *) &sal_fdesc;
			break;
		}
		case 5: {
			struct sal_ap_wakeup_descriptor *dp;

			dp = (struct sal_ap_wakeup_descriptor*)p;
			if (dp->sale_mechanism != 0) {
				printf("SAL: unsupported AP wake-up mechanism "
				    "(%d)\n", dp->sale_mechanism);
				break;
			}

			/* Reserve the XIV so that we won't use it. */
			error = ia64_xiv_reserve(dp->sale_vector,
			    IA64_XIV_PLAT, NULL);
			if (error) {
				printf("SAL: invalid AP wake-up XIV (%#lx)\n",
				    dp->sale_vector);
				break;
			}

			ia64_ipi_wakeup = dp->sale_vector;
			if (bootverbose)
				printf("SAL: AP wake-up XIV: %#x\n",
				    ia64_ipi_wakeup);
			break;
		}
		}
		p += sizes[*p];
	}
}
