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
#include <sys/kernel.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>
#include <arm/mv/mvwin.h>

#define MAX_CPU_WIN	5

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

#ifdef DEBUG
#define MV_DUMP_WIN	1
#else
#define MV_DUMP_WIN	0
#endif

static int win_eth_can_remap(int i);

static int decode_win_cpu_valid(void);
static int decode_win_usb_valid(void);
static int decode_win_eth_valid(void);
static int decode_win_pcie_valid(void);
static int decode_win_sata_valid(void);
static int decode_win_cesa_valid(void);
static int decode_win_idma_valid(void);
static int decode_win_xor_valid(void);

static void decode_win_cpu_setup(void);
static void decode_win_usb_setup(u_long);
static void decode_win_eth_setup(u_long);
static void decode_win_pcie_setup(u_long);
static void decode_win_sata_setup(u_long);
static void decode_win_cesa_setup(u_long);
static void decode_win_idma_setup(u_long);
static void decode_win_xor_setup(u_long);

static void decode_win_cesa_dump(u_long);
static void decode_win_usb_dump(u_long);
static void decode_win_eth_dump(u_long base);
static void decode_win_idma_dump(u_long base);
static void decode_win_xor_dump(u_long base);

static int fdt_get_ranges(const char *, void *, int, int *, int *);

static int win_cpu_from_dt(void);
static int fdt_win_setup(void);

static uint32_t used_cpu_wins;
static uint32_t dev_mask = 0;
static int cpu_wins_no = 0;
static int eth_port = 0;
static int usb_port = 0;

static struct decode_win cpu_win_tbl[MAX_CPU_WIN];

static const struct decode_win *cpu_wins = cpu_win_tbl;

typedef void (*decode_win_setup_t)(u_long);
typedef void (*dump_win_t)(u_long);

struct soc_node_spec {
	const char		*compat;
	decode_win_setup_t	decode_handler;
	dump_win_t		dump_handler;
};

static struct soc_node_spec soc_nodes[] = {
	{ "mrvl,cesa", &decode_win_cesa_setup, &decode_win_cesa_dump },
	{ "mrvl,ge", &decode_win_eth_setup, &decode_win_eth_dump },
	{ "mrvl,usb-ehci", &decode_win_usb_setup, &decode_win_usb_dump },
	{ "mrvl,sata", &decode_win_sata_setup, NULL },
	{ "mrvl,xor", &decode_win_xor_setup, &decode_win_xor_dump },
	{ "mrvl,idma", &decode_win_idma_setup, &decode_win_idma_dump },
	{ "mvrl,pcie", &decode_win_pcie_setup, NULL },
	{ NULL, NULL, NULL },
};

struct fdt_pm_mask_entry fdt_pm_mask_table[] = {
	{ "mrvl,ge",		CPU_PM_CTRL_GE(0) },
	{ "mrvl,ge",		CPU_PM_CTRL_GE(1) },
	{ "mrvl,usb-ehci",	CPU_PM_CTRL_USB(0) },
	{ "mrvl,usb-ehci",	CPU_PM_CTRL_USB(1) },
	{ "mrvl,usb-ehci",	CPU_PM_CTRL_USB(2) },
	{ "mrvl,cesa",		CPU_PM_CTRL_CRYPTO },
	{ "mrvl,xor",		CPU_PM_CTRL_XOR },
	{ "mrvl,sata",		CPU_PM_CTRL_SATA },

	{ NULL, 0 }
};

static __inline int
pm_is_disabled(uint32_t mask)
{

	return (soc_power_ctrl_get(mask) == mask ? 0 : 1);
}

/*
 * Disable device using power management register.
 * 1 - Device Power On
 * 0 - Device Power Off
 * Mask can be set in loader.
 * EXAMPLE:
 * loader> set hw.pm-disable-mask=0x2
 *
 * Common mask:
 * |-------------------------------|
 * | Device | Kirkwood | Discovery |
 * |-------------------------------|
 * | USB0   | 0x00008  | 0x020000  |
 * |-------------------------------|
 * | USB1   |     -    | 0x040000  |
 * |-------------------------------|
 * | USB2   |     -    | 0x080000  |
 * |-------------------------------|
 * | GE0    | 0x00001  | 0x000002  |
 * |-------------------------------|
 * | GE1    |     -    | 0x000004  |
 * |-------------------------------|
 * | IDMA   |     -    | 0x100000  |
 * |-------------------------------|
 * | XOR    | 0x10000  | 0x200000  |
 * |-------------------------------|
 * | CESA   | 0x20000  | 0x400000  |
 * |-------------------------------|
 * | SATA   | 0x04000  | 0x004000  |
 * --------------------------------|
 * This feature can be used only on Kirkwood and Discovery
 * machines.
 */
static __inline void
pm_disable_device(int mask)
{
#ifdef DIAGNOSTIC
	uint32_t reg;

	reg = soc_power_ctrl_get(CPU_PM_CTRL_ALL);
	printf("Power Management Register: 0%x\n", reg);

	reg &= ~mask;
	soc_power_ctrl_set(reg);
	printf("Device %x is disabled\n", mask);

	reg = soc_power_ctrl_get(CPU_PM_CTRL_ALL);
	printf("Power Management Register: 0%x\n", reg);
#endif
}

int
fdt_pm(phandle_t node)
{
	uint32_t cpu_pm_ctrl;
	int i, ena, compat;

	ena = 1;
	cpu_pm_ctrl = read_cpu_ctrl(CPU_PM_CTRL);
	for (i = 0; fdt_pm_mask_table[i].compat != NULL; i++) {
		if (dev_mask & (1 << i))
			continue;

		compat = fdt_is_compatible(node, fdt_pm_mask_table[i].compat);

		if (compat && (~cpu_pm_ctrl & fdt_pm_mask_table[i].mask)) {
			dev_mask |= (1 << i);
			ena = 0;
			break;
		} else if (compat) {
			dev_mask |= (1 << i);
			break;
		}
	}

	return (ena);
}

uint32_t
read_cpu_ctrl(uint32_t reg)
{

	return (bus_space_read_4(fdtbus_bs_tag, MV_CPU_CONTROL_BASE, reg));
}

void
write_cpu_ctrl(uint32_t reg, uint32_t val)
{

	bus_space_write_4(fdtbus_bs_tag, MV_CPU_CONTROL_BASE, reg, val);
}

void
cpu_reset(void)
{

	write_cpu_ctrl(RSTOUTn_MASK, SOFT_RST_OUT_EN);
	write_cpu_ctrl(SYSTEM_SOFT_RESET, SYS_SOFT_RST);
	while (1);
}

uint32_t
cpu_extra_feat(void)
{
	uint32_t dev, rev;
	uint32_t ef = 0;

	soc_id(&dev, &rev);
	if (dev == MV_DEV_88F6281 || dev == MV_DEV_MV78100_Z0 ||
	    dev == MV_DEV_MV78100)
		__asm __volatile("mrc p15, 1, %0, c15, c1, 0" : "=r" (ef));
	else if (dev == MV_DEV_88F5182 || dev == MV_DEV_88F5281)
		__asm __volatile("mrc p15, 0, %0, c14, c0, 0" : "=r" (ef));
	else if (bootverbose)
		printf("This ARM Core does not support any extra features\n");

	return (ef);
}

