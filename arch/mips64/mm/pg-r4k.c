/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 Ralf Baechle (ralf@linux-mips.org)
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/module.h>

#include <asm/cacheops.h>
#include <asm/inst.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/prefetch.h>
#include <asm/system.h>
#include <asm/bootinfo.h>
#include <asm/mipsregs.h>
#include <asm/mmu_context.h>
#include <asm/cpu.h>
#include <asm/war.h>

/*
 * Maximum sizes:
 *
 * R4000 16 bytes D-cache, 128 bytes S-cache:		0x78 bytes
 * R4600 v1.7:						0x5c bytes
 * R4600 v2.0:						0x60 bytes
 * With prefetching, 16 byte strides			0xa0 bytes
 */

static unsigned int clear_page_array[0xa0 / 4];

void clear_page(void * page) __attribute__((alias("clear_page_array")));

/*
 * Maximum sizes:
 *
 * R4000 16 bytes D-cache, 128 bytes S-cache:		0xbc bytes
 * R4600 v1.7:						0x80 bytes
 * R4600 v2.0:						0x84 bytes
 * With prefetching, 16 byte strides			0xb8 bytes
 */
static unsigned int copy_page_array[0xb8 / 4];

void copy_page(void *to, void *from) __attribute__((alias("copy_page_array")));

/*
 * An address fits into a single register so it's safe to use 64-bit registers
 * if we have 64-bit adresses.
 */
#define cpu_has_64bit_registers	cpu_has_64bit_addresses

/*
 * This is suboptimal for 32-bit kernels; we assume that R10000 is only used
 * with 64-bit kernels.  The prefetch offsets have been experimentally tuned
 * an Origin 200.
 */
static int pref_offset_clear __initdata = 512;
static int pref_offset_copy  __initdata = 256;

static unsigned int pref_src_mode __initdata;
static unsigned int pref_dst_mode __initdata;

static int has_scache __initdata = 0;
static int load_offset __initdata = 0;
static int store_offset __initdata = 0;

static unsigned int __initdata *dest, *epc;

static inline void build_src_pref(int advance)
{
	if (!(load_offset & (cpu_dcache_line_size() - 1))) {
		union mips_instruction mi;

		mi.i_format.opcode     = pref_op;
		mi.i_format.rs         = 5;		/* $a1 */
		mi.i_format.rt         = pref_src_mode;
		mi.i_format.simmediate = load_offset + advance;

		*epc++ = mi.word;
	}
}

static inline void __build_load_reg(int reg)
{
	union mips_instruction mi;

	if (cpu_has_64bit_registers)
		mi.i_format.opcode     = ld_op;
	else
		mi.i_format.opcode     = lw_op;
	mi.i_format.rs         = 5;		/* $a1 */
	mi.i_format.rt         = reg;		/* $zero */
	mi.i_format.simmediate = load_offset;

	load_offset += (cpu_has_64bit_registers ? 8 : 4);

	*epc++ = mi.word;
}

static inline void build_load_reg(int reg)
{
	if (cpu_has_prefetch)
		build_src_pref(pref_offset_copy);

	__build_load_reg(reg);
}

static inline void build_dst_pref(int advance)
{
	if (!(store_offset & (cpu_dcache_line_size() - 1))) {
		union mips_instruction mi;

		mi.i_format.opcode     = pref_op;
		mi.i_format.rs         = 4;		/* $a0 */
		mi.i_format.rt         = pref_dst_mode;
		mi.i_format.simmediate = store_offset + advance;

		*epc++ = mi.word;
	}
}

static inline void build_cdex(void)
{
	union mips_instruction mi;

	if (cpu_has_cache_cdex_s &&
	    !(store_offset & (cpu_scache_line_size() - 1))) {

		mi.c_format.opcode     = cache_op;
		mi.c_format.rs         = 4;	/* $a0 */
		mi.c_format.c_op       = 3;	/* Create Dirty Exclusive */
		mi.c_format.cache      = 3;	/* Secondary Data Cache */
		mi.c_format.simmediate = store_offset;

		*epc++ = mi.word;
	}

	if (store_offset & (cpu_dcache_line_size() - 1))
		return;

	if (R4600_V1_HIT_CACHEOP_WAR && ((read_c0_prid() & 0xfff0) == 0x2010)) {
		*epc++ = 0;			/* nop */
		*epc++ = 0;			/* nop */
		*epc++ = 0;			/* nop */
		*epc++ = 0;			/* nop */
	}

	mi.c_format.opcode     = cache_op;
	mi.c_format.rs         = 4;		/* $a0 */
	mi.c_format.c_op       = 3;		/* Create Dirty Exclusive */
	mi.c_format.cache      = 1;		/* Data Cache */
	mi.c_format.simmediate = store_offset;

	*epc++ = mi.word;
}

