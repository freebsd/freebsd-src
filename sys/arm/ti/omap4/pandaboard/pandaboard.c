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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>

#include <arm/ti/omapvar.h>
#include <arm/ti/omap4/omap4var.h>
#include <arm/ti/omap4/omap44xx_reg.h>

/* Registers in the SCRM that control the AUX clocks */
#define SCRM_ALTCLKSRC			     (OMAP44XX_SCRM_VBASE + 0x110)
#define SCRM_AUXCLK0                         (OMAP44XX_SCRM_VBASE + 0x0310)
#define SCRM_AUXCLK1                         (OMAP44XX_SCRM_VBASE + 0x0314)
#define SCRM_AUXCLK2                         (OMAP44XX_SCRM_VBASE + 0x0318)
#define SCRM_AUXCLK3                         (OMAP44XX_SCRM_VBASE + 0x031C)

/* Some of the GPIO register set */
#define GPIO1_OE                             (OMAP44XX_GPIO1_VBASE + 0x0134)
#define GPIO1_CLEARDATAOUT                   (OMAP44XX_GPIO1_VBASE + 0x0190)
#define GPIO1_SETDATAOUT                     (OMAP44XX_GPIO1_VBASE + 0x0194)
#define GPIO2_OE                             (OMAP44XX_GPIO2_VBASE + 0x0134)
#define GPIO2_CLEARDATAOUT                   (OMAP44XX_GPIO2_VBASE + 0x0190)
#define GPIO2_SETDATAOUT                     (OMAP44XX_GPIO2_VBASE + 0x0194)

/* Some of the PADCONF register set */
#define CONTROL_WKUP_PAD0_FREF_CLK3_OUT  (OMAP44XX_SCM_PADCONF_VBASE + 0x058)
#define CONTROL_CORE_PAD1_KPD_COL2       (OMAP44XX_SCM_PADCONF_VBASE + 0x186)
#define CONTROL_CORE_PAD0_GPMC_WAIT1     (OMAP44XX_SCM_PADCONF_VBASE + 0x08C)

#define REG_WRITE32(r, x)    *((volatile uint32_t*)(r)) = (uint32_t)(x)
#define REG_READ32(r)        *((volatile uint32_t*)(r))

#define REG_WRITE16(r, x)    *((volatile uint16_t*)(r)) = (uint16_t)(x)
#define REG_READ16(r)        *((volatile uint16_t*)(r))

/**
 *	usb_hub_init - initialises and resets the external USB hub
 *	
 *	The USB hub needs to be held in reset while the power is being applied
 *	and the reference clock is enabled at 19.2MHz.  The following is the
 *	layout of the USB hub taken from the Pandaboard reference manual.
 *
 *
 *	   .-------------.         .--------------.         .----------------.
 *	   |  OMAP4430   |         |   USB3320C   |         |    LAN9514     |
 *	   |             |         |              |         | USB Hub / Eth  |
 *	   |         CLK | <------ | CLKOUT       |         |                |
 *	   |         STP | ------> | STP          |         |                |
 *	   |         DIR | <------ | DIR          |         |                |
 *	   |         NXT | <------ | NXT          |         |                |
 *	   |        DAT0 | <-----> | DAT0         |         |                |
 *	   |        DAT1 | <-----> | DAT1      DP | <-----> | DP             |
 *	   |        DAT2 | <-----> | DAT2      DM | <-----> | DM             |
 *	   |        DAT3 | <-----> | DAT3         |         |                |
 *	   |        DAT4 | <-----> | DAT4         |         |                |
 *	   |        DAT5 | <-----> | DAT5         |  +----> | N_RESET        |
 *	   |        DAT6 | <-----> | DAT6         |  |      |                |
 *	   |        DAT7 | <-----> | DAT7         |  |      |                |
 *	   |             |         |              |  |  +-> | VDD33IO        |
 *	   |    AUX_CLK3 | ------> | REFCLK       |  |  +-> | VDD33A         |
 *	   |             |         |              |  |  |   |                |
 *	   |     GPIO_62 | --+---> | RESET        |  |  |   |                |
 *	   |             |   |     |              |  |  |   |                |
 *	   |             |   |     '--------------'  |  |   '----------------'
 *	   |             |   |     .--------------.  |  |
 *	   |             |   '---->| VOLT CONVERT |--'  |
 *	   |             |         '--------------'     |
 *	   |             |                              |
 *	   |             |         .--------------.     |
 *	   |      GPIO_1 | ------> |   TPS73633   |-----'
 *	   |             |         '--------------'
 *	   '-------------'
 *	
 *
 *	RETURNS:
 *	nothing.
 */