/*
 * Get the power status of device. This feature is only supported on
 * Kirkwood and Discovery SoCs.
 */
uint32_t
soc_power_ctrl_get(uint32_t mask)
{

#ifndef SOC_MV_ORION
	if (mask != CPU_PM_CTRL_NONE)
		mask &= read_cpu_ctrl(CPU_PM_CTRL);

	return (mask);
#else
	return (mask);
#endif
}

/*
 * Set the power status of device. This feature is only supported on
 * Kirkwood and Discovery SoCs.
 */
void
soc_power_ctrl_set(uint32_t mask)
{

#ifndef SOC_MV_ORION
	if (mask != CPU_PM_CTRL_NONE)
		write_cpu_ctrl(CPU_PM_CTRL, mask);
#endif
}

void
soc_id(uint32_t *dev, uint32_t *rev)
{

	/*
	 * Notice: system identifiers are available in the registers range of
	 * PCIE controller, so using this function is only allowed (and
	 * possible) after the internal registers range has been mapped in via
	 * pmap_devmap_bootstrap().
	 */
	*dev = bus_space_read_4(fdtbus_bs_tag, MV_PCIE_BASE, 0) >> 16;
	*rev = bus_space_read_4(fdtbus_bs_tag, MV_PCIE_BASE, 8) & 0xff;
}

static void
soc_identify(void)
{
	uint32_t d, r;
	const char *dev;
	const char *rev;

	soc_id(&d, &r);

	printf("SOC: ");
	if (bootverbose)
		printf("(0x%4x:0x%02x) ", d, r);

	rev = "";
	switch (d) {
	case MV_DEV_88F5181:
		dev = "Marvell 88F5181";
		if (r == 3)
			rev = "B1";
		break;
	case MV_DEV_88F5182:
		dev = "Marvell 88F5182";
		if (r == 2)
			rev = "A2";
		break;
	case MV_DEV_88F5281:
		dev = "Marvell 88F5281";
		if (r == 4)
			rev = "D0";
		else if (r == 5)
			rev = "D1";
		else if (r == 6)
			rev = "D2";
		break;
	case MV_DEV_88F6281:
		dev = "Marvell 88F6281";
		if (r == 0)
			rev = "Z0";
		else if (r == 2)
			rev = "A0";
		else if (r == 3)
			rev = "A1";
		break;
	case MV_DEV_MV78100_Z0:
		dev = "Marvell MV78100 Z0";
		break;
	case MV_DEV_MV78100:
		dev = "Marvell MV78100";
		break;
	default:
		dev = "UNKNOWN";
		break;
	}

	printf("%s", dev);
	if (*rev != '\0')
		printf(" rev %s", rev);
	printf(", TClock %dMHz\n", get_tclk() / 1000 / 1000);

	/* TODO add info on currently set endianess */
}

static void
platform_identify(void *dummy)
{

	soc_identify();

	/*
	 * XXX Board identification e.g. read out from FPGA or similar should
	 * go here
	 */
}
SYSINIT(platform_identify, SI_SUB_CPU, SI_ORDER_SECOND, platform_identify,
    NULL);

int
soc_decode_win(void)
{
	uint32_t dev, rev;
	int mask, err;

	mask = 0;
	TUNABLE_INT_FETCH("hw.pm-disable-mask", &mask);

	if (mask != 0)
		pm_disable_device(mask);

	/* Retrieve data about physical addresses from device tree. */
	if ((err = win_cpu_from_dt()) != 0)
		return (err);

	/* Retrieve our ID: some windows facilities vary between SoC models */
	soc_id(&dev, &rev);

	if (!decode_win_cpu_valid() || !decode_win_usb_valid() ||
	    !decode_win_eth_valid() || !decode_win_idma_valid() ||
	    !decode_win_pcie_valid() || !decode_win_sata_valid() ||
	    !decode_win_cesa_valid() || !decode_win_xor_valid())
		return (EINVAL);

	decode_win_cpu_setup();
	if (MV_DUMP_WIN)
		soc_dump_decode_win();

	eth_port = 0;
	usb_port = 0;
	if ((err = fdt_win_setup()) != 0)
		return (err);

	return (0);
}

/**************************************************************************
 * Decode windows registers accessors
 **************************************************************************/
WIN_REG_IDX_RD(win_cpu, cr, MV_WIN_CPU_CTRL, MV_MBUS_BRIDGE_BASE)
WIN_REG_IDX_RD(win_cpu, br, MV_WIN_CPU_BASE, MV_MBUS_BRIDGE_BASE)
WIN_REG_IDX_RD(win_cpu, remap_l, MV_WIN_CPU_REMAP_LO, MV_MBUS_BRIDGE_BASE)
WIN_REG_IDX_RD(win_cpu, remap_h, MV_WIN_CPU_REMAP_HI, MV_MBUS_BRIDGE_BASE)
WIN_REG_IDX_WR(win_cpu, cr, MV_WIN_CPU_CTRL, MV_MBUS_BRIDGE_BASE)
WIN_REG_IDX_WR(win_cpu, br, MV_WIN_CPU_BASE, MV_MBUS_BRIDGE_BASE)
WIN_REG_IDX_WR(win_cpu, remap_l, MV_WIN_CPU_REMAP_LO, MV_MBUS_BRIDGE_BASE)
WIN_REG_IDX_WR(win_cpu, remap_h, MV_WIN_CPU_REMAP_HI, MV_MBUS_BRIDGE_BASE)

WIN_REG_IDX_RD(ddr, br, MV_WIN_DDR_BASE, MV_DDR_CADR_BASE)
WIN_REG_IDX_RD(ddr, sz, MV_WIN_DDR_SIZE, MV_DDR_CADR_BASE)

WIN_REG_BASE_IDX_RD(win_usb, cr, MV_WIN_USB_CTRL)
WIN_REG_BASE_IDX_RD(win_usb, br, MV_WIN_USB_BASE)
WIN_REG_BASE_IDX_WR(win_usb, cr, MV_WIN_USB_CTRL)
WIN_REG_BASE_IDX_WR(win_usb, br, MV_WIN_USB_BASE)

WIN_REG_BASE_IDX_RD(win_cesa, cr, MV_WIN_CESA_CTRL)
WIN_REG_BASE_IDX_RD(win_cesa, br, MV_WIN_CESA_BASE)
WIN_REG_BASE_IDX_WR(win_cesa, cr, MV_WIN_CESA_CTRL)
WIN_REG_BASE_IDX_WR(win_cesa, br, MV_WIN_CESA_BASE)