static inline void __build_store_zero_reg(void)
{
	union mips_instruction mi;

	if (cpu_has_64bits)
		mi.i_format.opcode     = sd_op;
	else
		mi.i_format.opcode     = sw_op;
	mi.i_format.rs         = 4;		/* $a0 */
	mi.i_format.rt         = 0;		/* $zero */
	mi.i_format.simmediate = store_offset;

	store_offset += (cpu_has_64bits ? 8 : 4);

	*epc++ = mi.word;
}

static inline void __build_store_reg(int reg)
{
	union mips_instruction mi;
	int reg_size;

#ifdef CONFIG_MIPS32
	if (cpu_has_64bit_registers && reg == 0) {
		mi.i_format.opcode     = sd_op;
		reg_size               = 8;
	} else {
		mi.i_format.opcode     = sw_op;
		reg_size               = 4;
	}
#endif
#ifdef CONFIG_MIPS64
	mi.i_format.opcode     = sd_op;
	reg_size               = 8;
#endif
	mi.i_format.rs         = 4;		/* $a0 */
	mi.i_format.rt         = reg;		/* $zero */
	mi.i_format.simmediate = store_offset;

	store_offset += reg_size;

	*epc++ = mi.word;
}

static inline void build_store_reg(int reg)
{
	if (cpu_has_prefetch)
		if (reg)
			build_dst_pref(pref_offset_copy);
		else
			build_dst_pref(pref_offset_clear);
	else if (cpu_has_cache_cdex_p)
		build_cdex();

	__build_store_reg(reg);
}

static inline void build_addiu_at_a0(unsigned long offset)
{
	union mips_instruction mi;

	BUG_ON(offset > 0x7fff);

	mi.i_format.opcode     = cpu_has_64bit_addresses ? daddiu_op : addiu_op;
	mi.i_format.rs         = 4;		/* $a0 */
	mi.i_format.rt         = 1;		/* $at */
	mi.i_format.simmediate = offset;

	*epc++ = mi.word;
}

static inline void build_addiu_a1(unsigned long offset)
{
	union mips_instruction mi;

	BUG_ON(offset > 0x7fff);

	mi.i_format.opcode     = cpu_has_64bit_addresses ? daddiu_op : addiu_op;
	mi.i_format.rs         = 5;		/* $a1 */
	mi.i_format.rt         = 5;		/* $a1 */
	mi.i_format.simmediate = offset;

	load_offset -= offset;

	*epc++ = mi.word;
}

static inline void build_addiu_a0(unsigned long offset)
{
	union mips_instruction mi;

	BUG_ON(offset > 0x7fff);

	mi.i_format.opcode     = cpu_has_64bit_addresses ? daddiu_op : addiu_op;
	mi.i_format.rs         = 4;		/* $a0 */
	mi.i_format.rt         = 4;		/* $a0 */
	mi.i_format.simmediate = offset;

	store_offset -= offset;

	*epc++ = mi.word;
}

static inline void build_bne(unsigned int *dest)
{
	union mips_instruction mi;

	mi.i_format.opcode = bne_op;
	mi.i_format.rs     = 1;			/* $at */
	mi.i_format.rt     = 4;			/* $a0 */
	mi.i_format.simmediate = dest - epc - 1;

	*epc++ = mi.word;
}

static inline void build_nop(void)
{
	*epc++ = 0;
}

static inline void build_jr_ra(void)
{
	union mips_instruction mi;

	mi.r_format.opcode = spec_op;
	mi.r_format.rs     = 31;
	mi.r_format.rt     = 0;
	mi.r_format.rd     = 0;
	mi.r_format.re     = 0;
	mi.r_format.func   = jr_op;

	*epc++ = mi.word;
}

