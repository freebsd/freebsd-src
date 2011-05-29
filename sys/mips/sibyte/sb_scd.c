/*-
 * Copyright (c) 2009 Neelkanth Natu
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/resource.h>
#include <machine/hwfunc.h>

#include "sb_scd.h"

/*
 * We compile a 32-bit kernel to run on the SB-1 processor which is a 64-bit
 * processor. It has some registers that must be accessed using 64-bit load
 * and store instructions.
 *
 * We use the mips_ld() and mips_sd() functions to do this for us.
 */
#define	sb_store64(addr, val)	mips3_sd((uint64_t *)(uintptr_t)(addr), (val))
#define	sb_load64(addr)		mips3_ld((uint64_t *)(uintptr_t)(addr))

/*
 * System Control and Debug (SCD) unit on the Sibyte ZBbus.
 */

/*
 * Extract the value starting at bit position 'b' for 'n' bits from 'x'.
 */
#define	GET_VAL_64(x, b, n)	(((x) >> (b)) & ((1ULL << (n)) - 1))

#define	SYSREV_ADDR		MIPS_PHYS_TO_KSEG1(0x10020000)
#define	SYSREV_NUM_PROCESSORS(x) GET_VAL_64((x), 24, 4)

#define	SYSCFG_ADDR		MIPS_PHYS_TO_KSEG1(0x10020008)
#define SYSCFG_PLLDIV(x)	GET_VAL_64((x), 7, 5)

#define	ZBBUS_CYCLE_COUNT_ADDR	MIPS_PHYS_TO_KSEG1(0x10030000)

#define	INTSRC_MASK_ADDR(cpu)	\
	(MIPS_PHYS_TO_KSEG1(0x10020028) | ((cpu) << 13))

#define	INTSRC_MAP_ADDR(cpu, intsrc)	\
	(MIPS_PHYS_TO_KSEG1(0x10020200) | ((cpu) << 13)) + (intsrc * 8)

#define	MAILBOX_SET_ADDR(cpu)	\
	(MIPS_PHYS_TO_KSEG1(0x100200C8) | ((cpu) << 13))

#define	MAILBOX_CLEAR_ADDR(cpu)	\
	(MIPS_PHYS_TO_KSEG1(0x100200D0) | ((cpu) << 13))

static uint64_t
sb_read_syscfg(void)
{

	return (sb_load64(SYSCFG_ADDR));
}

static void
sb_write_syscfg(uint64_t val)
{
	
	sb_store64(SYSCFG_ADDR, val);
}

uint64_t
sb_zbbus_cycle_count(void)
{

	return (sb_load64(ZBBUS_CYCLE_COUNT_ADDR));
}

uint64_t
sb_cpu_speed(void)
{
	int plldiv;
	const uint64_t MHZ = 1000000;
	
	plldiv = SYSCFG_PLLDIV(sb_read_syscfg());
	if (plldiv == 0) {
		printf("PLL_DIV is 0 - assuming 6 (300MHz).\n");
		plldiv = 6;
	}

	return (plldiv * 50 * MHZ);
}

void
sb_system_reset(void)
{
	uint64_t syscfg;

	const uint64_t SYSTEM_RESET = 1ULL << 60;
	const uint64_t EXT_RESET = 1ULL << 59;
	const uint64_t SOFT_RESET = 1ULL << 58;

	syscfg = sb_read_syscfg();
	syscfg &= ~SOFT_RESET;
	syscfg |= SYSTEM_RESET | EXT_RESET;
	sb_write_syscfg(syscfg);
}

void
sb_disable_intsrc(int cpu, int src)
{
	int regaddr;
	uint64_t val;

	regaddr = INTSRC_MASK_ADDR(cpu);

	val = sb_load64(regaddr);
	val |= 1ULL << src;
	sb_store64(regaddr, val);
}

void
sb_enable_intsrc(int cpu, int src)
{
	int regaddr;
	uint64_t val;

	regaddr = INTSRC_MASK_ADDR(cpu);

	val = sb_load64(regaddr);
	val &= ~(1ULL << src);
	sb_store64(regaddr, val);
}

void
sb_write_intsrc_mask(int cpu, uint64_t val)
{
	int regaddr;

	regaddr = INTSRC_MASK_ADDR(cpu);
	sb_store64(regaddr, val);
}

uint64_t
sb_read_intsrc_mask(int cpu)
{
	int regaddr;
	uint64_t val;

	regaddr = INTSRC_MASK_ADDR(cpu);
	val = sb_load64(regaddr);

	return (val);
}

void
sb_write_intmap(int cpu, int intsrc, int intrnum)
{
	int regaddr;

	regaddr = INTSRC_MAP_ADDR(cpu, intsrc);
	sb_store64(regaddr, intrnum);
}

int
sb_read_intmap(int cpu, int intsrc)
{
	int regaddr;

	regaddr = INTSRC_MAP_ADDR(cpu, intsrc);
	return (sb_load64(regaddr) & 0x7);
}

int
sb_route_intsrc(int intsrc)
{
	int intrnum;

	KASSERT(intsrc >= 0 && intsrc < NUM_INTSRC,
		("Invalid interrupt source number (%d)", intsrc));

	/*
	 * Interrupt 5 is used by sources internal to the CPU (e.g. timer).
	 * Use a deterministic mapping for the remaining sources.
	 */
#ifdef SMP
	KASSERT(platform_ipi_intrnum() == 4,
		("Unexpected interrupt number used for IPI"));
	intrnum = intsrc % 4;
#else
	intrnum = intsrc % 5;
#endif

	return (intrnum);
}

#ifdef SMP
static uint64_t
sb_read_sysrev(void)
{

	return (sb_load64(SYSREV_ADDR));
}

void
sb_set_mailbox(int cpu, uint64_t val)
{
	int regaddr;

	regaddr = MAILBOX_SET_ADDR(cpu);
	sb_store64(regaddr, val);
}

void
sb_clear_mailbox(int cpu, uint64_t val)
{
	int regaddr;

	regaddr = MAILBOX_CLEAR_ADDR(cpu);
	sb_store64(regaddr, val);
}

cpumask_t
platform_cpu_mask(void)
{

	return (~0U >> (32 - SYSREV_NUM_PROCESSORS(sb_read_sysrev())));
}
#endif	/* SMP */

#define	SCD_PHYSADDR	0x10000000
#define	SCD_SIZE	0x00060000

static int
scd_probe(device_t dev)
{

	device_set_desc(dev, "Broadcom/Sibyte System Control and Debug");
	return (0);
}

static int
scd_attach(device_t dev)
{
	int rid;
	struct resource *res;

	if (bootverbose)
		device_printf(dev, "attached.\n");

	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, SCD_PHYSADDR,
				 SCD_PHYSADDR + SCD_SIZE - 1, SCD_SIZE, 0);
	if (res == NULL)
		panic("Cannot allocate resource for system control and debug.");
	
	return (0);
}

static device_method_t scd_methods[] ={
	/* Device interface */
	DEVMETHOD(device_probe,		scd_probe),
	DEVMETHOD(device_attach,	scd_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	{ 0, 0 }
};

static driver_t scd_driver = {
	"scd",
	scd_methods
};

static devclass_t scd_devclass;

DRIVER_MODULE(scd, zbbus, scd_driver, scd_devclass, 0, 0);
