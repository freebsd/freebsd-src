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

#define _MV_PCIE_MAX_PORT	8

#define _MV_PCIE_IO_SIZE	(MV_PCIE_IO_SIZE / _MV_PCIE_MAX_PORT)
#define _MV_PCIE_MEM_SIZE	(MV_PCIE_MEM_SIZE / _MV_PCIE_MAX_PORT)

#define	_MV_PCIE_IO(n)	(MV_PCIE_IO_BASE + ((n) * _MV_PCIE_IO_SIZE))
#define	_MV_PCIE_MEM(n)	(MV_PCIE_MEM_BASE + ((n) * _MV_PCIE_MEM_SIZE))

#define	_MV_PCIE_IO_PHYS(n)	(MV_PCIE_IO_PHYS_BASE + ((n) * _MV_PCIE_IO_SIZE))
#define	_MV_PCIE_MEM_PHYS(n)	(MV_PCIE_MEM_PHYS_BASE + ((n) * _MV_PCIE_MEM_SIZE))

/*
 * Note the 'pcib' devices are not declared in the obio_devices[]: due to the
 * much more complex configuration schemes allowed, specifically of the
 * PCI-Express (multiple lanes width per port configured dynamically etc.) it
 * is better and flexible to instantiate the number of PCI bridge devices
 * (known in run-time) in the pcib_mbus_identify() method.
 */
struct obio_device obio_devices[] = {
	{ "ic", MV_IC_BASE, MV_IC_SIZE,
		{ -1 },
		{ -1 },
		CPU_PM_CTRL_NONE
	},
	{ "timer", MV_TIMERS_BASE, MV_TIMERS_SIZE,
		{ MV_INT_TIMER0, -1 },
		{ -1 },
		CPU_PM_CTRL_NONE
	},
	{ "gpio", MV_GPIO_BASE, MV_GPIO_SIZE,
		{ MV_INT_GPIO7_0, MV_INT_GPIO15_8,
		  MV_INT_GPIO23_16, MV_INT_GPIO31_24, -1 },
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
	{ "idma", MV_IDMA_BASE, MV_IDMA_SIZE,
		{ MV_INT_IDMA_ERR, MV_INT_IDMA0, MV_INT_IDMA1,
		  MV_INT_IDMA2, MV_INT_IDMA3, -1 },
		{ -1 },
		CPU_PM_CTRL_IDMA
	},
	{ "xor", MV_XOR_BASE, MV_XOR_SIZE,
		{ MV_INT_XOR0, MV_INT_XOR1,
		  MV_INT_XOR_ERR, -1 },
		{ -1 },
		CPU_PM_CTRL_XOR
	},
	{ "ehci", MV_USB0_BASE, MV_USB_SIZE,
		{ MV_INT_USB_ERR, MV_INT_USB0, -1 },
		{ -1 },
		CPU_PM_CTRL_USB0 | CPU_PM_CTRL_USB1 | CPU_PM_CTRL_USB2
	},
	{ "mge", MV_ETH0_BASE, MV_ETH_SIZE,
		{ MV_INT_GBERX, MV_INT_GBETX, MV_INT_GBEMISC,
		  MV_INT_GBESUM, MV_INT_GBE_ERR, -1 },
		{ -1 },
		CPU_PM_CTRL_GE0
	},
	{ "mge", MV_ETH1_BASE, MV_ETH_SIZE,
		{ MV_INT_GBE1RX, MV_INT_GBE1TX, MV_INT_GBE1MISC,
		  MV_INT_GBE1SUM, MV_INT_GBE_ERR, -1 },
		{ -1 },
		CPU_PM_CTRL_GE1
	},
	{ "twsi", MV_TWSI_BASE, MV_TWSI_SIZE,
		{ -1 }, { -1 },
		CPU_PM_CTRL_NONE
	},
	{ NULL, 0, 0, { 0 }, { 0 }, 0 }
};

const struct obio_pci mv_pci_info[] = {
	{ MV_TYPE_PCIE,
		MV_PCIE00_BASE, MV_PCIE_SIZE,
		_MV_PCIE_IO(0), _MV_PCIE_IO_SIZE, 4, 0xE0,
		_MV_PCIE_MEM(0), _MV_PCIE_MEM_SIZE, 4, 0xE8,
		NULL, MV_INT_PEX00 },

	{ MV_TYPE_PCIE_AGGR_LANE,
		MV_PCIE01_BASE, MV_PCIE_SIZE,
		_MV_PCIE_IO(1), _MV_PCIE_IO_SIZE, 4, 0xD0,
		_MV_PCIE_MEM(1), _MV_PCIE_MEM_SIZE, 4, 0xD8,
		NULL, MV_INT_PEX01 },
#if 0
	/*
	 * XXX Access to devices on this interface (PCIE 0.2) crashes the
	 * system.  Could be a silicon defect as Marvell U-Boot has a 'Do not
	 * touch' precaution comment...
	 */
	{ MV_TYPE_PCIE_AGGR_LANE,
		MV_PCIE02_BASE, MV_PCIE_SIZE,
		_MV_PCIE_IO(2), _MV_PCIE_IO_SIZE(2), 4, 0xB0,
		_MV_PCIE_MEM(2), _MV_PCIE_MEM_SIZE(2), 4, 0xB8,
		NULL, MV_INT_PEX02 },
#endif
	{ MV_TYPE_PCIE_AGGR_LANE,
		MV_PCIE03_BASE, MV_PCIE_SIZE,
		_MV_PCIE_IO(3), _MV_PCIE_IO_SIZE, 4, 0x70,
		_MV_PCIE_MEM(3), _MV_PCIE_MEM_SIZE, 4, 0x78,
		NULL, MV_INT_PEX03 },

	{ MV_TYPE_PCIE,
		MV_PCIE10_BASE, MV_PCIE_SIZE,
		_MV_PCIE_IO(4), _MV_PCIE_IO_SIZE, 8, 0xE0,
		_MV_PCIE_MEM(4), _MV_PCIE_MEM_SIZE, 8, 0xE8,
		NULL, MV_INT_PEX10 },

	{ MV_TYPE_PCIE_AGGR_LANE,
		MV_PCIE11_BASE, MV_PCIE_SIZE,
		_MV_PCIE_IO(5), _MV_PCIE_IO_SIZE, 8, 0xD0,
		_MV_PCIE_MEM(5), _MV_PCIE_MEM_SIZE, 8, 0xD8,
		NULL, MV_INT_PEX11 },
	{ MV_TYPE_PCIE_AGGR_LANE,
		MV_PCIE12_BASE, MV_PCIE_SIZE,
		_MV_PCIE_IO(6), _MV_PCIE_IO_SIZE, 8, 0xB0,
		_MV_PCIE_MEM(6), _MV_PCIE_MEM_SIZE, 8, 0xB8,
		NULL, MV_INT_PEX12 },
	{ MV_TYPE_PCIE_AGGR_LANE,
		MV_PCIE13_BASE, MV_PCIE_SIZE,
		_MV_PCIE_IO(7), _MV_PCIE_IO_SIZE, 8, 0x70,
		_MV_PCIE_MEM(7), _MV_PCIE_MEM_SIZE, 8, 0x78,
		NULL, MV_INT_PEX13 },

	{ 0, 0, 0 }
};

struct resource_spec mv_gpio_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ SYS_RES_IRQ,		3,	RF_ACTIVE },
	{ -1, 0 }
};