WIN_REG_BASE_IDX_RD(win_eth, br, MV_WIN_ETH_BASE)
WIN_REG_BASE_IDX_RD(win_eth, sz, MV_WIN_ETH_SIZE)
WIN_REG_BASE_IDX_RD(win_eth, har, MV_WIN_ETH_REMAP)
WIN_REG_BASE_IDX_WR(win_eth, br, MV_WIN_ETH_BASE)
WIN_REG_BASE_IDX_WR(win_eth, sz, MV_WIN_ETH_SIZE)
WIN_REG_BASE_IDX_WR(win_eth, har, MV_WIN_ETH_REMAP)

WIN_REG_BASE_IDX_RD2(win_xor, br, MV_WIN_XOR_BASE)
WIN_REG_BASE_IDX_RD2(win_xor, sz, MV_WIN_XOR_SIZE)
WIN_REG_BASE_IDX_RD2(win_xor, har, MV_WIN_XOR_REMAP)
WIN_REG_BASE_IDX_RD2(win_xor, ctrl, MV_WIN_XOR_CTRL)
WIN_REG_BASE_IDX_WR2(win_xor, br, MV_WIN_XOR_BASE)
WIN_REG_BASE_IDX_WR2(win_xor, sz, MV_WIN_XOR_SIZE)
WIN_REG_BASE_IDX_WR2(win_xor, har, MV_WIN_XOR_REMAP)
WIN_REG_BASE_IDX_WR2(win_xor, ctrl, MV_WIN_XOR_CTRL)

WIN_REG_BASE_RD(win_eth, bare, 0x290)
WIN_REG_BASE_RD(win_eth, epap, 0x294)
WIN_REG_BASE_WR(win_eth, bare, 0x290)
WIN_REG_BASE_WR(win_eth, epap, 0x294)

WIN_REG_BASE_IDX_RD(win_pcie, cr, MV_WIN_PCIE_CTRL);
WIN_REG_BASE_IDX_RD(win_pcie, br, MV_WIN_PCIE_BASE);
WIN_REG_BASE_IDX_RD(win_pcie, remap, MV_WIN_PCIE_REMAP);
WIN_REG_BASE_IDX_WR(win_pcie, cr, MV_WIN_PCIE_CTRL);
WIN_REG_BASE_IDX_WR(win_pcie, br, MV_WIN_PCIE_BASE);
WIN_REG_BASE_IDX_WR(win_pcie, remap, MV_WIN_PCIE_REMAP);
WIN_REG_BASE_IDX_WR(pcie, bar, MV_PCIE_BAR);

WIN_REG_BASE_IDX_RD(win_idma, br, MV_WIN_IDMA_BASE)
WIN_REG_BASE_IDX_RD(win_idma, sz, MV_WIN_IDMA_SIZE)
WIN_REG_BASE_IDX_RD(win_idma, har, MV_WIN_IDMA_REMAP)
WIN_REG_BASE_IDX_RD(win_idma, cap, MV_WIN_IDMA_CAP)
WIN_REG_BASE_IDX_WR(win_idma, br, MV_WIN_IDMA_BASE)
WIN_REG_BASE_IDX_WR(win_idma, sz, MV_WIN_IDMA_SIZE)
WIN_REG_BASE_IDX_WR(win_idma, har, MV_WIN_IDMA_REMAP)
WIN_REG_BASE_IDX_WR(win_idma, cap, MV_WIN_IDMA_CAP)
WIN_REG_BASE_RD(win_idma, bare, 0xa80)
WIN_REG_BASE_WR(win_idma, bare, 0xa80)

WIN_REG_BASE_IDX_RD(win_sata, cr, MV_WIN_SATA_CTRL);
WIN_REG_BASE_IDX_RD(win_sata, br, MV_WIN_SATA_BASE);
WIN_REG_BASE_IDX_WR(win_sata, cr, MV_WIN_SATA_CTRL);
WIN_REG_BASE_IDX_WR(win_sata, br, MV_WIN_SATA_BASE);

/**************************************************************************
 * Decode windows helper routines
 **************************************************************************/
void
soc_dump_decode_win(void)
{
	uint32_t dev, rev;
	int i;

	soc_id(&dev, &rev);

	for (i = 0; i < MV_WIN_CPU_MAX; i++) {
		printf("CPU window#%d: c 0x%08x, b 0x%08x", i,
		    win_cpu_cr_read(i),
		    win_cpu_br_read(i));

		if (win_cpu_can_remap(i))
			printf(", rl 0x%08x, rh 0x%08x",
			    win_cpu_remap_l_read(i),
			    win_cpu_remap_h_read(i));

		printf("\n");
	}
	printf("Internal regs base: 0x%08x\n",
	    bus_space_read_4(fdtbus_bs_tag, MV_INTREGS_BASE, 0));

	for (i = 0; i < MV_WIN_DDR_MAX; i++)
		printf("DDR CS#%d: b 0x%08x, s 0x%08x\n", i,
		    ddr_br_read(i), ddr_sz_read(i));
}

/**************************************************************************
 * CPU windows routines
 **************************************************************************/
int
win_cpu_can_remap(int i)
{
	uint32_t dev, rev;

	soc_id(&dev, &rev);

	/* Depending on the SoC certain windows have remap capability */
	if ((dev == MV_DEV_88F5182 && i < 2) ||
	    (dev == MV_DEV_88F5281 && i < 4) ||
	    (dev == MV_DEV_88F6281 && i < 4) ||
	    (dev == MV_DEV_MV78100 && i < 8) ||
	    (dev == MV_DEV_MV78100_Z0 && i < 8))
		return (1);

	return (0);
}

/* XXX This should check for overlapping remap fields too.. */
int
decode_win_overlap(int win, int win_no, const struct decode_win *wintab)
{
	const struct decode_win *tab;
	int i;

	tab = wintab;

	for (i = 0; i < win_no; i++, tab++) {
		if (i == win)
			/* Skip self */
			continue;

		if ((tab->base + tab->size - 1) < (wintab + win)->base)
			continue;

		else if (((wintab + win)->base + (wintab + win)->size - 1) <
		    tab->base)
			continue;
		else
			return (i);
	}

	return (-1);
}

static int
decode_win_cpu_valid(void)
{
	int i, j, rv;
	uint32_t b, e, s;

	if (cpu_wins_no > MV_WIN_CPU_MAX) {
		printf("CPU windows: too many entries: %d\n", cpu_wins_no);
		return (0);
	}

	rv = 1;
	for (i = 0; i < cpu_wins_no; i++) {

		if (cpu_wins[i].target == 0) {
			printf("CPU window#%d: DDR target window is not "
			    "supposed to be reprogrammed!\n", i);
			rv = 0;
		}

		if (cpu_wins[i].remap >= 0 && win_cpu_can_remap(i) != 1) {
			printf("CPU window#%d: not capable of remapping, but "
			    "val 0x%08x defined\n", i, cpu_wins[i].remap);
			rv = 0;
		}

		s = cpu_wins[i].size;
		b = cpu_wins[i].base;
		e = b + s - 1;
		if (s > (0xFFFFFFFF - b + 1)) {
			/*
			 * XXX this boundary check should account for 64bit
			 * and remapping..
			 */
			printf("CPU window#%d: no space for size 0x%08x at "
			    "0x%08x\n", i, s, b);
			rv = 0;
			continue;
		}

		j = decode_win_overlap(i, cpu_wins_no, &cpu_wins[0]);
		if (j >= 0) {
			printf("CPU window#%d: (0x%08x - 0x%08x) overlaps "
			    "with #%d (0x%08x - 0x%08x)\n", i, b, e, j,
			    cpu_wins[j].base,
			    cpu_wins[j].base + cpu_wins[j].size - 1);
			rv = 0;
		}
	}

	return (rv);
}

