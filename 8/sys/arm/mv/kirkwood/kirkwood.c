/*-
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#include <sys/bus.h>

#include <machine/bus.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>
#include <arm/mv/mvwin.h>

struct obio_device obio_devices[] = {
	{ "ic", MV_IC_BASE, MV_IC_SIZE,
		{ -1 },
		{ -1 },
		CPU_PM_CTRL_NONE
	},
	{ "timer", MV_TIMERS_BASE, MV_TIMERS_SIZE,
		{ MV_INT_BRIDGE, -1 },
		{ -1 },
		CPU_PM_CTRL_NONE
	},
	{ "rtc", MV_RTC_BASE, MV_RTC_SIZE,
		{ -1 },
		{ -1 },
		CPU_PM_CTRL_NONE
	},
	{ "gpio", MV_GPIO_BASE, MV_GPIO_SIZE,
		{ MV_INT_GPIO7_0, MV_INT_GPIO15_8,
		  MV_INT_GPIO23_16, MV_INT_GPIO31_24, 
		  MV_INT_GPIOHI7_0, MV_INT_GPIOHI15_8,
		  MV_INT_GPIOHI23_16, -1 },
		{ -1 },
		CPU_PM_CTRL_NONE
	},
	{ "uart", MV_UART0_BASE, MV_UART_SIZE,
		{ MV_INT_UART0, -1 },
		{ -1 },
		CPU_PM_CTRL_NONE
	},
	{ "uart", MV_UART1_BASE, MV_UART_SIZE,
		{ MV_INT_UART1, -1 },
		{ -1 },
		CPU_PM_CTRL_NONE
	},
	{ "xor", MV_XOR_BASE, MV_XOR_SIZE,
		{ MV_INT_XOR0_CHAN0, MV_INT_XOR0_CHAN1,
		  MV_INT_XOR1_CHAN0, MV_INT_XOR1_CHAN1,
		  MV_INT_XOR0_ERR, MV_INT_XOR1_ERR,
		  -1 },
		{ -1 },
		CPU_PM_CTRL_XOR0 | CPU_PM_CTRL_XOR1
	},
	{ "ehci", MV_USB0_BASE, MV_USB_SIZE,
		{ MV_INT_USB_BERR, MV_INT_USB_CI, -1 },
		{ -1 },
		CPU_PM_CTRL_USB0
	},
	{ "mge", MV_ETH0_BASE, MV_ETH_SIZE,
		{ MV_INT_GBERX, MV_INT_GBETX, MV_INT_GBEMISC,
		  MV_INT_GBESUM, MV_INT_GBEERR, -1 },
		{ -1 },
		CPU_PM_CTRL_GE0
	},
	{ "twsi", MV_TWSI0_BASE, MV_TWSI_SIZE,
		{ -1 }, { -1 },
		CPU_PM_CTRL_NONE
	},
	{ "sata", MV_SATAHC_BASE, MV_SATAHC_SIZE,
		{ MV_INT_SATA, -1 },
		{ -1 },
		CPU_PM_CTRL_SATA0 | CPU_PM_CTRL_SATA1
	},
	{ NULL, 0, 0, { 0 }, { 0 }, 0 }
};

const struct obio_pci mv_pci_info[] = {
	{ MV_TYPE_PCIE,
		MV_PCIE_BASE, MV_PCIE_SIZE,
		MV_PCIE_IO_BASE, MV_PCIE_IO_SIZE,	4, 0xE0,
		MV_PCIE_MEM_BASE, MV_PCIE_MEM_SIZE,	4, 0xE8,
		NULL, MV_INT_PEX0
	},

	{ 0, 0, 0 }
};

struct resource_spec mv_gpio_res[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ SYS_RES_IRQ,		3,	RF_ACTIVE },
	{ SYS_RES_IRQ,		4,	RF_ACTIVE },
	{ SYS_RES_IRQ,		5,	RF_ACTIVE },
	{ SYS_RES_IRQ,		6,	RF_ACTIVE },
	{ -1, 0 }
};

struct resource_spec mv_xor_res[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ SYS_RES_IRQ,		3,	RF_ACTIVE },
	{ SYS_RES_IRQ,		4,	RF_ACTIVE },
	{ SYS_RES_IRQ,		5,	RF_ACTIVE },
	{ -1, 0 }
};

const struct decode_win cpu_win_tbl[] = {
	/* Device bus BOOT */
	{ 1, 0x0f, MV_DEV_BOOT_PHYS_BASE, MV_DEV_BOOT_SIZE, -1 },

	/* Device bus CS0 */
	{ 1, 0x1e, MV_DEV_CS0_PHYS_BASE, MV_DEV_CS0_SIZE, -1 },

	/* Device bus CS1 */
	{ 1, 0x1d, MV_DEV_CS1_PHYS_BASE, MV_DEV_CS1_SIZE, -1 },

	/* Device bus CS2 */
	{ 1, 0x1b, MV_DEV_CS2_PHYS_BASE, MV_DEV_CS2_SIZE, -1 },

	/* CESA */
	{ 3, 0x00, MV_CESA_SRAM_PHYS_BASE, MV_CESA_SRAM_SIZE, -1 },

};
const struct decode_win *cpu_wins = cpu_win_tbl;
int cpu_wins_no = sizeof(cpu_win_tbl) / sizeof(struct decode_win);

const struct decode_win xor_win_tbl[] = {
	/* PCIE MEM */
	{ 4, 0xE8, MV_PCIE_MEM_PHYS_BASE, MV_PCIE_MEM_SIZE, -1 },
};
const struct decode_win *xor_wins = xor_win_tbl;
int xor_wins_no = sizeof(xor_win_tbl) / sizeof(struct decode_win);

uint32_t
get_tclk(void)
{
	uint32_t dev, rev;

	/*
	 * On Kirkwood TCLK is not configurable and depends on silicon
	 * revision:
	 * - A0 and A1 have TCLK hardcoded to 200 MHz.
	 * - Z0 and others have TCLK hardcoded to 166 MHz.
	 */
	soc_id(&dev, &rev);
	if (dev == MV_DEV_88F6281 && (rev == 2 || rev == 3))
		return (TCLK_200MHZ);

	return (TCLK_166MHZ);
}