static void
usb_hub_init(void)
{


	/* Need to set FREF_CLK3_OUT to 19.2 MHz and pump it out on pin GPIO_WK31.
	 * We know the SYS_CLK is 38.4Mhz and therefore to get the needed 19.2Mhz,
	 * just use a 2x divider and ensure the SYS_CLK is used as the source.
	 */
	//int pouet = REG_READ32(SCRM_AUXCLK3);
	REG_WRITE32(SCRM_AUXCLK3, (1 << 16) |    /* Divider of 2 */
	                          (0 << 1) |     /* Use the SYS_CLK as the source */
	                          (1 << 8));     /* Enable the clock */

#if 0
	REG_WRITE32(SCRM_ALTCLKSRC, (1 << 1) | ( 3 << 2));
#endif
	/* Enable the clock out to the pin (GPIO_WK31). 
	 *   muxmode=fref_clk3_out, pullup/down=disabled, input buffer=disabled,
	 *   wakeup=disabled.
	 */
	REG_WRITE16(CONTROL_WKUP_PAD0_FREF_CLK3_OUT, 0x0000);


	/* Disable the power to the USB hub, drive GPIO1 low */
	REG_WRITE32(GPIO1_OE, REG_READ32(GPIO1_OE) & ~(1UL << 1));
	REG_WRITE32(GPIO1_CLEARDATAOUT, (1UL << 1));
	REG_WRITE16(CONTROL_CORE_PAD1_KPD_COL2, 0x0003);
	
	
	/* Reset the USB PHY and Hub using GPIO_62 */
	REG_WRITE32(GPIO2_OE, REG_READ32(GPIO2_OE) & ~(1UL << 30));
	REG_WRITE32(GPIO2_CLEARDATAOUT, (1UL << 30));
	REG_WRITE16(CONTROL_CORE_PAD0_GPMC_WAIT1, 0x0003);
	DELAY(10);
	REG_WRITE32(GPIO2_SETDATAOUT, (1UL << 30));

	
	/* Enable power to the hub (GPIO_1) */
	REG_WRITE32(GPIO1_SETDATAOUT, (1UL << 1));

}

/**
 *	board_init - initialises the pandaboard
 *	@dummy: ignored
 * 
 *	This function is called before any of the driver are initialised, which is
 *	annoying because it means we can't use the SCM, PRCM and GPIO modules which
 *	would really be useful.
 *
 *	So we don't have:
 *	   - any drivers
 *	   - no interrupts
 *
 *	What we do have:
 *	   - virt/phys mappings from the devmap (see omap44xx.c)
 *	   - 
 *
 *
 *	So we are hamstrung without the useful drivers and we have to go back to
 *	direct register manupulation. Luckly we don't have to do to much, basically
 *	just setup the usb hub/ethernet.
 *
 */
static void
board_init(void *dummy)
{
	/* Initialise the USB phy and hub */
	usb_hub_init();
	
	/*
	 * XXX Board identification e.g. read out from FPGA or similar should
	 * go here
	 */
}

SYSINIT(board_init, SI_SUB_CPU, SI_ORDER_THIRD, board_init, NULL);
