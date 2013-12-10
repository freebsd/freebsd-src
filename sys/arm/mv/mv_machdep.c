/*-
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/at91/kb920x_machdep.c, rev 45
 */

#include "opt_ddb.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/devmap.h>
#include <machine/machdep.h>

#include <arm/mv/mvreg.h>	/* XXX */
#include <arm/mv/mvvar.h>	/* XXX eventually this should be eliminated */
#include <arm/mv/mvwin.h>

#include <dev/fdt/fdt_common.h>

static int platform_mpp_init(void);
#if defined(SOC_MV_ARMADAXP)
void armadaxp_init_coher_fabric(void);
void armadaxp_l2_init(void);
#endif

#define MPP_PIN_MAX		68
#define MPP_PIN_CELLS		2
#define MPP_PINS_PER_REG	8
#define MPP_SEL(pin,func)	(((func) & 0xf) <<		\
    (((pin) % MPP_PINS_PER_REG) * 4))

static int
platform_mpp_init(void)
{
	pcell_t pinmap[MPP_PIN_MAX * MPP_PIN_CELLS];
	int mpp[MPP_PIN_MAX];
	uint32_t ctrl_val, ctrl_offset;
	pcell_t reg[4];
	u_long start, size;
	phandle_t node;
	pcell_t pin_cells, *pinmap_ptr, pin_count;
	ssize_t len;
	int par_addr_cells, par_size_cells;
	int tuple_size, tuples, rv, pins, i, j;
	int mpp_pin, mpp_function;

	/*
	 * Try to access the MPP node directly i.e. through /aliases/mpp.
	 */
	if ((node = OF_finddevice("mpp")) != -1)
		if (fdt_is_compatible(node, "mrvl,mpp"))
			goto moveon;
	/*
	 * Find the node the long way.
	 */
	if ((node = OF_finddevice("/")) == -1)
		return (ENXIO);

	if ((node = fdt_find_compatible(node, "simple-bus", 0)) == 0)
		return (ENXIO);

	if ((node = fdt_find_compatible(node, "mrvl,mpp", 0)) == 0)
		/*
		 * No MPP node. Fall back to how MPP got set by the
		 * first-stage loader and try to continue booting.
		 */
		return (0);
moveon:
	/*
	 * Process 'reg' prop.
	 */
	if ((rv = fdt_addrsize_cells(OF_parent(node), &par_addr_cells,
	    &par_size_cells)) != 0)
		return(ENXIO);

	tuple_size = sizeof(pcell_t) * (par_addr_cells + par_size_cells);
	len = OF_getprop(node, "reg", reg, sizeof(reg));
	tuples = len / tuple_size;
	if (tuple_size <= 0)
		return (EINVAL);

	/*
	 * Get address/size. XXX we assume only the first 'reg' tuple is used.
	 */
	rv = fdt_data_to_res(reg, par_addr_cells, par_size_cells,
	    &start, &size);
	if (rv != 0)
		return (rv);
	start += fdt_immr_va;

	/*
	 * Process 'pin-count' and 'pin-map' props.
	 */
	if (OF_getprop(node, "pin-count", &pin_count, sizeof(pin_count)) <= 0)
		return (ENXIO);
	pin_count = fdt32_to_cpu(pin_count);
	if (pin_count > MPP_PIN_MAX)
		return (ERANGE);

	if (OF_getprop(node, "#pin-cells", &pin_cells, sizeof(pin_cells)) <= 0)
		pin_cells = MPP_PIN_CELLS;
	pin_cells = fdt32_to_cpu(pin_cells);
	if (pin_cells > MPP_PIN_CELLS)
		return (ERANGE);
	tuple_size = sizeof(pcell_t) * pin_cells;

	bzero(pinmap, sizeof(pinmap));
	len = OF_getprop(node, "pin-map", pinmap, sizeof(pinmap));
	if (len <= 0)
		return (ERANGE);
	if (len % tuple_size)
		return (ERANGE);
	pins = len / tuple_size;
	if (pins > pin_count)
		return (ERANGE);
	/*
	 * Fill out a "mpp[pin] => function" table. All pins unspecified in
	 * the 'pin-map' property are defaulted to 0 function i.e. GPIO.
	 */
	bzero(mpp, sizeof(mpp));
	pinmap_ptr = pinmap;
	for (i = 0; i < pins; i++) {
		mpp_pin = fdt32_to_cpu(*pinmap_ptr);
		mpp_function = fdt32_to_cpu(*(pinmap_ptr + 1));
		mpp[mpp_pin] = mpp_function;
		pinmap_ptr += pin_cells;
	}

	/*
	 * Prepare and program MPP control register values.
	 */
	ctrl_offset = 0;
	for (i = 0; i < pin_count;) {
		ctrl_val = 0;

		for (j = 0; j < MPP_PINS_PER_REG; j++) {
			if (i + j == pin_count - 1)
				break;
			ctrl_val |= MPP_SEL(i + j, mpp[i + j]);
		}
		i += MPP_PINS_PER_REG;
		bus_space_write_4(fdtbus_bs_tag, start, ctrl_offset,
		    ctrl_val);

#if defined(SOC_MV_ORION)
		/*
		 * Third MPP reg on Orion SoC is placed
		 * non-linearly (with different offset).
		 */
		if (i ==  (2 * MPP_PINS_PER_REG))
			ctrl_offset = 0x50;
		else
#endif
			ctrl_offset += 4;
	}

	return (0);
}