void __init build_clear_page(void)
{
	epc = (unsigned int *) &clear_page_array;

	if (cpu_has_prefetch) {
		switch (current_cpu_data.cputype) {
		case CPU_R10000:
		case CPU_R12000:
			pref_src_mode = Pref_LoadStreamed;
			pref_dst_mode = Pref_StoreRetained;
			break;
		default:
			pref_src_mode = Pref_LoadStreamed;
			pref_dst_mode = Pref_PrepareForStore;
			break;
		}
	}

	build_addiu_at_a0(PAGE_SIZE - (cpu_has_prefetch ? pref_offset_clear : 0));

	if (R4600_V2_HIT_CACHEOP_WAR && ((read_c0_prid() & 0xfff0) == 0x2020)) {
		*epc++ = 0x40026000;		/* mfc0    $v0, $12	*/
		*epc++ = 0x34410001;		/* ori     $at, v0, 0x1	*/
		*epc++ = 0x38210001;		/* xori    $at, at, 0x1	*/
		*epc++ = 0x40816000;		/* mtc0    $at, $12	*/
		*epc++ = 0x00000000;		/* nop			*/
		*epc++ = 0x00000000;		/* nop			*/
		*epc++ = 0x00000000;		/* nop			*/
		*epc++ = 0x3c01a000;		/* lui     $at, 0xa000  */
		*epc++ = 0x8c200000;		/* lw      $zero, ($at) */
	}

dest = epc;
	build_store_reg(0);
	build_store_reg(0);
	build_store_reg(0);
	build_store_reg(0);
	if (has_scache && cpu_scache_line_size() == 128) {
		build_store_reg(0);
		build_store_reg(0);
		build_store_reg(0);
		build_store_reg(0);
	}
	build_addiu_a0(2 * store_offset);
	build_store_reg(0);
	build_store_reg(0);
	if (has_scache && cpu_scache_line_size() == 128) {
		build_store_reg(0);
		build_store_reg(0);
		build_store_reg(0);
		build_store_reg(0);
	}
	build_store_reg(0);
	build_bne(dest);
	 build_store_reg(0);

	if (cpu_has_prefetch && pref_offset_clear) {
		build_addiu_at_a0(pref_offset_clear);
	dest = epc;
		__build_store_reg(0);
		__build_store_reg(0);
		__build_store_reg(0);
		__build_store_reg(0);
		build_addiu_a0(2 * store_offset);
		__build_store_reg(0);
		__build_store_reg(0);
		__build_store_reg(0);
		build_bne(dest);
		 __build_store_reg(0);
	}

	build_jr_ra();
	if (R4600_V2_HIT_CACHEOP_WAR && ((read_c0_prid() & 0xfff0) == 0x2020))
		*epc++ = 0x40826000;		/* mtc0    $v0, $12	*/
	else
		build_nop();

	flush_icache_range((unsigned long)&clear_page_array,
	                   (unsigned long) epc);

	BUG_ON(epc > clear_page_array + ARRAY_SIZE(clear_page_array));
}

void __init build_copy_page(void)
{
	epc = (unsigned int *) &copy_page_array;

	build_addiu_at_a0(PAGE_SIZE - (cpu_has_prefetch ? pref_offset_copy : 0));

	if (R4600_V2_HIT_CACHEOP_WAR && ((read_c0_prid() & 0xfff0) == 0x2020)) {
		*epc++ = 0x40026000;		/* mfc0    $v0, $12	*/
		*epc++ = 0x34410001;		/* ori     $at, v0, 0x1	*/
		*epc++ = 0x38210001;		/* xori    $at, at, 0x1	*/
		*epc++ = 0x40816000;		/* mtc0    $at, $12	*/
		*epc++ = 0x00000000;		/* nop			*/
		*epc++ = 0x00000000;		/* nop			*/
		*epc++ = 0x00000000;		/* nop			*/
		*epc++ = 0x3c01a000;		/* lui     $at, 0xa000  */
		*epc++ = 0x8c200000;		/* lw      $zero, ($at) */
	}

dest = epc;
	build_load_reg( 8);
	build_load_reg( 9);
	build_load_reg(10);
	build_load_reg(11);
	build_store_reg( 8);
	build_store_reg( 9);
	build_store_reg(10);
	build_store_reg(11);
	if (has_scache && cpu_scache_line_size() == 128) {
		build_load_reg( 8);
		build_load_reg( 9);
		build_load_reg(10);
		build_load_reg(11);
		build_store_reg( 8);
		build_store_reg( 9);
		build_store_reg(10);
		build_store_reg(11);
	}
	build_addiu_a0(2 * store_offset);
	build_addiu_a1(2 * load_offset);
	build_load_reg( 8);
	build_load_reg( 9);
	build_load_reg(10);
	build_load_reg(11);
	build_store_reg( 8);
	build_store_reg( 9);
	build_store_reg(10);
	if (has_scache && cpu_scache_line_size() == 128) {
		build_store_reg(11);
		build_load_reg( 8);
		build_load_reg( 9);
		build_load_reg(10);
		build_load_reg(11);
		build_store_reg( 8);
		build_store_reg( 9);
		build_store_reg(10);
	}
	build_bne(dest);
	 build_store_reg(11);

	if (cpu_has_prefetch && pref_offset_copy) {
		build_addiu_at_a0(pref_offset_copy);
	dest = epc;
		__build_load_reg( 8);
		__build_load_reg( 9);
		__build_load_reg(10);
		__build_load_reg(11);
		__build_store_reg( 8);
		__build_store_reg( 9);
		__build_store_reg(10);
		__build_store_reg(11);
		build_addiu_a0(2 * store_offset);
		build_addiu_a1(2 * load_offset);
		__build_load_reg( 8);
		__build_load_reg( 9);
		__build_load_reg(10);
		__build_load_reg(11);
		__build_store_reg( 8);
		__build_store_reg( 9);
		__build_store_reg(10);
		build_bne(dest);
		 __build_store_reg(11);
	}

	build_jr_ra();
	if (R4600_V2_HIT_CACHEOP_WAR && ((read_c0_prid() & 0xfff0) == 0x2020))
		*epc++ = 0x40826000;		/* mtc0    $v0, $12	*/
	else
		build_nop();

	BUG_ON(epc > copy_page_array + ARRAY_SIZE(copy_page_array));
}
