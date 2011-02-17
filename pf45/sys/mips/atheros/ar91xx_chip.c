/*-
 * Copyright (c) 2010 Adrian Chadd
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
#include <machine/cpuregs.h>

#include <mips/sentry5/s5reg.h>

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/reboot.h>
 
#include <vm/vm.h>
#include <vm/vm_page.h>
 
#include <net/ethernet.h>
 
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/hwfunc.h>
#include <machine/md_var.h>
#include <machine/trap.h>
#include <machine/vmparam.h>
 
#include <mips/atheros/ar71xxreg.h>
#include <mips/atheros/ar91xxreg.h>

#include <mips/atheros/ar71xx_cpudef.h>
#include <mips/atheros/ar91xx_chip.h>

static void
ar91xx_chip_detect_mem_size(void)
{
}

static void
ar91xx_chip_detect_sys_frequency(void)
{
        uint32_t pll;
        uint32_t freq;
        uint32_t div;
        
        pll = ATH_READ_REG(AR91XX_PLL_REG_CPU_CONFIG);

        div = ((pll >> AR91XX_PLL_DIV_SHIFT) & AR91XX_PLL_DIV_MASK);
        freq = div * AR91XX_BASE_FREQ;
        
        u_ar71xx_cpu_freq = freq;

        div = ((pll >> AR91XX_DDR_DIV_SHIFT) & AR91XX_DDR_DIV_MASK) + 1;
        u_ar71xx_ddr_freq = freq / div;
        
        div = (((pll >> AR91XX_AHB_DIV_SHIFT) & AR91XX_AHB_DIV_MASK) + 1) * 2;
        u_ar71xx_ahb_freq = u_ar71xx_cpu_freq / div;
}

static void
ar91xx_chip_device_stop(uint32_t mask)
{
        uint32_t reg;
        
        reg = ATH_READ_REG(AR91XX_RESET_REG_RESET_MODULE);
        ATH_WRITE_REG(AR91XX_RESET_REG_RESET_MODULE, reg | mask);
}

static void
ar91xx_chip_device_start(uint32_t mask)
{
        uint32_t reg;
        
        reg = ATH_READ_REG(AR91XX_RESET_REG_RESET_MODULE);
        ATH_WRITE_REG(AR91XX_RESET_REG_RESET_MODULE, reg & ~mask);
}

static int
ar91xx_chip_device_stopped(uint32_t mask)
{
        uint32_t reg;
        
        reg = ATH_READ_REG(AR91XX_RESET_REG_RESET_MODULE);
        return ((reg & mask) == mask);
}

static void
ar91xx_chip_set_pll_ge0(int speed)
{
	uint32_t pll;

	switch(speed) {
		case 10:
			pll = AR91XX_PLL_VAL_10;
			break;
		case 100:
			pll = AR91XX_PLL_VAL_100;
			break;
		case 1000:
			pll = AR91XX_PLL_VAL_1000;
			break;
		default:
			printf("ar91xx_chip_set_pll_ge0: invalid speed %d\n", speed);
			return;
	}
	ar71xx_write_pll(AR91XX_PLL_REG_ETH_CONFIG, AR91XX_PLL_REG_ETH0_INT_CLOCK, pll, AR91XX_ETH0_PLL_SHIFT);
}

static void
ar91xx_chip_set_pll_ge1(int speed)
{
	uint32_t pll;

	switch(speed) {
		case 10:
			pll = AR91XX_PLL_VAL_10;
			break;
		case 100:
			pll = AR91XX_PLL_VAL_100;
			break;
		case 1000:
			pll = AR91XX_PLL_VAL_1000;
			break;
		default:
			printf("ar91xx_chip_set_pll_ge0: invalid speed %d\n", speed);
			return;
	}
	ar71xx_write_pll(AR91XX_PLL_REG_ETH_CONFIG, AR91XX_PLL_REG_ETH1_INT_CLOCK, pll, AR91XX_ETH1_PLL_SHIFT);
}

static void
ar91xx_chip_ddr_flush_ge0(void)
{
	ar71xx_ddr_flush(AR91XX_DDR_REG_FLUSH_GE0);
}

static void
ar91xx_chip_ddr_flush_ge1(void)
{
	ar71xx_ddr_flush(AR91XX_DDR_REG_FLUSH_GE1);
}

static uint32_t
ar91xx_chip_get_eth_pll(unsigned int mac, int speed)
{
        return 0;
}

static void
ar91xx_chip_init_usb_peripheral(void)
{
	ar71xx_device_stop(AR91XX_RST_RESET_MODULE_USBSUS_OVERRIDE);
	DELAY(100);

	ar71xx_device_start(RST_RESET_USB_HOST);
	DELAY(100);

	ar71xx_device_start(RST_RESET_USB_PHY);
	DELAY(100);
}

struct ar71xx_cpu_def ar91xx_chip_def = {
        &ar91xx_chip_detect_mem_size,
        &ar91xx_chip_detect_sys_frequency,
        &ar91xx_chip_device_stop,
        &ar91xx_chip_device_start,
        &ar91xx_chip_device_stopped,
        &ar91xx_chip_set_pll_ge0,
        &ar91xx_chip_set_pll_ge1,
        &ar91xx_chip_ddr_flush_ge0,
        &ar91xx_chip_ddr_flush_ge1,
        &ar91xx_chip_get_eth_pll,
	NULL,
	&ar91xx_chip_init_usb_peripheral,
};