vm_offset_t
initarm_lastaddr(void)
{

	return (fdt_immr_va);
}

void
initarm_early_init(void)
{

	if (fdt_immr_addr(MV_BASE) != 0)
		while (1);
}

void
initarm_gpio_init(void)
{

	/*
	 * Re-initialise MPP. It is important to call this prior to using
	 * console as the physical connection can be routed via MPP.
	 */
	if (platform_mpp_init() != 0)
		while (1);
}

void
initarm_late_init(void)
{
	/*
	 * Re-initialise decode windows
	 */
#if !defined(SOC_MV_FREY)
	if (soc_decode_win() != 0)
		printf("WARNING: could not re-initialise decode windows! "
		    "Running with existing settings...\n");
#else
	/* Disable watchdog and timers */
	write_cpu_ctrl(CPU_TIMERS_BASE + CPU_TIMER_CONTROL, 0);
#endif
#if defined(SOC_MV_ARMADAXP)
#if !defined(SMP)
	/* For SMP case it should be initialized after APs are booted */
	armadaxp_init_coher_fabric();
#endif
	armadaxp_l2_init();
#endif
}

#define FDT_DEVMAP_MAX	(MV_WIN_CPU_MAX + 2)
static struct arm_devmap_entry fdt_devmap[FDT_DEVMAP_MAX] = {
	{ 0, 0, 0, 0, 0, }
};

static int
platform_sram_devmap(struct arm_devmap_entry *map)
{
#if !defined(SOC_MV_ARMADAXP)
	phandle_t child, root;
	u_long base, size;
	/*
	 * SRAM range.
	 */
	if ((child = OF_finddevice("/sram")) != 0)
		if (fdt_is_compatible(child, "mrvl,cesa-sram") ||
		    fdt_is_compatible(child, "mrvl,scratchpad"))
			goto moveon;

	if ((root = OF_finddevice("/")) == 0)
		return (ENXIO);

	if ((child = fdt_find_compatible(root, "mrvl,cesa-sram", 0)) == 0 &&
	    (child = fdt_find_compatible(root, "mrvl,scratchpad", 0)) == 0)
			goto out;

moveon:
	if (fdt_regsize(child, &base, &size) != 0)
		return (EINVAL);

	map->pd_va = MV_CESA_SRAM_BASE; /* XXX */
	map->pd_pa = base;
	map->pd_size = size;
	map->pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	map->pd_cache = PTE_NOCACHE;

	return (0);
out:
#endif
	return (ENOENT);

}

/*
 * Supply a default do-nothing implementation of fdt_pci_devmap() via a weak
 * alias.  Many Marvell platforms don't support a PCI interface, but to support
 * those that do, we end up with a reference to this function below, in
 * initarm_devmap_init().  If "device pci" appears in the kernel config, the
 * real implementation of this function in dev/fdt/fdt_pci.c overrides the weak
 * alias defined here.
 */
int mv_default_fdt_pci_devmap(phandle_t node, struct arm_devmap_entry *devmap,
    vm_offset_t io_va, vm_offset_t mem_va);
int
mv_default_fdt_pci_devmap(phandle_t node, struct arm_devmap_entry *devmap,
    vm_offset_t io_va, vm_offset_t mem_va)
{

	return (0);
}
__weak_reference(mv_default_fdt_pci_devmap, fdt_pci_devmap);

/*
 * XXX: When device entry in devmap has pd_size smaller than section size,
 * system will freeze during initialization
 */

/*
 * Construct pmap_devmap[] with DT-derived config data.
 */