int
decode_win_cpu_set(int target, int attr, vm_paddr_t base, uint32_t size,
    int remap)
{
	uint32_t br, cr;
	int win;

	if (used_cpu_wins >= MV_WIN_CPU_MAX)
		return (0);

	win = used_cpu_wins++;

	br = base & 0xffff0000;
	win_cpu_br_write(win, br);

	if (win_cpu_can_remap(win)) {
		if (remap >= 0) {
			win_cpu_remap_l_write(win, remap & 0xffff0000);
			win_cpu_remap_h_write(win, 0);
		} else {
			/*
			 * Remap function is not used for a given window
			 * (capable of remapping) - set remap field with the
			 * same value as base.
			 */
			win_cpu_remap_l_write(win, base & 0xffff0000);
			win_cpu_remap_h_write(win, 0);
		}
	}

	cr = ((size - 1) & 0xffff0000) | (attr << 8) | (target << 4) | 1;
	win_cpu_cr_write(win, cr);

	return (0);
}

static void
decode_win_cpu_setup(void)
{
	int i;

	used_cpu_wins = 0;

	/* Disable all CPU windows */
	for (i = 0; i < MV_WIN_CPU_MAX; i++) {
		win_cpu_cr_write(i, 0);
		win_cpu_br_write(i, 0);
		if (win_cpu_can_remap(i)) {
			win_cpu_remap_l_write(i, 0);
			win_cpu_remap_h_write(i, 0);
		}
	}

	for (i = 0; i < cpu_wins_no; i++)
		if (cpu_wins[i].target > 0)
			decode_win_cpu_set(cpu_wins[i].target,
			    cpu_wins[i].attr, cpu_wins[i].base,
			    cpu_wins[i].size, cpu_wins[i].remap);

}

/*
 * Check if we're able to cover all active DDR banks.
 */
static int
decode_win_can_cover_ddr(int max)
{
	int i, c;

	c = 0;
	for (i = 0; i < MV_WIN_DDR_MAX; i++)
		if (ddr_is_active(i))
			c++;

	if (c > max) {
		printf("Unable to cover all active DDR banks: "
		    "%d, available windows: %d\n", c, max);
		return (0);
	}

	return (1);
}

/**************************************************************************
 * DDR windows routines
 **************************************************************************/
int
ddr_is_active(int i)
{

	if (ddr_sz_read(i) & 0x1)
		return (1);

	return (0);
}

uint32_t
ddr_base(int i)
{

	return (ddr_br_read(i) & 0xff000000);
}

uint32_t
ddr_size(int i)
{

	return ((ddr_sz_read(i) | 0x00ffffff) + 1);
}

uint32_t
ddr_attr(int i)
{

	return (i == 0 ? 0xe :
	    (i == 1 ? 0xd :
	    (i == 2 ? 0xb :
	    (i == 3 ? 0x7 : 0xff))));
}

uint32_t
ddr_target(int i)
{

	/* Mbus unit ID is 0x0 for DDR SDRAM controller */
	return (0);
}

/**************************************************************************
 * USB windows routines
 **************************************************************************/
static int
decode_win_usb_valid(void)
{

	return (decode_win_can_cover_ddr(MV_WIN_USB_MAX));
}

static void
decode_win_usb_dump(u_long base)
{
	int i;

	if (pm_is_disabled(CPU_PM_CTRL_USB(usb_port - 1)))
		return;

	for (i = 0; i < MV_WIN_USB_MAX; i++)
		printf("USB window#%d: c 0x%08x, b 0x%08x\n", i,
		    win_usb_cr_read(base, i), win_usb_br_read(base, i));
}

/*
 * Set USB decode windows.
 */
static void
decode_win_usb_setup(u_long base)
{
	uint32_t br, cr;
	int i, j;


	if (pm_is_disabled(CPU_PM_CTRL_USB(usb_port)))
		return;

	usb_port++;

	for (i = 0; i < MV_WIN_USB_MAX; i++) {
		win_usb_cr_write(base, i, 0);
		win_usb_br_write(base, i, 0);
	}

	/* Only access to active DRAM banks is required */
	for (i = 0; i < MV_WIN_DDR_MAX; i++) {
		if (ddr_is_active(i)) {
			br = ddr_base(i);
			/*
			 * XXX for 6281 we should handle Mbus write
			 * burst limit field in the ctrl reg
			 */
			cr = (((ddr_size(i) - 1) & 0xffff0000) |
			    (ddr_attr(i) << 8) |
			    (ddr_target(i) << 4) | 1);

			/* Set the first free USB window */
			for (j = 0; j < MV_WIN_USB_MAX; j++) {
				if (win_usb_cr_read(base, j) & 0x1)
					continue;

				win_usb_br_write(base, j, br);
				win_usb_cr_write(base, j, cr);
				break;
			}
		}
	}
}

/**************************************************************************
 * ETH windows routines
 **************************************************************************/

static int
win_eth_can_remap(int i)
{

	/* ETH encode windows 0-3 have remap capability */
	if (i < 4)
		return (1);
	
	return (0);
}

static int
eth_bare_read(uint32_t base, int i)
{
	uint32_t v;

	v = win_eth_bare_read(base);
	v &= (1 << i);

	return (v >> i);
}

static void
eth_bare_write(uint32_t base, int i, int val)
{
	uint32_t v;

	v = win_eth_bare_read(base);
	v &= ~(1 << i);
	v |= (val << i);
	win_eth_bare_write(base, v);
}

static void
eth_epap_write(uint32_t base, int i, int val)
{
	uint32_t v;

	v = win_eth_epap_read(base);
	v &= ~(0x3 << (i * 2));
	v |= (val << (i * 2));
	win_eth_epap_write(base, v);
}

static void
decode_win_eth_dump(u_long base)
{
	int i;

	if (pm_is_disabled(CPU_PM_CTRL_GE(eth_port - 1)))
		return;

	for (i = 0; i < MV_WIN_ETH_MAX; i++) {
		printf("ETH window#%d: b 0x%08x, s 0x%08x", i,
		    win_eth_br_read(base, i),
		    win_eth_sz_read(base, i));

		if (win_eth_can_remap(i))
			printf(", ha 0x%08x",
			    win_eth_har_read(base, i));

		printf("\n");
	}
	printf("ETH windows: bare 0x%08x, epap 0x%08x\n",
	    win_eth_bare_read(base),
	    win_eth_epap_read(base));
}

