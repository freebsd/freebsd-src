/*
 *  linux/arch/arm/mach-integrator/mm.c
 *
 *  Extra MM routines for the ARM Integrator board
 *
 *  Copyright (C) 1999,2000 Arm Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
 
#include <asm/mach/map.h>

/*
 * Logical      Physical
 * e8000000	40000000	PCI memory
 * ec000000	62000000	PCI config space
 * ed000000	61000000	PCI V3 regs
 * ee000000	60000000	PCI IO
 * ef000000			Cache flush
 * f1000000	10000000	Core module registers
 * f1100000	11000000	System controller registers
 * f1200000	12000000	EBI registers
 * f1300000	13000000	Counter/Timer
 * f1400000	14000000	Interrupt controller
 * f1500000	15000000	RTC
 * f1600000	16000000	UART 0
 * f1700000	17000000	UART 1
 * f1800000	18000000	Keyboard
 * f1900000	19000000	Mouse
 * f1a00000	1a000000	Debug LEDs
 * f1b00000	1b000000	GPIO
 */
 
static struct map_desc integrator_io_desc[] __initdata = {
 { IO_ADDRESS(INTEGRATOR_HDR_BASE),   INTEGRATOR_HDR_BASE,   SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_SC_BASE),    INTEGRATOR_SC_BASE,    SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_EBI_BASE),   INTEGRATOR_EBI_BASE,   SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_CT_BASE),    INTEGRATOR_CT_BASE,    SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_IC_BASE),    INTEGRATOR_IC_BASE,    SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_RTC_BASE),   INTEGRATOR_RTC_BASE,   SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_UART0_BASE), INTEGRATOR_UART0_BASE, SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_UART1_BASE), INTEGRATOR_UART1_BASE, SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_KBD_BASE),   INTEGRATOR_KBD_BASE,   SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_MOUSE_BASE), INTEGRATOR_MOUSE_BASE, SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_DBG_BASE),   INTEGRATOR_DBG_BASE,   SZ_4K     , DOMAIN_IO, 0, 1},
 { IO_ADDRESS(INTEGRATOR_GPIO_BASE),  INTEGRATOR_GPIO_BASE,  SZ_4K     , DOMAIN_IO, 0, 1},
 { PCI_MEMORY_VADDR,                  PHYS_PCI_MEM_BASE,     SZ_16M    , DOMAIN_IO, 0, 1},
 { PCI_CONFIG_VADDR,                  PHYS_PCI_CONFIG_BASE,  SZ_16M    , DOMAIN_IO, 0, 1},
 { PCI_V3_VADDR,                      PHYS_PCI_V3_BASE,      SZ_512K   , DOMAIN_IO, 0, 1},
 { PCI_IO_VADDR,                      PHYS_PCI_IO_BASE,      SZ_64K    , DOMAIN_IO, 0, 1},
 LAST_DESC
};

void __init integrator_map_io(void)
{
	iotable_init(integrator_io_desc);
}
