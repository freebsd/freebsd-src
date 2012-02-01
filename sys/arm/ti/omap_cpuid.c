/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <arm/ti/omapvar.h>
#include <arm/ti/omap_cpuid.h>

#include <arm/ti/omap4/omap44xx_reg.h>
#include <arm/ti/omap3/omap35xx_reg.h>

#define OMAP4_STD_FUSE_DIE_ID_0    0x2200 
#define OMAP4_ID_CODE              0x2204
#define OMAP4_STD_FUSE_DIE_ID_1    0x2208 
#define OMAP4_STD_FUSE_DIE_ID_2    0x220C 
#define OMAP4_STD_FUSE_DIE_ID_3    0x2210 
#define OMAP4_STD_FUSE_PROD_ID_0   0x2214 
#define OMAP4_STD_FUSE_PROD_ID_1   0x2218

#define OMAP3_ID_CODE              0xA204

#define REG_READ32(r)              *((volatile uint32_t*)(r))

static uint32_t chip_revision = 0xffffffff;

/**
 *	omap_revision - Returns the revision number of the device
 * 
 *	Simply returns an identifier for the revision of the chip we are running
 *	on.
 *
 *	RETURNS
 *	A 32-bit identifier for the current chip
 */
uint32_t
omap_revision(void)
{
	return chip_revision;
}

/**
 *	omap4_get_revision - determines omap4 revision
 * 
 *	Reads the registers to determine the revision of the chip we are currently
 *	running on.  Stores the information in global variables.
 *
 *
 */
static void
omap4_get_revision(void)
{
	uint32_t id_code;
	uint32_t revision;
	uint32_t hawkeye;

	/* The chip revsion is read from the device identification registers and
	 * the JTAG (?) tap registers, which are located in address 0x4A00_2200 to
	 * 0x4A00_2218.  This is part of the L4_CORE memory range and should have
	 * been mapped in by the machdep.c code.
	 *
	 *   STD_FUSE_DIE_ID_0    0x4A00 2200 
	 *   ID_CODE              0x4A00 2204   (this is the only one we need)
	 *   STD_FUSE_DIE_ID_1    0x4A00 2208 
	 *   STD_FUSE_DIE_ID_2    0x4A00 220C 
	 *   STD_FUSE_DIE_ID_3    0x4A00 2210 
	 *   STD_FUSE_PROD_ID_0   0x4A00 2214 
	 *   STD_FUSE_PROD_ID_1   0x4A00 2218
	 */
	id_code = REG_READ32(OMAP44XX_L4_CORE_VBASE + OMAP4_ID_CODE);
	
	hawkeye = ((id_code >> 12) & 0xffff);
	revision = ((id_code >> 28) & 0xf);
	
	/* Apparently according to the linux code there were some ES2.0 samples that
	 * have the wrong id code and report themselves as ES1.0 silicon.  So used
	 * the ARM cpuid to get the correct revision.
	 */
	if (revision == 0) {
		id_code = cpufunc_id();
		revision = (id_code & 0xf) - 1;
	}
	
	switch (hawkeye) {
	case 0xB852:
		if (revision == 0)
			chip_revision = OMAP4430_REV_ES1_0;
		else
			chip_revision = OMAP4430_REV_ES2_0;
		break;
	case 0xB95C:
		if (revision == 3)
			chip_revision = OMAP4430_REV_ES2_1;
		else if (revision == 4)
			chip_revision = OMAP4430_REV_ES2_2;
		else
			chip_revision = OMAP4430_REV_ES2_3;
		break;
	default:
		/* Default to the latest revision if we can't determine type */
		chip_revision = OMAP4430_REV_ES2_3;
		break;
	}
			
	printf("OMAP%04x ES%u.%u\n", OMAP_REV_DEVICE(chip_revision),
	       OMAP_REV_MAJOR(chip_revision), OMAP_REV_MINOR(chip_revision));
}

/**
 *	omap3_get_revision - determines omap3 revision
 * 
 *	Reads the registers to determine the revision of the chip we are currently
 *	running on.  Stores the information in global variables.
 *
 *	WARNING: This function currently only really works for OMAP3530 devices.
 *	
 *
 *
 */
static void
omap3_get_revision(void)
{
	uint32_t id_code;
	uint32_t revision;
	uint32_t hawkeye;

	/* The chip revsion is read from the device identification registers and
	 * the JTAG (?) tap registers, which are located in address 0x4A00_2200 to
	 * 0x4A00_2218.  This is part of the L4_CORE memory range and should have
	 * been mapped in by the machdep.c code.
	 *
	 *   CONTROL_IDCODE       0x4830 A204   (this is the only one we need)
	 *
	 *
	 */
	id_code = REG_READ32(OMAP35XX_L4_WAKEUP_VBASE + OMAP3_ID_CODE);
	
	hawkeye = ((id_code >> 12) & 0xffff);
	revision = ((id_code >> 28) & 0xf);

	switch (hawkeye) {
	case 0xB6D6:
		chip_revision = OMAP3350_REV_ES1_0;
		break;
	case 0xB7AE:
		if (revision == 1)
			chip_revision = OMAP3530_REV_ES2_0;
		else if (revision == 2)
			chip_revision = OMAP3530_REV_ES2_1;
		else if (revision == 3)
			chip_revision = OMAP3530_REV_ES3_0;
		else if (revision == 4)
			chip_revision = OMAP3530_REV_ES3_1;
		else if (revision == 7)
			chip_revision = OMAP3530_REV_ES3_1_2;
		break;
	default:
		/* Default to the latest revision if we can't determine type */
		chip_revision = OMAP3530_REV_ES3_1_2;
		break;
	}

	printf("OMAP%04x ES%u.%u\n", OMAP_REV_DEVICE(chip_revision),
	       OMAP_REV_MAJOR(chip_revision), OMAP_REV_MINOR(chip_revision));
}

/**
 *	omap_cpu_ident - attempts to identify the chip we are running on
 *	@dummy: ignored
 * 
 *	This function is called before any of the driver are initialised, however
 *	the basic virt to phys maps have been setup in machdep.c so we can still
 *	access the required registers, we just have to use direct register reads
 *	and writes rather than going through the bus stuff.
 *
 *
 */
static void
omap_cpu_ident(void *dummy)
{
	switch(omap_chip()) {
	case CHIP_OMAP_3:
		omap3_get_revision();
		break;
	case CHIP_OMAP_4:
		omap4_get_revision();
		break;
	default:
		panic("Unknown OMAP chip type, fixme!\n");
	}
}

SYSINIT(omap_cpu_ident, SI_SUB_CPU, SI_ORDER_SECOND, omap_cpu_ident, NULL);