static void
decode_win_eth_setup(u_long base)
{
	uint32_t br, sz;
	int i, j;

	if (pm_is_disabled(CPU_PM_CTRL_GE(eth_port)))
		return;

	eth_port++;

	/* Disable, clear and revoke protection for all ETH windows */
	for (i = 0; i < MV_WIN_ETH_MAX; i++) {

		eth_bare_write(base, i, 1);
		eth_epap_write(base, i, 0);
		win_eth_br_write(base, i, 0);
		win_eth_sz_write(base, i, 0);
		if (win_eth_can_remap(i))
			win_eth_har_write(base, i, 0);
	}

	/* Only access to active DRAM banks is required */
	for (i = 0; i < MV_WIN_DDR_MAX; i++)
		if (ddr_is_active(i)) {

			br = ddr_base(i) | (ddr_attr(i) << 8) | ddr_target(i);
			sz = ((ddr_size(i) - 1) & 0xffff0000);

			/* Set the first free ETH window */
			for (j = 0; j < MV_WIN_ETH_MAX; j++) {
				if (eth_bare_read(base, j) == 0)
					continue;

				win_eth_br_write(base, j, br);
				win_eth_sz_write(base, j, sz);

				/* XXX remapping ETH windows not supported */

				/* Set protection RW */
				eth_epap_write(base, j, 0x3);

				/* Enable window */
				eth_bare_write(base, j, 0);
				break;
			}
		}
}

static int
decode_win_eth_valid(void)
{

	return (decode_win_can_cover_ddr(MV_WIN_ETH_MAX));
}

/**************************************************************************
 * PCIE windows routines
 **************************************************************************/

static void
decode_win_pcie_setup(u_long base)
{
	uint32_t size = 0;
	uint32_t cr, br;
	int i, j;

	for (i = 0; i < MV_PCIE_BAR_MAX; i++)
		pcie_bar_write(base, i, 0);

	for (i = 0; i < MV_WIN_PCIE_MAX; i++) {
		win_pcie_cr_write(base, i, 0);
		win_pcie_br_write(base, i, 0);
		win_pcie_remap_write(base, i, 0);
	}

	for (i = 0; i < MV_WIN_DDR_MAX; i++) {
		if (ddr_is_active(i)) {
			/* Map DDR to BAR 1 */
			cr = (ddr_size(i) - 1) & 0xffff0000;
			size += ddr_size(i) & 0xffff0000;
			cr |= (ddr_attr(i) << 8) | (ddr_target(i) << 4) | 1;
			br = ddr_base(i);

			/* Use the first available PCIE window */
			for (j = 0; j < MV_WIN_PCIE_MAX; j++) {
				if (win_pcie_cr_read(base, j) != 0)
					continue;

				win_pcie_br_write(base, j, br);
				win_pcie_cr_write(base, j, cr);
				break;
			}
		}
	}

	/*
	 * Upper 16 bits in BAR register is interpreted as BAR size
	 * (in 64 kB units) plus 64kB, so substract 0x10000
	 * form value passed to register to get correct value.
	 */
	size -= 0x10000;
	pcie_bar_write(base, 0, size | 1);
}

static int
decode_win_pcie_valid(void)
{

	return (decode_win_can_cover_ddr(MV_WIN_PCIE_MAX));
}

/**************************************************************************
 * IDMA windows routines
 **************************************************************************/
#if defined(SOC_MV_ORION) || defined(SOC_MV_DISCOVERY)
static int
idma_bare_read(u_long base, int i)
{
	uint32_t v;

	v = win_idma_bare_read(base);
	v &= (1 << i);

	return (v >> i);
}

static void
idma_bare_write(u_long base, int i, int val)
{
	uint32_t v;

	v = win_idma_bare_read(base);
	v &= ~(1 << i);
	v |= (val << i);
	win_idma_bare_write(base, v);
}

/*
 * Sets channel protection 'val' for window 'w' on channel 'c'
 */
static void
idma_cap_write(u_long base, int c, int w, int val)
{
	uint32_t v;

	v = win_idma_cap_read(base, c);
	v &= ~(0x3 << (w * 2));
	v |= (val << (w * 2));
	win_idma_cap_write(base, c, v);
}

/*
 * Set protection 'val' on all channels for window 'w'
 */
static void
idma_set_prot(u_long base, int w, int val)
{
	int c;

	for (c = 0; c < MV_IDMA_CHAN_MAX; c++)
		idma_cap_write(base, c, w, val);
}

static int
win_idma_can_remap(int i)
{

	/* IDMA decode windows 0-3 have remap capability */
	if (i < 4)
		return (1);

	return (0);
}

void
decode_win_idma_setup(u_long base)
{
	uint32_t br, sz;
	int i, j;

	if (pm_is_disabled(CPU_PM_CTRL_IDMA))
		return;
	/*
	 * Disable and clear all IDMA windows, revoke protection for all channels
	 */
	for (i = 0; i < MV_WIN_IDMA_MAX; i++) {

		idma_bare_write(base, i, 1);
		win_idma_br_write(base, i, 0);
		win_idma_sz_write(base, i, 0);
		if (win_idma_can_remap(i) == 1)
			win_idma_har_write(base, i, 0);
	}
	for (i = 0; i < MV_IDMA_CHAN_MAX; i++)
		win_idma_cap_write(base, i, 0);

	/*
	 * Set up access to all active DRAM banks
	 */
	for (i = 0; i < MV_WIN_DDR_MAX; i++)
		if (ddr_is_active(i)) {
			br = ddr_base(i) | (ddr_attr(i) << 8) | ddr_target(i);
			sz = ((ddr_size(i) - 1) & 0xffff0000);

			/* Place DDR entries in non-remapped windows */
			for (j = 0; j < MV_WIN_IDMA_MAX; j++)
				if (win_idma_can_remap(j) != 1 &&
				    idma_bare_read(base, j) == 1) {

					/* Configure window */
					win_idma_br_write(base, j, br);
					win_idma_sz_write(base, j, sz);

					/* Set protection RW on all channels */
					idma_set_prot(base, j, 0x3);

					/* Enable window */
					idma_bare_write(base, j, 0);
					break;
				}
		}

	/*
	 * Remaining targets -- from statically defined table
	 */
	for (i = 0; i < idma_wins_no; i++)
		if (idma_wins[i].target > 0) {
			br = (idma_wins[i].base & 0xffff0000) |
			    (idma_wins[i].attr << 8) | idma_wins[i].target;
			sz = ((idma_wins[i].size - 1) & 0xffff0000);

			/* Set the first free IDMA window */
			for (j = 0; j < MV_WIN_IDMA_MAX; j++) {
				if (idma_bare_read(base, j) == 0)
					continue;

				/* Configure window */
				win_idma_br_write(base, j, br);
				win_idma_sz_write(base, j, sz);
				if (win_idma_can_remap(j) &&
				    idma_wins[j].remap >= 0)
					win_idma_har_write(base, j,
					    idma_wins[j].remap);

				/* Set protection RW on all channels */
				idma_set_prot(base, j, 0x3);

				/* Enable window */
				idma_bare_write(base, j, 0);
				break;
			}
		}
}