int
initarm_devmap_init(void)
{
	phandle_t root, child;
	pcell_t bank_count;
	int i, num_mapped;

	i = 0;
	arm_devmap_register_table(&fdt_devmap[0]);

#ifdef SOC_MV_ARMADAXP
	vm_paddr_t cur_immr_pa;

	/*
	 * Acquire SoC registers' base passed by u-boot and fill devmap
	 * accordingly. DTB is going to be modified basing on this data
	 * later.
	 */
	__asm __volatile("mrc p15, 4, %0, c15, c0, 0" : "=r" (cur_immr_pa));
	cur_immr_pa = (cur_immr_pa << 13) & 0xff000000;
	if (cur_immr_pa != 0)
		fdt_immr_pa = cur_immr_pa;
#endif
	/*
	 * IMMR range.
	 */
	fdt_devmap[i].pd_va = fdt_immr_va;
	fdt_devmap[i].pd_pa = fdt_immr_pa;
	fdt_devmap[i].pd_size = fdt_immr_size;
	fdt_devmap[i].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[i].pd_cache = PTE_NOCACHE;
	i++;

	/*
	 * SRAM range.
	 */
	if (i < FDT_DEVMAP_MAX)
		if (platform_sram_devmap(&fdt_devmap[i]) == 0)
			i++;

	/*
	 * PCI range(s).
	 * PCI range(s) and localbus.
	 */
	if ((root = OF_finddevice("/")) == -1)
		return (ENXIO);
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (fdt_is_type(child, "pci") || fdt_is_type(child, "pciep")) {
			/*
			 * Check space: each PCI node will consume 2 devmap
			 * entries.
			 */
			if (i + 1 >= FDT_DEVMAP_MAX)
				return (ENOMEM);

			/*
			 * XXX this should account for PCI and multiple ranges
			 * of a given kind.
			 */
			if (fdt_pci_devmap(child, &fdt_devmap[i], MV_PCI_VA_IO_BASE,
				    MV_PCI_VA_MEM_BASE) != 0)
				return (ENXIO);
			i += 2;
		}

		if (fdt_is_compatible(child, "mrvl,lbc")) {
			/* Check available space */
			if (OF_getprop(child, "bank-count", (void *)&bank_count,
			    sizeof(bank_count)) <= 0)
				/* If no property, use default value */
				bank_count = 1;
			else
				bank_count = fdt32_to_cpu(bank_count);

			if ((i + bank_count) >= FDT_DEVMAP_MAX)
				return (ENOMEM);

			/* Add all localbus ranges to device map */
			num_mapped = 0;

			if (fdt_localbus_devmap(child, &fdt_devmap[i],
			    (int)bank_count, &num_mapped) != 0)
				return (ENXIO);

			i += num_mapped;
		}
	}

	return (0);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (NULL);
}

int
bus_dma_get_range_nb(void)
{

	return (0);
}

#if defined(CPU_MV_PJ4B)
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(cp15, db_show_cp15)
{
	u_int reg;

	__asm __volatile("mrc p15, 0, %0, c0, c0, 0" : "=r" (reg));
	db_printf("Cpu ID: 0x%08x\n", reg);
	__asm __volatile("mrc p15, 0, %0, c0, c0, 1" : "=r" (reg));
	db_printf("Current Cache Lvl ID: 0x%08x\n",reg);

	__asm __volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (reg));
	db_printf("Ctrl: 0x%08x\n",reg);
	__asm __volatile("mrc p15, 0, %0, c1, c0, 1" : "=r" (reg));
	db_printf("Aux Ctrl: 0x%08x\n",reg);

	__asm __volatile("mrc p15, 0, %0, c0, c1, 0" : "=r" (reg));
	db_printf("Processor Feat 0: 0x%08x\n", reg);
	__asm __volatile("mrc p15, 0, %0, c0, c1, 1" : "=r" (reg));
	db_printf("Processor Feat 1: 0x%08x\n", reg);
	__asm __volatile("mrc p15, 0, %0, c0, c1, 2" : "=r" (reg));
	db_printf("Debug Feat 0: 0x%08x\n", reg);
	__asm __volatile("mrc p15, 0, %0, c0, c1, 3" : "=r" (reg));
	db_printf("Auxiliary Feat 0: 0x%08x\n", reg);
	__asm __volatile("mrc p15, 0, %0, c0, c1, 4" : "=r" (reg));
	db_printf("Memory Model Feat 0: 0x%08x\n", reg);
	__asm __volatile("mrc p15, 0, %0, c0, c1, 5" : "=r" (reg));
	db_printf("Memory Model Feat 1: 0x%08x\n", reg);
	__asm __volatile("mrc p15, 0, %0, c0, c1, 6" : "=r" (reg));
	db_printf("Memory Model Feat 2: 0x%08x\n", reg);
	__asm __volatile("mrc p15, 0, %0, c0, c1, 7" : "=r" (reg));
	db_printf("Memory Model Feat 3: 0x%08x\n", reg);

	__asm __volatile("mrc p15, 1, %0, c15, c2, 0" : "=r" (reg));
	db_printf("Aux Func Modes Ctrl 0: 0x%08x\n",reg);
	__asm __volatile("mrc p15, 1, %0, c15, c2, 1" : "=r" (reg));
	db_printf("Aux Func Modes Ctrl 1: 0x%08x\n",reg);

	__asm __volatile("mrc p15, 1, %0, c15, c12, 0" : "=r" (reg));
	db_printf("CPU ID code extension: 0x%08x\n",reg);
}

DB_SHOW_COMMAND(vtop, db_show_vtop)
{
	u_int reg;

	if (have_addr) {
		__asm __volatile("mcr p15, 0, %0, c7, c8, 0" : : "r" (addr));
		__asm __volatile("mrc p15, 0, %0, c7, c4, 0" : "=r" (reg));
		db_printf("Physical address reg: 0x%08x\n",reg);
	} else
		db_printf("show vtop <virt_addr>\n");
}
#endif /* DDB */
#endif /* CPU_MV_PJ4B */

