/*-
 * Copyright (c) 1999 Matthew N. Dodd <winter@jurai.net>
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
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <machine/md_var.h>
#include <machine/vm86.h>
#include <machine/pc/bios.h>
#include <machine/cpufunc.h>

#include <dev/mca/mca_busreg.h>
#include <i386/bios/mca_machdep.h>

/* Global MCA bus flag */
int MCA_system = 0;

/* System Configuration Block */
struct sys_config {
	u_int16_t	count;
	u_int8_t	model;
	u_int8_t	submodel;
	u_int8_t	bios_rev;
	u_int8_t	feature;
#define FEATURE_RESV	0x01	/* Reserved				*/
#define FEATURE_MCABUS	0x02	/* MicroChannel Architecture		*/
#define FEATURE_EBDA	0x04	/* Extended BIOS data area allocated	*/
#define FEATURE_WAITEV	0x08	/* Wait for external event is supported	*/
#define FEATURE_KBDINT	0x10	/* Keyboard intercept called by Int 09h	*/
#define FEATURE_RTC	0x20	/* Real-time clock present		*/
#define FEATURE_IC2	0x40	/* Second interrupt chip present	*/
#define FEATURE_DMA3	0x80	/* DMA channel 3 used by hard disk BIOS	*/
	u_int8_t	pad[3];
} __packed;

/* Function Prototypes */
static void bios_mcabus_present	(void *);
SYSINIT(mca_present, SI_SUB_CPU, SI_ORDER_ANY, bios_mcabus_present, NULL);

/* Functions */
static void
bios_mcabus_present(void * dummy)
{
	struct vm86frame	vmf;
	struct sys_config *	scp;
	vm_offset_t		paddr;

	bzero(&vmf, sizeof(struct vm86frame));

	vmf.vmf_ah = 0xc0;
	if (vm86_intcall(0x15, &vmf)) {
		if (bootverbose) {
			printf("BIOS SDT: INT call failed.\n");
		}
		return;
	}

	if ((vmf.vmf_ah != 0) && (vmf.vmf_flags & 0x01)) {
		if (bootverbose) {
			printf("BIOS SDT: Not supported.  Not PS/2?\n");
			printf("BIOS SDT: AH 0x%02x, Flags 0x%04x\n",
				vmf.vmf_ah, vmf.vmf_flags);
		}
		return;
	}

	paddr = vmf.vmf_es;
	paddr = (paddr << 4) + vmf.vmf_bx;
	scp = (struct sys_config *)BIOS_PADDRTOVADDR(paddr);

	if (bootverbose) {
		printf("BIOS SDT: model 0x%02x, submodel 0x%02x, bios_rev 0x%02x\n",
			scp->model, scp->submodel, scp->bios_rev);
		printf("BIOS SDT: features 0x%b\n", scp->feature,
			"\20"
			"\01RESV"
			"\02MCABUS"
			"\03EBDA"
			"\04WAITEV"
			"\05KBDINT"
			"\06RTC"
			"\07IC2"
			"\08DMA3\n");
	}

	MCA_system = ((scp->feature & FEATURE_MCABUS) ? 1 : 0);

	if (MCA_system)
		printf("MicroChannel Architecture System detected.\n");

	return;
}

int
mca_bus_nmi (void)
{
	int	slot;
	int	retval = 0;
	int	pos5 = 0;

	/* Disable motherboard setup */
	outb(MCA_MB_SETUP_REG, MCA_MB_SETUP_DIS);

	/* For each slot */
	for (slot = 0; slot < MCA_MAX_SLOTS; slot++) {

		/* Select the slot */
		outb(MCA_ADAP_SETUP_REG, slot | MCA_ADAP_SET); 
		pos5 = inb(MCA_POS_REG(MCA_POS5));

		/* If Adapter Check is low */
		if ((pos5 & MCA_POS5_CHCK) == 0) {
			retval++;

			/* If Adapter Check Status is available */
			if ((pos5 & MCA_POS5_CHCK_STAT) == 0) {
				printf("MCA NMI: slot %d, POS6=0x%02x, POS7=0x%02x\n",
					slot+1,
					inb( MCA_POS_REG(MCA_POS6) ),
					inb( MCA_POS_REG(MCA_POS7) ));
			} else { 
				printf("MCA NMI: slot %d\n", slot+1);
			}
		}
		/* Disable adapter setup */
		outb(MCA_ADAP_SETUP_REG, MCA_ADAP_SETUP_DIS);
	}

	return (retval);
}