int
decode_win_idma_valid(void)
{
	const struct decode_win *wintab;
	int c, i, j, rv;
	uint32_t b, e, s;

	if (idma_wins_no > MV_WIN_IDMA_MAX) {
		printf("IDMA windows: too many entries: %d\n", idma_wins_no);
		return (0);
	}
	for (i = 0, c = 0; i < MV_WIN_DDR_MAX; i++)
		if (ddr_is_active(i))
			c++;

	if (idma_wins_no > (MV_WIN_IDMA_MAX - c)) {
		printf("IDMA windows: too many entries: %d, available: %d\n",
		    idma_wins_no, MV_WIN_IDMA_MAX - c);
		return (0);
	}

	wintab = idma_wins;
	rv = 1;
	for (i = 0; i < idma_wins_no; i++, wintab++) {

		if (wintab->target == 0) {
			printf("IDMA window#%d: DDR target window is not "
			    "supposed to be reprogrammed!\n", i);
			rv = 0;
		}

		if (wintab->remap >= 0 && win_cpu_can_remap(i) != 1) {
			printf("IDMA window#%d: not capable of remapping, but "
			    "val 0x%08x defined\n", i, wintab->remap);
			rv = 0;
		}

		s = wintab->size;
		b = wintab->base;
		e = b + s - 1;
		if (s > (0xFFFFFFFF - b + 1)) {
			/* XXX this boundary check should account for 64bit and
			 * remapping.. */
			printf("IDMA window#%d: no space for size 0x%08x at "
			    "0x%08x\n", i, s, b);
			rv = 0;
			continue;
		}

		j = decode_win_overlap(i, idma_wins_no, &idma_wins[0]);
		if (j >= 0) {
			printf("IDMA window#%d: (0x%08x - 0x%08x) overlaps "
			    "with #%d (0x%08x - 0x%08x)\n", i, b, e, j,
			    idma_wins[j].base,
			    idma_wins[j].base + idma_wins[j].size - 1);
			rv = 0;
		}
	}

	return (rv);
}

void
decode_win_idma_dump(u_long base)
{
	int i;

	if (pm_is_disabled(CPU_PM_CTRL_IDMA))
		return;

	for (i = 0; i < MV_WIN_IDMA_MAX; i++) {
		printf("IDMA window#%d: b 0x%08x, s 0x%08x", i,
		    win_idma_br_read(base, i), win_idma_sz_read(base, i));
		
		if (win_idma_can_remap(i))
			printf(", ha 0x%08x", win_idma_har_read(base, i));

		printf("\n");
	}
	for (i = 0; i < MV_IDMA_CHAN_MAX; i++)
		printf("IDMA channel#%d: ap 0x%08x\n", i,
		    win_idma_cap_read(base, i));
	printf("IDMA windows: bare 0x%08x\n", win_idma_bare_read(base));
}
#else

/* Provide dummy functions to satisfy the build for SoCs not equipped with IDMA */
int
decode_win_idma_valid(void)
{

	return (1);
}

void
decode_win_idma_setup(u_long base)
{
}

void
decode_win_idma_dump(u_long base)
{
}
#endif

/**************************************************************************
 * XOR windows routines
 **************************************************************************/
#if defined(SOC_MV_KIRKWOOD) || defined(SOC_MV_DISCOVERY)
static int
xor_ctrl_read(u_long base, int i, int c, int e)
{
	uint32_t v;
	v = win_xor_ctrl_read(base, c, e);
	v &= (1 << i);

	return (v >> i);
}

static void
xor_ctrl_write(u_long base, int i, int c, int e, int val)
{
	uint32_t v;

	v = win_xor_ctrl_read(base, c, e);
	v &= ~(1 << i);
	v |= (val << i);
	win_xor_ctrl_write(base, c, e, v);
}

/*
 * Set channel protection 'val' for window 'w' on channel 'c'
 */

static void
xor_chan_write(u_long base, int c, int e, int w, int val)
{
	uint32_t v;

	v = win_xor_ctrl_read(base, c, e);
	v &= ~(0x3 << (w * 2 + 16));
	v |= (val << (w * 2 + 16));
	win_xor_ctrl_write(base, c, e, v);
}

/*
 * Set protection 'val' on all channels for window 'w' on engine 'e'
 */
static void
xor_set_prot(u_long base, int w, int e, int val)
{
	int c;

	for (c = 0; c < MV_XOR_CHAN_MAX; c++)
		xor_chan_write(base, c, e, w, val);
}

static int
win_xor_can_remap(int i)
{

	/* XOR decode windows 0-3 have remap capability */
	if (i < 4)
		return (1);

	return (0);
}

static int
xor_max_eng(void)
{
	uint32_t dev, rev;

	soc_id(&dev, &rev);
	if (dev == MV_DEV_88F6281)
		return (2);
	else if ((dev == MV_DEV_MV78100) || (dev == MV_DEV_MV78100_Z0))
		return (1);
	else
		return (0);
}

static void
xor_active_dram(u_long base, int c, int e, int *window)
{
	uint32_t br, sz;
	int i, m, w;

	/*
	 * Set up access to all active DRAM banks
	 */
	m = xor_max_eng();
	for (i = 0; i < m; i++)
		if (ddr_is_active(i)) {
			br = ddr_base(i) | (ddr_attr(i) << 8) |
			    ddr_target(i);
			sz = ((ddr_size(i) - 1) & 0xffff0000);

			/* Place DDR entries in non-remapped windows */
			for (w = 0; w < MV_WIN_XOR_MAX; w++)
				if (win_xor_can_remap(w) != 1 &&
				    (xor_ctrl_read(base, w, c, e) == 0) &&
				    w > *window) {
					/* Configure window */
					win_xor_br_write(base, w, e, br);
					win_xor_sz_write(base, w, e, sz);

					/* Set protection RW on all channels */
					xor_set_prot(base, w, e, 0x3);

					/* Enable window */
					xor_ctrl_write(base, w, c, e, 1);
					(*window)++;
					break;
				}
		}
}