struct resource_spec mv_xor_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ -1, 0 }
};

const struct decode_win cpu_win_tbl[] = {
	/* Device bus BOOT */
	{ 1, 0x2f, MV_DEV_BOOT_PHYS_BASE, MV_DEV_BOOT_SIZE, -1 },

	/* Device bus CS0 */
	{ 1, 0x3e, MV_DEV_CS0_PHYS_BASE, MV_DEV_CS0_SIZE, -1 },

	/* Device bus CS1 */
	{ 1, 0x3d, MV_DEV_CS1_PHYS_BASE, MV_DEV_CS1_SIZE, -1 },

	/* Device bus CS2 */
	{ 1, 0x3b, MV_DEV_CS2_PHYS_BASE, MV_DEV_CS2_SIZE, -1 },
};
const struct decode_win *cpu_wins = cpu_win_tbl;
int cpu_wins_no = sizeof(cpu_win_tbl) / sizeof(struct decode_win);

/*
 * Note: the decode windows table for IDMA does not explicitly have DRAM
 * entries, which are not statically defined: active DDR banks (== windows)
 * are established in run time from actual DDR windows settings. All active
 * DDR banks are mapped into IDMA decode windows, so at least one IDMA decode
 * window is occupied by the DDR bank; in case when all (MV_WIN_DDR_MAX)
 * DDR banks are active, the remaining available IDMA decode windows for other
 * targets is only MV_WIN_IDMA_MAX - MV_WIN_DDR_MAX.
 */
const struct decode_win idma_win_tbl[] = {
	/* PCIE MEM */
	{ 4, 0xE8, _MV_PCIE_MEM_PHYS(0), _MV_PCIE_MEM_SIZE, -1 },
	{ 4, 0xD8, _MV_PCIE_MEM_PHYS(1), _MV_PCIE_MEM_SIZE, -1 },
};
const struct decode_win *idma_wins = idma_win_tbl;
int idma_wins_no = sizeof(idma_win_tbl) / sizeof(struct decode_win);
