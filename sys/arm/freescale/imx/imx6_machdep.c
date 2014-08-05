/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
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

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/reboot.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/devmap.h>
#include <machine/machdep.h>
#include <machine/platform.h> 

#include <arm/arm/mpcore_timervar.h>
#include <arm/freescale/imx/imx6_anatopreg.h>
#include <arm/freescale/imx/imx6_anatopvar.h>
#include <arm/freescale/imx/imx_machdep.h>

vm_offset_t
platform_lastaddr(void)
{

	return (arm_devmap_lastaddr());
}

void
platform_probe_and_attach(void)
{

	/* Inform the MPCore timer driver that its clock is variable. */
	arm_tmr_change_frequency(ARM_TMR_FREQUENCY_VARIES);
}

void
platform_gpio_init(void)
{

}

void
platform_late_init(void)
{

}

/*
 * Set up static device mappings.
 *
 * This attempts to cover the most-used devices with 1MB section mappings, which
 * is good for performance (uses fewer TLB entries for device access).
 *
 * ARMMP covers the interrupt controller, MPCore timers, global timer, and the
 * L2 cache controller.  Most of the 1MB range is unused reserved space.
 *
 * AIPS1/AIPS2 cover most of the on-chip devices such as uart, spi, i2c, etc.
 *
 * Notably not mapped right now are HDMI, GPU, and other devices below ARMMP in
 * the memory map.  When we get support for graphics it might make sense to
 * static map some of that area.  Be careful with other things in that area such
 * as OCRAM that probably shouldn't be mapped as PTE_DEVICE memory.
 */
int
platform_devmap_init(void)
{
	const uint32_t IMX6_ARMMP_PHYS = 0x00a00000;
	const uint32_t IMX6_ARMMP_SIZE = 0x00100000;
	const uint32_t IMX6_AIPS1_PHYS = 0x02000000;
	const uint32_t IMX6_AIPS1_SIZE = 0x00100000;
	const uint32_t IMX6_AIPS2_PHYS = 0x02100000;
	const uint32_t IMX6_AIPS2_SIZE = 0x00100000;

	arm_devmap_add_entry(IMX6_ARMMP_PHYS, IMX6_ARMMP_SIZE);
	arm_devmap_add_entry(IMX6_AIPS1_PHYS, IMX6_AIPS1_SIZE);
	arm_devmap_add_entry(IMX6_AIPS2_PHYS, IMX6_AIPS2_SIZE);

	return (0);
}

void
cpu_reset(void)
{
	const uint32_t IMX6_WDOG_CR_PHYS = 0x020bc000;

	imx_wdog_cpu_reset(IMX6_WDOG_CR_PHYS);
}

/*
 * Determine what flavor of imx6 we're running on.
 *
 * This code is based on the way u-boot does it.  Information found on the web
 * indicates that Freescale themselves were the original source of this logic,
 * including the strange check for number of CPUs in the SCU configuration
 * register, which is apparently needed on some revisions of the SOLO.
 *
 * According to the documentation, there is such a thing as an i.MX6 Dual
 * (non-lite flavor).  However, Freescale doesn't seem to have assigned it a
 * number or provided any logic to handle it in their detection code.
 *
 * Note that the ANALOG_DIGPROG and SCU configuration registers are not
 * documented in the chip reference manual.  (SCU configuration is mentioned,
 * but not mapped out in detail.)  I think the bottom two bits of the scu config
 * register may be ncpu-1.
 *
 * This hasn't been tested yet on a dual[-lite].
 *
 * On a solo:
 *      digprog    = 0x00610001
 *      hwsoc      = 0x00000062
 *      scu config = 0x00000500
 * On a quad:
 *      digprog    = 0x00630002
 *      hwsoc      = 0x00000063
 *      scu config = 0x00005503
 */
u_int imx_soc_type()
{
	uint32_t digprog, hwsoc;
	uint32_t *pcr;
	static u_int soctype;
	const vm_offset_t SCU_CONFIG_PHYSADDR = 0x00a00004;
#define	HWSOC_MX6SL	0x60
#define	HWSOC_MX6DL	0x61
#define	HWSOC_MX6SOLO	0x62
#define	HWSOC_MX6Q	0x63

	if (soctype != 0)
		return (soctype);

	digprog = imx6_anatop_read_4(IMX6_ANALOG_DIGPROG_SL);
	hwsoc = (digprog >> IMX6_ANALOG_DIGPROG_SOCTYPE_SHIFT) & 
	    IMX6_ANALOG_DIGPROG_SOCTYPE_MASK;

	if (hwsoc != HWSOC_MX6SL) {
		digprog = imx6_anatop_read_4(IMX6_ANALOG_DIGPROG);
		hwsoc = (digprog & IMX6_ANALOG_DIGPROG_SOCTYPE_MASK) >>
		    IMX6_ANALOG_DIGPROG_SOCTYPE_SHIFT;
		/*printf("digprog = 0x%08x\n", digprog);*/
		if (hwsoc == HWSOC_MX6DL) {
			pcr = arm_devmap_ptov(SCU_CONFIG_PHYSADDR, 4);
			if (pcr != NULL) {
				/*printf("scu config = 0x%08x\n", *pcr);*/
				if ((*pcr & 0x03) == 0) {
					hwsoc = HWSOC_MX6SOLO;
				}
			}
		}
	}
	/* printf("hwsoc 0x%08x\n", hwsoc); */

	switch (hwsoc) {
	case HWSOC_MX6SL:
		soctype = IMXSOC_6SL;
		break;
	case HWSOC_MX6SOLO:
		soctype = IMXSOC_6S;
		break;
	case HWSOC_MX6DL:
		soctype = IMXSOC_6DL;
		break;
	case HWSOC_MX6Q :
		soctype = IMXSOC_6Q;
		break;
	default:
		printf("imx_soc_type: Don't understand hwsoc 0x%02x, "
		    "digprog 0x%08x; assuming IMXSOC_6Q\n", hwsoc, digprog);
		soctype = IMXSOC_6Q;
		break;
	}

	return (soctype);
}

/*
 * Early putc routine for EARLY_PRINTF support.  To use, add to kernel config:
 *   option SOCDEV_PA=0x02000000
 *   option SOCDEV_VA=0x02000000
 *   option EARLY_PRINTF
 * Resist the temptation to change the #if 0 to #ifdef EARLY_PRINTF here. It
 * makes sense now, but if multiple SOCs do that it will make early_putc another
 * duplicate symbol to be eliminated on the path to a generic kernel.
 */
#if 0 
static void 
imx6_early_putc(int c)
{
	volatile uint32_t * UART_STAT_REG = (uint32_t *)0x02020098;
	volatile uint32_t * UART_TX_REG   = (uint32_t *)0x02020040;
	const uint32_t      UART_TXRDY    = (1 << 3);

	while ((*UART_STAT_REG & UART_TXRDY) == 0)
		continue;
	*UART_TX_REG = c;
}
early_putc_t *early_putc = imx6_early_putc;
#endif