void
decode_win_xor_setup(u_long base)
{
	uint32_t br, sz;
	int i, j, z, e = 1, m, window;

	if (pm_is_disabled(CPU_PM_CTRL_XOR))
		return;

	/*
	 * Disable and clear all XOR windows, revoke protection for all
	 * channels
	 */
	m = xor_max_eng();
	for (j = 0; j < m; j++, e--) {

		/* Number of non-remaped windows */
		window = MV_XOR_NON_REMAP - 1;

		for (i = 0; i < MV_WIN_XOR_MAX; i++) {
			win_xor_br_write(base, i, e, 0);
			win_xor_sz_write(base, i, e, 0);
		}

		if (win_xor_can_remap(i) == 1)
			win_xor_har_write(base, i, e, 0);

		for (i = 0; i < MV_XOR_CHAN_MAX; i++) {
			win_xor_ctrl_write(base, i, e, 0);
			xor_active_dram(base, i, e, &window);
		}

		/*
		 * Remaining targets -- from a statically defined table
		 */
		for (i = 0; i < xor_wins_no; i++)
			if (xor_wins[i].target > 0) {
				br = (xor_wins[i].base & 0xffff0000) |
				    (xor_wins[i].attr << 8) |
				    xor_wins[i].target;
				sz = ((xor_wins[i].size - 1) & 0xffff0000);

				/* Set the first free XOR window */
				for (z = 0; z < MV_WIN_XOR_MAX; z++) {
					if (xor_ctrl_read(base, z, 0, e) &&
					    xor_ctrl_read(base, z, 1, e))
						continue;

					/* Configure window */
					win_xor_br_write(base, z, e, br);
					win_xor_sz_write(base, z, e, sz);
					if (win_xor_can_remap(z) &&
					    xor_wins[z].remap >= 0)
						win_xor_har_write(base, z, e,
						    xor_wins[z].remap);

					/* Set protection RW on all channels */
					xor_set_prot(base, z, e, 0x3);

					/* Enable window */
					xor_ctrl_write(base, z, 0, e, 1);
					xor_ctrl_write(base, z, 1, e, 1);
					break;
				}
			}
	}
}

int
decode_win_xor_valid(void)
{
	const struct decode_win *wintab;
	int c, i, j, rv;
	uint32_t b, e, s;

	if (xor_wins_no > MV_WIN_XOR_MAX) {
		printf("XOR windows: too many entries: %d\n", xor_wins_no);
		return (0);
	}
	for (i = 0, c = 0; i < MV_WIN_DDR_MAX; i++)
		if (ddr_is_active(i))
			c++;

	if (xor_wins_no > (MV_WIN_XOR_MAX - c)) {
		printf("XOR windows: too many entries: %d, available: %d\n",
		    xor_wins_no, MV_WIN_IDMA_MAX - c);
		return (0);
	}

	wintab = xor_wins;
	rv = 1;
	for (i = 0; i < xor_wins_no; i++, wintab++) {

		if (wintab->target == 0) {
			printf("XOR window#%d: DDR target window is not "
			    "supposed to be reprogrammed!\n", i);
			rv = 0;
		}

		if (wintab->remap >= 0 && win_cpu_can_remap(i) != 1) {
			printf("XOR window#%d: not capable of remapping, but "
			    "val 0x%08x defined\n", i, wintab->remap);
			rv = 0;
		}

		s = wintab->size;
		b = wintab->base;
		e = b + s - 1;
		if (s > (0xFFFFFFFF - b + 1)) {
			/*
			 * XXX this boundary check should account for 64bit
			 * and remapping..
			 */
			printf("XOR window#%d: no space for size 0x%08x at "
			    "0x%08x\n", i, s, b);
			rv = 0;
			continue;
		}

		j = decode_win_overlap(i, xor_wins_no, &xor_wins[0]);
		if (j >= 0) {
			printf("XOR window#%d: (0x%08x - 0x%08x) overlaps "
			    "with #%d (0x%08x - 0x%08x)\n", i, b, e, j,
			    xor_wins[j].base,
			    xor_wins[j].base + xor_wins[j].size - 1);
			rv = 0;
		}
	}

	return (rv);
}

void
decode_win_xor_dump(u_long base)
{
	int i, j;
	int e = 1;

	if (pm_is_disabled(CPU_PM_CTRL_XOR))
		return;

	for (j = 0; j < xor_max_eng(); j++, e--) {
		for (i = 0; i < MV_WIN_XOR_MAX; i++) {
			printf("XOR window#%d: b 0x%08x, s 0x%08x", i,
			    win_xor_br_read(base, i, e), win_xor_sz_read(base, i, e));

			if (win_xor_can_remap(i))
				printf(", ha 0x%08x", win_xor_har_read(base, i, e));

			printf("\n");
		}
		for (i = 0; i < MV_XOR_CHAN_MAX; i++)
			printf("XOR control#%d: 0x%08x\n", i,
			    win_xor_ctrl_read(base, i, e));
	}
}

#else
/* Provide dummy functions to satisfy the build for SoCs not equipped with XOR */
static int
decode_win_xor_valid(void)
{

	return (1);
}

static void
decode_win_xor_setup(u_long base)
{
}

static void
decode_win_xor_dump(u_long base)
{
}
#endif

/**************************************************************************
 * CESA TDMA windows routines
 **************************************************************************/
#if defined(SOC_MV_KIRKWOOD) || defined(SOC_MV_DISCOVERY)
/*
 * Dump CESA TDMA decode windows.
 */
static void
decode_win_cesa_dump(u_long base)
{
	int i;

	if (pm_is_disabled(CPU_PM_CTRL_CRYPTO))
		return;

	for (i = 0; i < MV_WIN_CESA_MAX; i++)
		printf("CESA window#%d: c 0x%08x, b 0x%08x\n", i,
		    win_cesa_cr_read(base, i), win_cesa_br_read(base, i));
}


/*
 * Set CESA TDMA decode windows.
 */
static void
decode_win_cesa_setup(u_long base)
{
	uint32_t br, cr;
	int i, j;

	if (pm_is_disabled(CPU_PM_CTRL_CRYPTO))
		return;

	/* Disable and clear all CESA windows */
	for (i = 0; i < MV_WIN_CESA_MAX; i++) {
		win_cesa_cr_write(base, i, 0);
		win_cesa_br_write(base, i, 0);
	}

	/* Only access to active DRAM banks is required. */
	for (i = 0; i < MV_WIN_DDR_MAX; i++)
		if (ddr_is_active(i)) {
			br = ddr_base(i);
			cr = (((ddr_size(i) - 1) & 0xffff0000) |
			   (ddr_attr(i) << 8) | (ddr_target(i) << 4) | 1);

			/* Set the first available CESA window */
			for (j = 0; j < MV_WIN_CESA_MAX; j++) {
				if (win_cesa_cr_read(base, j) & 0x1)
					continue;

				win_cesa_br_write(base, j, br);
				win_cesa_cr_write(base, j, cr);
				break;
			}
		}
}

/*
 * Check CESA TDMA decode windows.
 */
static int
decode_win_cesa_valid(void)
{

	return (decode_win_can_cover_ddr(MV_WIN_CESA_MAX));
}
#else

/*
 * Provide dummy functions to satisfy the build for SoCs not equipped with
 * CESA
 */

static int
decode_win_cesa_valid(void)
{

	return (1);
}

static void
decode_win_cesa_setup(u_long base)
{
}

static void
decode_win_cesa_dump(u_long base)
{
}
#endif

/**************************************************************************
 * SATA windows routines
 **************************************************************************/
