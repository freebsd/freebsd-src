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
__FBSDID("$FreeBSD: src/sys/ia64/ia64/sal.c,v 1.15.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <machine/efi.h>
#include <machine/md_var.h>
#include <machine/sal.h>
#include <machine/smp.h>

/*
 * IPIs are used more genericly than only
 * for inter-processor interrupts. Don't
 * make it a SMP specific thing...
 */
int ipi_vector[IPI_COUNT];

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

static void
setup_ipi_vectors(int ceil)
{
	int ipi;

	ipi_vector[IPI_MCA_RENDEZ] = ceil - 0x10;

	ipi = IPI_AST;		/* First generic IPI. */
	ceil -= 0x20;		/* First vector in group. */
	while (ipi < IPI_COUNT)
		ipi_vector[ipi++] = ceil++;

	ipi_vector[IPI_HIGH_FP] = ceil - 0x30;
	ipi_vector[IPI_MCA_CMCV] = ceil - 0x30 + 1;
	ipi_vector[IPI_TEST] = ceil - 0x30 + 2;
}

void
ia64_sal_init(void)
{
	static int sizes[6] = {
		48, 32, 16, 32, 16, 16
	};
	u_int8_t *p;
	int i;

	sal_systbl = efi_get_table(&sal_table);
	if (sal_systbl == NULL)
		return;

	if (memcmp(sal_systbl->sal_signature, SAL_SIGNATURE, 4)) {
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
#ifdef SMP
			struct ia64_sal_result result;
			struct ia64_fdesc *fd;
#endif

			dp = (struct sal_ap_wakeup_descriptor*)p;
			if (dp->sale_mechanism != 0) {
				printf("SAL: unsupported AP wake-up mechanism "
				    "(%d)\n", dp->sale_mechanism);
				break;
			}

			if (dp->sale_vector < 0x10 || dp->sale_vector > 0xff) {
				printf("SAL: invalid AP wake-up vector "
				    "(0x%lx)\n", dp->sale_vector);
				break;
			}

			/*
			 * SAL documents that the wake-up vector should be
			 * high (close to 255). The MCA rendezvous vector
			 * should be less than the wake-up vector, but still
			 * "high". We use the following priority assignment:
			 *	Wake-up:	priority of the sale_vector
			 *	Rendezvous:	priority-1
			 *	Generic IPIs:	priority-2
			 *	Special IPIs:	priority-3
			 * Consequently, the wake-up priority should be at
			 * least 4 (ie vector >= 0x40).
			 */
			if (dp->sale_vector < 0x40) {
				printf("SAL: AP wake-up vector too low "
				    "(0x%lx)\n", dp->sale_vector);
				break;
			}

			if (bootverbose)
				printf("SAL: AP wake-up vector: 0x%lx\n",
				    dp->sale_vector);

			ipi_vector[IPI_AP_WAKEUP] = dp->sale_vector;
			setup_ipi_vectors(dp->sale_vector & 0xf0);

#ifdef SMP
			fd = (struct ia64_fdesc *) os_boot_rendez;
			result = ia64_sal_entry(SAL_SET_VECTORS,
			    SAL_OS_BOOT_RENDEZ, ia64_tpa(fd->func),
			    ia64_tpa(fd->gp), 0, 0, 0, 0);
#endif

			break;
		}
		}
		p += sizes[*p];
	}

	if (ipi_vector[IPI_AP_WAKEUP] == 0)
		setup_ipi_vectors(0xf0);
}