static void
decode_win_sata_setup(u_long base)
{
	uint32_t cr, br;
	int i, j;

	if (pm_is_disabled(CPU_PM_CTRL_SATA))
		return;

	for (i = 0; i < MV_WIN_SATA_MAX; i++) {
		win_sata_cr_write(base, i, 0);
		win_sata_br_write(base, i, 0);
	}

	for (i = 0; i < MV_WIN_DDR_MAX; i++)
		if (ddr_is_active(i)) {
			cr = ((ddr_size(i) - 1) & 0xffff0000) |
			    (ddr_attr(i) << 8) | (ddr_target(i) << 4) | 1;
			br = ddr_base(i);

			/* Use the first available SATA window */
			for (j = 0; j < MV_WIN_SATA_MAX; j++) {
				if ((win_sata_cr_read(base, j) & 1) != 0)
					continue;

				win_sata_br_write(base, j, br);
				win_sata_cr_write(base, j, cr);
				break;
			}
		}
}

static int
decode_win_sata_valid(void)
{
	uint32_t dev, rev;

	soc_id(&dev, &rev);
	if (dev == MV_DEV_88F5281)
		return (1);

	return (decode_win_can_cover_ddr(MV_WIN_SATA_MAX));
}

/**************************************************************************
 * FDT parsing routines.
 **************************************************************************/

static int
fdt_get_ranges(const char *nodename, void *buf, int size, int *tuples,
    int *tuplesize)
{
	phandle_t node;
	pcell_t addr_cells, par_addr_cells, size_cells;
	int len, tuple_size, tuples_count;

	node = OF_finddevice(nodename);
	if (node <= 0)
		return (EINVAL);

	if ((fdt_addrsize_cells(node, &addr_cells, &size_cells)) != 0)
		return (ENXIO);

	par_addr_cells = fdt_parent_addr_cells(node);
	if (par_addr_cells > 2)
		return (ERANGE);

	tuple_size = sizeof(pcell_t) * (addr_cells + par_addr_cells +
	    size_cells);

	/* Note the OF_getprop_alloc() cannot be used at this early stage. */
	len = OF_getprop(node, "ranges", buf, size);

	/*
	 * XXX this does not handle the empty 'ranges;' case, which is
	 * legitimate and should be allowed.
	 */
	tuples_count = len / tuple_size;
	if (tuples_count <= 0)
		return (ERANGE);

	if (fdt_ranges_verify(buf, tuples_count, par_addr_cells,
	    addr_cells, size_cells) != 0)
		return (ERANGE);

	*tuples = tuples_count;
	*tuplesize = tuple_size;
	return (0);
}

static int
win_cpu_from_dt(void)
{
	pcell_t ranges[48];
	u_long sram_base, sram_size;
	phandle_t node;
	int i, entry_size, err, t, tuple_size, tuples;

	/* Retrieve 'ranges' property of '/localbus' node. */
	if ((err = fdt_get_ranges("/localbus", ranges, sizeof(ranges),
	    &tuples, &tuple_size)) != 0)
		return (err);

	/*
	 * Fill CPU decode windows table.
	 */
	bzero((void *)&cpu_win_tbl, sizeof(cpu_win_tbl));

	entry_size = tuple_size / sizeof(pcell_t);
	cpu_wins_no = tuples;

	for (i = 0, t = 0; t < tuples; i += entry_size, t++) {
		cpu_win_tbl[t].target = 1;
		cpu_win_tbl[t].attr = fdt32_to_cpu(ranges[i + 1]);
		cpu_win_tbl[t].base = fdt32_to_cpu(ranges[i + 2]);
		cpu_win_tbl[t].size = fdt32_to_cpu(ranges[i + 3]);
		cpu_win_tbl[t].remap = -1;
		debugf("target = 0x%0x attr = 0x%0x base = 0x%0x "
		    "size = 0x%0x remap = %d\n", cpu_win_tbl[t].target,
		    cpu_win_tbl[t].attr, cpu_win_tbl[t].base,
		    cpu_win_tbl[t].size, cpu_win_tbl[t].remap);
	}

	/*
	 * Retrieve CESA SRAM data.
	 */
	if ((node = OF_finddevice("sram")) != 0)
		if (fdt_is_compatible(node, "mrvl,cesa-sram"))
			goto moveon;

	if ((node = OF_finddevice("/")) == 0)
		return (ENXIO);

	if ((node = fdt_find_compatible(node, "mrvl,cesa-sram", 0)) == 0)
		/* SRAM block is not always present. */
		return (0);
moveon:
	sram_base = sram_size = 0;
	if (fdt_regsize(node, &sram_base, &sram_size) != 0)
		return (EINVAL);

	cpu_win_tbl[++t].target = MV_WIN_CESA_TARGET;
	cpu_win_tbl[t].attr = MV_WIN_CESA_ATTR;
	cpu_win_tbl[t].base = sram_base;
	cpu_win_tbl[t].size = sram_size;
	cpu_win_tbl[t].remap = -1;
	debugf("sram: base = 0x%0lx size = 0x%0lx\n", sram_base, sram_size);

	return (0);
}

static int
fdt_win_setup(void)
{
	phandle_t node, child;
	struct soc_node_spec *soc_node;
	u_long size, base;
	int err, i;

	node = OF_finddevice("/");
	if (node == 0)
		panic("fdt_win_setup: no root node");

	node = fdt_find_compatible(node, "simple-bus", 1);
	if (node == 0)
		return (ENXIO);

	/*
	 * Traverse through all children of simple-bus node, and retrieve
	 * decode windows data for devices (if applicable).
	 */
	for (child = OF_child(node); child != 0; child = OF_peer(child))
		for (i = 0; soc_nodes[i].compat != NULL; i++) {

			soc_node = &soc_nodes[i];

			if (!fdt_is_compatible(child, soc_node->compat))
				continue;

			err = fdt_regsize(child, &base, &size);
			if (err != 0)
				return (err);

			base += fdt_immr_va;
			if (soc_node->decode_handler != NULL)
				soc_node->decode_handler(base);
			else
				return (ENXIO);

			if (MV_DUMP_WIN && (soc_node->dump_handler != NULL))
				soc_node->dump_handler(base);
		}

	return (0);
}

static void
fdt_fixup_busfreq(phandle_t root)
{
	phandle_t sb;
	pcell_t freq;

	/*
	 * This fixup sets the simple-bus bus-frequency property.
	 */

	if ((sb = fdt_find_compatible(root, "simple-bus", 1)) == 0)
		return;

	freq = cpu_to_fdt32(get_tclk());
	OF_setprop(sb, "bus-frequency", (void *)&freq, sizeof(freq));
}

struct fdt_fixup_entry fdt_fixup_table[] = {
	{ "mrvl,DB-88F6281", &fdt_fixup_busfreq },
	{ NULL, NULL }
};

static int
fdt_pic_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{

	if (!fdt_is_compatible(node, "mrvl,pic"))
		return (ENXIO);

	*interrupt = fdt32_to_cpu(intr[0]);
	*trig = INTR_TRIGGER_CONFORM;
	*pol = INTR_POLARITY_CONFORM;

	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_pic_decode_ic,
	NULL
};
