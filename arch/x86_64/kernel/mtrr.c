/*  x86-64 MTRR (Memory Type Range Register) driver.
	Based largely upon arch/i386/kernel/mtrr.c

	Copyright (C) 1997-2000  Richard Gooch
	Copyright (C) 2002 Dave Jones.

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the Free
	Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	(For earlier history, see arch/i386/kernel/mtrr.c)
	v2.00	September 2001	Dave Jones <davej@suse.de>
	  Initial rewrite for x86-64.
	  Removal of non-Intel style MTRR code.
	v2.01  June 2002  Dave Jones <davej@suse.de>
	  Removal of redundant abstraction layer.
	  64-bit fixes.
	v2.02  July 2002  Dave Jones <davej@suse.de>
	  Fix gentry inconsistencies between kernel/userspace.
	  More casts to clean up warnings.
*/

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#define MTRR_NEED_STRINGS
#include <asm/mtrr.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/agp_backend.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/atomic.h>
#include <asm/msr.h>

#include <asm/hardirq.h>
#include <linux/irq.h>

#define MTRR_VERSION "2.02 (20020716)"

#define MTRR_BEG_BIT 12
#define MTRR_END_BIT 7

#undef Dprintk

#define Dprintk(...) 

#define TRUE  1
#define FALSE 0

#define MSR_MTRRphysBase(reg) (0x200 + 2 * (reg))
#define MSR_MTRRphysMask(reg) (0x200 + 2 * (reg) + 1)

#define NUM_FIXED_RANGES 88

#define MTRR_CHANGE_MASK_FIXED 0x01
#define MTRR_CHANGE_MASK_VARIABLE 0x02
#define MTRR_CHANGE_MASK_DEFTYPE 0x04

typedef u8 mtrr_type;

#define LINE_SIZE 80

#ifdef CONFIG_SMP
#define set_mtrr(reg,base,size,type) set_mtrr_smp (reg, base, size, type)
#else
#define set_mtrr(reg,base,size,type) set_mtrr_up (reg, base, size, type, TRUE)
#endif

#if defined(CONFIG_PROC_FS) || defined(CONFIG_DEVFS_FS)
#define USERSPACE_INTERFACE
#endif

#ifdef USERSPACE_INTERFACE
static char *ascii_buffer;
static unsigned int ascii_buf_bytes;
static void compute_ascii (void);
#else
#define compute_ascii() while (0)
#endif

static unsigned int *usage_table;
static DECLARE_MUTEX (mtrr_lock);

struct set_mtrr_context {
	u32 deftype_lo;
	u32 deftype_hi;
	unsigned long flags;
	u64 cr4val;
};


/*  Put the processor into a state where MTRRs can be safely set  */
static void set_mtrr_prepare (struct set_mtrr_context *ctxt)
{
	u64 cr0;

	/* Disable interrupts locally */
	__save_flags(ctxt->flags);
	__cli();

	/* Save value of CR4 and clear Page Global Enable (bit 7) */
	if (test_bit(X86_FEATURE_PGE, boot_cpu_data.x86_capability)) {
		ctxt->cr4val = read_cr4();
		write_cr4(ctxt->cr4val & ~(1UL << 7));
	}

	/* Disable and flush caches. Note that wbinvd flushes the TLBs as
	   a side-effect */
	cr0 = read_cr0() | 0x40000000;
	wbinvd();
	write_cr0(cr0);
	wbinvd();

	rdmsr(MSR_MTRRdefType, ctxt->deftype_lo, ctxt->deftype_hi);
}

static void set_mtrr_disable(struct set_mtrr_context *ctxt) 
{ 
	/* Disable MTRRs, and set the default type to uncached */
	wrmsr(MSR_MTRRdefType, ctxt->deftype_lo & 0xf300UL, ctxt->deftype_hi);
} 

/* Restore the processor after a set_mtrr_prepare */
static void set_mtrr_done (struct set_mtrr_context *ctxt)
{
	/* Flush caches and TLBs */
	wbinvd();

	/* Restore MTRRdefType */
	wrmsr(MSR_MTRRdefType, ctxt->deftype_lo, ctxt->deftype_hi);

	/* Enable caches */
	write_cr0(read_cr0() & 0xbfffffff);

	/* Restore value of CR4 */
	if (test_bit(X86_FEATURE_PGE, boot_cpu_data.x86_capability))
		write_cr4 (ctxt->cr4val);

	/* Re-enable interrupts locally (if enabled previously) */
	__restore_flags(ctxt->flags);
}


/*  This function returns the number of variable MTRRs  */
static unsigned int get_num_var_ranges (void)
{
	u32 config, dummy;

	rdmsr (MSR_MTRRcap, config, dummy);
	return (config & 0xff);
}


/*  Returns non-zero if we have the write-combining memory type  */
static int have_wrcomb (void)
{
	u32 config, dummy;

	rdmsr (MSR_MTRRcap, config, dummy);
	return (config & (1 << 10));
}


static u64 size_or_mask, size_and_mask;

static void get_mtrr (unsigned int reg, u64 *base, u32 *size, mtrr_type * type)
{
	u32 count, tmp, mask_lo, mask_hi;
	int i;
	u32 base_lo, base_hi;

	rdmsr (MSR_MTRRphysMask(reg), mask_lo, mask_hi);
	if ((mask_lo & 0x800) == 0) {
		/*  Invalid (i.e. free) range  */
		*base = 0;
		*size = 0;
		*type = 0;
		return;
	}

	rdmsr (MSR_MTRRphysBase(reg), base_lo, base_hi);

	count = 0;
	tmp = mask_lo >> MTRR_BEG_BIT;
	for (i = MTRR_BEG_BIT; i <= 31; i++, tmp = tmp >> 1)
		count = (count << (~tmp & 1)) | (~tmp & 1);
	
	tmp = mask_hi;
	for (i = 0; i <= MTRR_END_BIT; i++, tmp = tmp >> 1)
		count = (count << (~tmp & 1)) | (~tmp & 1);
	
	*size = (count+1); 
	*base = base_hi << (32 - PAGE_SHIFT) | base_lo >> PAGE_SHIFT;
	*type = base_lo & 0xff;
}



/*
 * Set variable MTRR register on the local CPU.
 *  <reg> The register to set.
 *  <base> The base address of the region.
 *  <size> The size of the region. If this is 0 the region is disabled.
 *  <type> The type of the region.
 *  <do_safe> If TRUE, do the change safely. If FALSE, safety measures should
 *  be done externally.
 */
static void set_mtrr_up (unsigned int reg, u64 base,
		   u32 size, mtrr_type type, int do_safe)
{
	struct set_mtrr_context ctxt;
	u64 base64;
	u64 size64;

	if (do_safe) { 
		set_mtrr_prepare (&ctxt);
		set_mtrr_disable (&ctxt); 
	} 

	if (size == 0) {
		/* The invalid bit is kept in the mask, so we simply clear the
		   relevant mask register to disable a range. */
		wrmsr (MSR_MTRRphysMask(reg), 0, 0);
	} else {
		base64 = (base << PAGE_SHIFT) & size_and_mask;
		wrmsr (MSR_MTRRphysBase(reg), base64 | type, base64 >> 32);

		size64 = ~(((u64)size << PAGE_SHIFT) - 1);
		size64 = size64 & size_and_mask;
		wrmsr (MSR_MTRRphysMask(reg), (u32) (size64 | 0x800), (u32) (size64 >> 32));
	}
	if (do_safe)
		set_mtrr_done (&ctxt);
}


#ifdef CONFIG_SMP

struct mtrr_var_range {
	u32 base_lo;
	u32 base_hi;
	u32 mask_lo;
	u32 mask_hi;
};

/*  Get the MSR pair relating to a var range  */
static void __init get_mtrr_var_range (unsigned int index,
		struct mtrr_var_range *vr)
{
	rdmsr (MSR_MTRRphysBase(index), vr->base_lo, vr->base_hi);
	rdmsr (MSR_MTRRphysMask(index), vr->mask_lo, vr->mask_hi);
}


/*  Set the MSR pair relating to a var range. Returns TRUE if
    changes are made  */
static int __init set_mtrr_var_range_testing (unsigned int index,
		struct mtrr_var_range *vr)
{
	u32 lo, hi;
	int changed = FALSE;

	rdmsr (MSR_MTRRphysBase(index), lo, hi);
	if ((vr->base_lo & 0xfffff0ff) != (lo & 0xfffff0ff) ||
		(vr->base_hi & 0x000fffff) != (hi & 0x000fffff)) {
		wrmsr (MSR_MTRRphysBase(index), vr->base_lo, vr->base_hi);
		changed = TRUE;
	}

	rdmsr (MSR_MTRRphysMask(index), lo, hi);
	if ((vr->mask_lo & 0xfffff800) != (lo & 0xfffff800) ||
		(vr->mask_hi & 0x000fffff) != (hi & 0x000fffff)) {
		wrmsr (MSR_MTRRphysMask(index), vr->mask_lo, vr->mask_hi);
		changed = TRUE;
	}
	return changed;
}


static void __init get_fixed_ranges (mtrr_type * frs)
{
	u32 *p = (u32 *) frs;
	int i;

	rdmsr (MSR_MTRRfix64K_00000, p[0], p[1]);

	for (i = 0; i < 2; i++)
		rdmsr (MSR_MTRRfix16K_80000 + i, p[2 + i * 2], p[3 + i * 2]);
	for (i = 0; i < 8; i++)
		rdmsr (MSR_MTRRfix4K_C0000 + i, p[6 + i * 2], p[7 + i * 2]);
}


static int __init set_fixed_ranges_testing (mtrr_type * frs)
{
	u32 *p = (u32 *) frs;
	int changed = FALSE;
	int i;
	u32 lo, hi;

	Dprintk (KERN_INFO "mtrr: rdmsr 64K_00000\n");
	rdmsr (MSR_MTRRfix64K_00000, lo, hi);
	if (p[0] != lo || p[1] != hi) {
		Dprintk (KERN_INFO "mtrr: Writing %x:%x to 64K MSR. lohi were %x:%x\n", p[0], p[1], lo, hi);
		wrmsr (MSR_MTRRfix64K_00000, p[0], p[1]);
		changed = TRUE;
	}

	Dprintk (KERN_INFO "mtrr: rdmsr 16K_80000\n");
	for (i = 0; i < 2; i++) {
		rdmsr (MSR_MTRRfix16K_80000 + i, lo, hi);
		if (p[2 + i * 2] != lo || p[3 + i * 2] != hi) {
			Dprintk (KERN_INFO "mtrr: Writing %x:%x to 16K MSR%d. lohi were %x:%x\n", p[2 + i * 2], p[3 + i * 2], i, lo, hi );
			wrmsr (MSR_MTRRfix16K_80000 + i, p[2 + i * 2], p[3 + i * 2]);
			changed = TRUE;
		}
	}

	Dprintk (KERN_INFO "mtrr: rdmsr 4K_C0000\n");
	for (i = 0; i < 8; i++) {
		rdmsr (MSR_MTRRfix4K_C0000 + i, lo, hi);
		Dprintk (KERN_INFO "mtrr: MTRRfix4K_C0000+%d = %x:%x\n", i, lo, hi);
		if (p[6 + i * 2] != lo || p[7 + i * 2] != hi) {
			Dprintk (KERN_INFO "mtrr: Writing %x:%x to 4K MSR%d. lohi were %x:%x\n", p[6 + i * 2], p[7 + i * 2], i, lo, hi);
			wrmsr (MSR_MTRRfix4K_C0000 + i, p[6 + i * 2], p[7 + i * 2]);
			changed = TRUE;
		}
	}
	return changed;
}


struct mtrr_state {
	unsigned int num_var_ranges;
	struct mtrr_var_range *var_ranges;
	mtrr_type fixed_ranges[NUM_FIXED_RANGES];
	mtrr_type def_type;
	unsigned char enabled;
};


/*  Grab all of the MTRR state for this CPU into *state  */
static void __init get_mtrr_state (struct mtrr_state *state)
{
	unsigned int nvrs, i;
	struct mtrr_var_range *vrs;
	u32 lo, dummy;

	nvrs = state->num_var_ranges = get_num_var_ranges();
	vrs = state->var_ranges
	    = kmalloc (nvrs * sizeof (struct mtrr_var_range), GFP_KERNEL);
	if (vrs == NULL)
		nvrs = state->num_var_ranges = 0;

	for (i = 0; i < nvrs; i++)
		get_mtrr_var_range (i, &vrs[i]);
	get_fixed_ranges (state->fixed_ranges);

	rdmsr (MSR_MTRRdefType, lo, dummy);
	state->def_type = (lo & 0xff);
	state->enabled = (lo & 0xc00) >> 10;
}


/*  Free resources associated with a struct mtrr_state  */
static void __init finalize_mtrr_state (struct mtrr_state *state)
{
	if (state->var_ranges)
		kfree (state->var_ranges);
}


/*
 * Set the MTRR state for this CPU.
 *  <state> The MTRR state information to read.
 *  <ctxt> Some relevant CPU context.
 *  [NOTE] The CPU must already be in a safe state for MTRR changes.
 *  [RETURNS] 0 if no changes made, else a mask indication what was changed.
 */
static u64 __init set_mtrr_state (struct mtrr_state *state,
		struct set_mtrr_context *ctxt)
{
	unsigned int i;
	u64 change_mask = 0;

	for (i = 0; i < state->num_var_ranges; i++)
		if (set_mtrr_var_range_testing (i, &state->var_ranges[i]))
			change_mask |= MTRR_CHANGE_MASK_VARIABLE;

	if (set_fixed_ranges_testing (state->fixed_ranges))
		change_mask |= MTRR_CHANGE_MASK_FIXED;
	/* Set_mtrr_restore restores the old value of MTRRdefType,
	   so to set it we fiddle with the saved value  */
	if ((ctxt->deftype_lo & 0xff) != state->def_type
	    || ((ctxt->deftype_lo & 0xc00) >> 10) != state->enabled) {
		ctxt->deftype_lo |= (state->def_type | state->enabled << 10);
		change_mask |= MTRR_CHANGE_MASK_DEFTYPE;
	}

	return change_mask;
}


static atomic_t undone_count;
static volatile int wait_barrier_mtrr_disable = FALSE;
static volatile int wait_barrier_execute = FALSE;
static volatile int wait_barrier_cache_enable = FALSE;

struct set_mtrr_data {
	u64 smp_base;
	u32 smp_size;
	unsigned int smp_reg;
	mtrr_type smp_type;
};

/*
 * Synchronisation handler. Executed by "other" CPUs.
 */
static void ipi_handler (void *info)
{
	struct set_mtrr_data *data = info;
	struct set_mtrr_context ctxt;

	set_mtrr_prepare (&ctxt);
	/* Notify master that I've flushed and disabled my cache  */
	atomic_dec (&undone_count);

	while (wait_barrier_mtrr_disable) {
		rep_nop();
		barrier ();
	}	
	set_mtrr_disable (&ctxt); 
	/* wait again for disable confirmation*/
	atomic_dec (&undone_count);
	while (wait_barrier_execute) { 
		rep_nop(); 
		barrier(); 
	}	

	/* The master has cleared me to execute  */
	set_mtrr_up (data->smp_reg, data->smp_base, data->smp_size,
			data->smp_type, FALSE);

	/* Notify master CPU that I've executed the function  */
	atomic_dec (&undone_count);

	/* Wait for master to clear me to enable cache and return  */
	while (wait_barrier_cache_enable) {
		rep_nop();
		barrier ();
	}	
	set_mtrr_done (&ctxt);
}


static void set_mtrr_smp (unsigned int reg, u64 base, u32 size, mtrr_type type)
{
	struct set_mtrr_data data;
	struct set_mtrr_context ctxt;

	data.smp_reg = reg;
	data.smp_base = base;
	data.smp_size = size;
	data.smp_type = type;
	wait_barrier_execute = TRUE;
	wait_barrier_cache_enable = TRUE;
	wait_barrier_mtrr_disable = TRUE;
	atomic_set (&undone_count, smp_num_cpus - 1);

	/*  Start the ball rolling on other CPUs  */
	if (smp_call_function (ipi_handler, &data, 1, 0) != 0)
		panic ("mtrr: timed out waiting for other CPUs\n");

	/* Flush and disable the local CPU's cache */
	set_mtrr_prepare (&ctxt);
	while (atomic_read (&undone_count) > 0) { 
		rep_nop();
		barrier(); 
	}

	/* Set up for completion wait and then release other CPUs to change MTRRs*/
	atomic_set (&undone_count, smp_num_cpus - 1);
	wait_barrier_mtrr_disable = FALSE;
	set_mtrr_disable (&ctxt);

	/*  Wait for all other CPUs to flush and disable their caches  */
	while (atomic_read (&undone_count) > 0) { 
		rep_nop ();
		barrier ();
	}	

	/* Set up for completion wait and then release other CPUs to change MTRRs */
	atomic_set (&undone_count, smp_num_cpus - 1);
	wait_barrier_execute = FALSE;
	set_mtrr_up (reg, base, size, type, FALSE);

	/*  Now wait for other CPUs to complete the function  */
	while (atomic_read (&undone_count) > 0) {
		rep_nop();
		barrier ();
	} 	

	/*  Now all CPUs should have finished the function. Release the barrier to
	   allow them to re-enable their caches and return from their interrupt,
	   then enable the local cache and return  */
	wait_barrier_cache_enable = FALSE;
	set_mtrr_done (&ctxt);
}


/*  Some BIOS's are fucked and don't set all MTRRs the same!  */
static void __init mtrr_state_warn (u32 mask)
{
	if (!mask)
		return;
	if (mask & MTRR_CHANGE_MASK_FIXED)
		printk (KERN_INFO "mtrr: your CPUs had inconsistent fixed MTRR settings\n");
	if (mask & MTRR_CHANGE_MASK_VARIABLE)
		printk (KERN_INFO "mtrr: your CPUs had inconsistent variable MTRR settings\n");
	if (mask & MTRR_CHANGE_MASK_DEFTYPE)
		printk (KERN_INFO "mtrr: your CPUs had inconsistent MTRRdefType settings\n");
	printk (KERN_INFO "mtrr: probably your BIOS does not setup all CPUs\n");
}

#endif	/*  CONFIG_SMP  */


static inline char * attrib_to_str (int x)
{
	return (x <= 6) ? mtrr_strings[x] : "?";
}


static void __init init_table (void)
{
	int i, max;

	max = get_num_var_ranges ();
	if ((usage_table = kmalloc (max * sizeof *usage_table, GFP_KERNEL))==NULL) {
		printk ("mtrr: could not allocate\n");
		return;
	}

	for (i = 0; i < max; i++)
		usage_table[i] = 1;

#ifdef USERSPACE_INTERFACE
	if ((ascii_buffer = kmalloc (max * LINE_SIZE, GFP_KERNEL)) == NULL) {
		printk ("mtrr: could not allocate\n");
		return;
	}
	ascii_buf_bytes = 0;
	compute_ascii ();
#endif
}


/*
 * Get a free MTRR.
 * returns the index of the region on success, else -1 on error.
*/
static int get_free_region(void)
{
	int i, max;
	mtrr_type ltype;
	u64 lbase;
	u32 lsize;

	max = get_num_var_ranges ();
	for (i = 0; i < max; ++i) {
		get_mtrr (i, &lbase, &lsize, &ltype);
		if (lsize == 0)
			return i;
	}
	return -ENOSPC;
}


/**
 *	mtrr_add_page - Add a memory type region
 *	@base: Physical base address of region in pages (4 KB)
 *	@size: Physical size of region in pages (4 KB)
 *	@type: Type of MTRR desired
 *	@increment: If this is true do usage counting on the region
 *	Returns The MTRR register on success, else a negative number
 *	indicating the error code.
 *
 *	Memory type region registers control the caching on newer
 *	processors. This function allows drivers to request an MTRR is added.
 *	The caller should expect to need to provide a power of two size on
 *	an equivalent power of two boundary.
 *
 *	If the region cannot be added either because all regions are in use
 *	or the CPU cannot support it a negative value is returned. On success
 *	the register number for this entry is returned, but should be treated
 *	as a cookie only.
 *
 *	On a multiprocessor machine the changes are made to all processors.
 *
 *	The available types are
 *
 *	%MTRR_TYPE_UNCACHABLE	-	No caching
 *	%MTRR_TYPE_WRBACK	-	Write data back in bursts whenever
 *	%MTRR_TYPE_WRCOMB	-	Write data back soon but allow bursts
 *	%MTRR_TYPE_WRTHROUGH	-	Cache reads but not writes
 *
 *	BUGS: Needs a quiet flag for the cases where drivers do not mind
 *	failures and do not wish system log messages to be sent.
 */

int mtrr_add_page (u64 base, u32 size, unsigned int type, char increment)
{
	int i, max;
	mtrr_type ltype;
	u64 lbase, last;
	u32 lsize;

	if (base + size < 0x100) {
		printk (KERN_WARNING
			"mtrr: cannot set region below 1 MiB (0x%Lx000,0x%x000)\n",
			base, size);
		return -EINVAL;
	}

#if 0 && defined(__x86_64__) && defined(CONFIG_AGP) 
	{
	agp_kern_info info; 
	if (type != MTRR_TYPE_UNCACHABLE && agp_copy_info(&info) >= 0 && 
	    base<<PAGE_SHIFT >= info.aper_base && 
            (base<<PAGE_SHIFT)+(size<<PAGE_SHIFT) >= 
			info.aper_base+info.aper_size*1024*1024)
		printk(KERN_INFO "%s[%d] setting conflicting mtrr into agp aperture\n",current->comm,current->pid); 
	}
#endif

	/*  Check upper bits of base and last are equal and lower bits are 0
	   for base and 1 for last  */
	last = base + size - 1;
	for (lbase = base; !(lbase & 1) && (last & 1);
	     lbase = lbase >> 1, last = last >> 1) ;

	if (lbase != last) {
		printk (KERN_WARNING
			"mtrr: base(0x%Lx000) is not aligned on a size(0x%x000) boundary\n",
			base, size);
		return -EINVAL;
	}

	if (type >= MTRR_NUM_TYPES) {
		printk ("mtrr: type: %u illegal\n", type);
		return -EINVAL;
	}

	/*  If the type is WC, check that this processor supports it  */
	if ((type == MTRR_TYPE_WRCOMB) && !have_wrcomb()) {
		printk (KERN_WARNING
			"mtrr: your processor doesn't support write-combining\n");
		return -ENOSYS;
	}

	if (base & (size_or_mask>>PAGE_SHIFT)) {
		printk (KERN_WARNING "mtrr: base(%Lx) exceeds the MTRR width(%Lx)\n",
				base, (size_or_mask>>PAGE_SHIFT));
		return -EINVAL;
	}

	if (size & (size_or_mask>>PAGE_SHIFT)) {
		printk (KERN_WARNING "mtrr: size exceeds the MTRR width\n");
		return -EINVAL;
	}

	increment = increment ? 1 : 0;
	max = get_num_var_ranges ();
	/*  Search for existing MTRR  */
	down (&mtrr_lock);
	for (i = 0; i < max; ++i) {
		get_mtrr (i, &lbase, &lsize, &ltype);
		if (base >= lbase + lsize)
			continue;
		if ((base < lbase) && (base + size <= lbase))
			continue;

		/*  At this point we know there is some kind of overlap/enclosure  */
		if ((base < lbase) || (base + size > lbase + lsize)) {
			up (&mtrr_lock);
			printk (KERN_WARNING
				"mtrr: 0x%Lx000,0x%x000 overlaps existing"
				" 0x%Lx000,0x%x000\n", base, size, lbase, lsize);
			return -EINVAL;
		}
		/*  New region is enclosed by an existing region  */
		if (ltype != type) {
			if (type == MTRR_TYPE_UNCACHABLE)
				continue;
			up (&mtrr_lock);
			printk
			    ("mtrr: type mismatch for %Lx000,%x000 old: %s new: %s\n",
			     base, size,
				 attrib_to_str (ltype),
			     attrib_to_str (type));
			return -EINVAL;
		}
		if (increment)
			++usage_table[i];
		compute_ascii ();
		up (&mtrr_lock);
		return i;
	}
	/*  Search for an empty MTRR  */
	i = get_free_region();
	if (i < 0) {
		up (&mtrr_lock);
		printk ("mtrr: no more MTRRs available\n");
		return i;
	}
	set_mtrr (i, base, size, type);
	usage_table[i] = 1;
	compute_ascii ();
	up (&mtrr_lock);
	return i;
}


/**
 *	mtrr_add - Add a memory type region
 *	@base: Physical base address of region
 *	@size: Physical size of region
 *	@type: Type of MTRR desired
 *	@increment: If this is true do usage counting on the region
 *	Return the MTRR register on success, else a negative numbe
 *	indicating the error code.
 *
 *	Memory type region registers control the caching on newer processors.
 *	This function allows drivers to request an MTRR is added.
 *	The caller should expect to need to provide a power of two size on
 *	an equivalent power of two boundary.
 *
 *	If the region cannot be added either because all regions are in use
 *	or the CPU cannot support it a negative value is returned. On success
 *	the register number for this entry is returned, but should be treated
 *	as a cookie only.
 *
 *	On a multiprocessor machine the changes are made to all processors.
 *	This is required on x86 by the Intel processors.
 *
 *	The available types are
 *
 *	%MTRR_TYPE_UNCACHABLE	-	No caching
 *	%MTRR_TYPE_WRBACK	-	Write data back in bursts whenever
 *	%MTRR_TYPE_WRCOMB	-	Write data back soon but allow bursts
 *	%MTRR_TYPE_WRTHROUGH	-	Cache reads but not writes
 *
 *	BUGS: Needs a quiet flag for the cases where drivers do not mind
 *	failures and do not wish system log messages to be sent.
 */

int mtrr_add (u64 base, u32 size, unsigned int type, char increment)
{
	if ((base & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
		printk ("mtrr: size and base must be multiples of 4 kiB\n");
		printk ("mtrr: size: 0x%x  base: 0x%Lx\n", size, base);
		return -EINVAL;
	}
	return mtrr_add_page (base >> PAGE_SHIFT, size >> PAGE_SHIFT, type,
			      increment);
}


/**
 *	mtrr_del_page - delete a memory type region
 *	@reg: Register returned by mtrr_add
 *	@base: Physical base address
 *	@size: Size of region
 *
 *	If register is supplied then base and size are ignored. This is
 *	how drivers should call it.
 *
 *	Releases an MTRR region. If the usage count drops to zero the 
 *	register is freed and the region returns to default state.
 *	On success the register is returned, on failure a negative error
 *	code.
 */

int mtrr_del_page (int reg, u64 base, u32 size)
{
	int i, max;
	mtrr_type ltype;
	u64 lbase;
	u32 lsize;

	max = get_num_var_ranges ();
	down (&mtrr_lock);
	if (reg < 0) {
		/*  Search for existing MTRR  */
		for (i = 0; i < max; ++i) {
			get_mtrr (i, &lbase, &lsize, &ltype);
			if (lbase == base && lsize == size) {
				reg = i;
				break;
			}
		}
		if (reg < 0) {
			up (&mtrr_lock);
			printk ("mtrr: no MTRR for %Lx000,%x000 found\n", base, size);
			return -EINVAL;
		}
	}

	if (reg >= max) {
		up (&mtrr_lock);
		printk ("mtrr: register: %d too big\n", reg);
		return -EINVAL;
	}
	get_mtrr (reg, &lbase, &lsize, &ltype);

	if (lsize < 1) {
		up (&mtrr_lock);
		printk ("mtrr: MTRR %d not used\n", reg);
		return -EINVAL;
	}

	if (usage_table[reg] < 1) {
		up (&mtrr_lock);
		printk ("mtrr: reg: %d has count=0\n", reg);
		return -EINVAL;
	}

	if (--usage_table[reg] < 1)
		set_mtrr (reg, 0, 0, 0);
	compute_ascii ();
	up (&mtrr_lock);
	return reg;
}


/**
 *	mtrr_del - delete a memory type region
 *	@reg: Register returned by mtrr_add
 *	@base: Physical base address
 *	@size: Size of region
 *
 *	If register is supplied then base and size are ignored. This is
 *	how drivers should call it.
 *
 *	Releases an MTRR region. If the usage count drops to zero the 
 *	register is freed and the region returns to default state.
 *	On success the register is returned, on failure a negative error
 *	code.
 */

int mtrr_del (int reg, u64 base, u32 size)
{
	if ((base & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
		printk ("mtrr: size and base must be multiples of 4 kiB\n");
		printk ("mtrr: size: 0x%x  base: 0x%Lx\n", size, base);
		return -EINVAL;
	}
	return mtrr_del_page (reg, base >> PAGE_SHIFT, size >> PAGE_SHIFT);
}


#ifdef USERSPACE_INTERFACE

static int mtrr_file_add (u64 base, u32 size, unsigned int type,
		struct file *file, int page)
{
	int reg, max;
	unsigned int *fcount = file->private_data;

	max = get_num_var_ranges ();
	if (fcount == NULL) {
		if ((fcount =
		     kmalloc (max * sizeof *fcount, GFP_KERNEL)) == NULL) {
			printk ("mtrr: could not allocate\n");
			return -ENOMEM;
		}
		memset (fcount, 0, max * sizeof *fcount);
		file->private_data = fcount;
	}

	if (!page) {
		if ((base & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
			printk
			    (KERN_INFO "mtrr: size and base must be multiples of 4 kiB\n");
			printk (KERN_INFO "mtrr: size: 0x%x  base: 0x%Lx\n", size, base);
			return -EINVAL;
		}
		base >>= PAGE_SHIFT;
		size >>= PAGE_SHIFT;
	}

	reg = mtrr_add_page (base, size, type, 1);

	if (reg >= 0)
		++fcount[reg];
	return reg;
}


static int mtrr_file_del (u64 base, u32 size,
		struct file *file, int page)
{
	int reg;
	unsigned int *fcount = file->private_data;

	if (!page) {
		if ((base & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
			printk
			    (KERN_INFO "mtrr: size and base must be multiples of 4 kiB\n");
			printk (KERN_INFO "mtrr: size: 0x%x  base: 0x%Lx\n", size, base);
			return -EINVAL;
		}
		base >>= PAGE_SHIFT;
		size >>= PAGE_SHIFT;
	}
	reg = mtrr_del_page (-1, base, size);
	if (reg < 0)
		return reg;
	if (fcount == NULL)
		return reg;
	if (fcount[reg] < 1)
		return -EINVAL;
	--fcount[reg];
	return reg;
}


static ssize_t mtrr_read (struct file *file, char *buf, size_t len,
		loff_t * ppos)
{
	if (*ppos >= ascii_buf_bytes)
		return 0;

	if (*ppos + len > ascii_buf_bytes)
		len = ascii_buf_bytes - *ppos;

	if (copy_to_user (buf, ascii_buffer + *ppos, len))
		return -EFAULT;

	*ppos += len;
	return len;
}


static ssize_t mtrr_write (struct file *file, const char *buf,
		size_t len, loff_t * ppos)
/*  Format of control line:
    "base=%Lx size=%Lx type=%s"     OR:
    "disable=%d"
*/
{
	int i, err, reg;
	u64 base;
	u32 size;
	char *ptr;
	char line[LINE_SIZE];

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;
	memset (line, 0, LINE_SIZE);

	if (len > LINE_SIZE)
		len = LINE_SIZE;

	if (copy_from_user (line, buf, len - 1))
		return -EFAULT;
	ptr = line + strlen (line) - 1;

	if (*ptr == '\n')
		*ptr = '\0';

	if (!strncmp (line, "disable=", 8)) {
		reg = simple_strtoul (line + 8, &ptr, 0);
		err = mtrr_del_page (reg, 0, 0);
		if (err < 0)
			return err;
		return len;
	}

	if (strncmp (line, "base=", 5)) {
		printk (KERN_INFO "mtrr: no \"base=\" in line: \"%s\"\n", line);
		return -EINVAL;
	}

	base = simple_strtoull (line + 5, &ptr, 0);

	for (; isspace (*ptr); ++ptr) ;

	if (strncmp (ptr, "size=", 5)) {
		printk (KERN_INFO "mtrr: no \"size=\" in line: \"%s\"\n", line);
		return -EINVAL;
	}

	size = simple_strtoull (ptr + 5, &ptr, 0);

	if ((base & 0xfff) || (size & 0xfff)) {
		printk (KERN_INFO "mtrr: size and base must be multiples of 4 kiB\n");
		printk (KERN_INFO "mtrr: size: 0x%x  base: 0x%Lx\n", size, base);
		return -EINVAL;
	}

	for (; isspace (*ptr); ++ptr) ;

	if (strncmp (ptr, "type=", 5)) {
		printk (KERN_INFO "mtrr: no \"type=\" in line: \"%s\"\n", line);
		return -EINVAL;
	}
	ptr += 5;

	for (; isspace (*ptr); ++ptr) ;

	for (i = 0; i < MTRR_NUM_TYPES; ++i) {
		if (strcmp (ptr, mtrr_strings[i]))
			continue;
		base >>= PAGE_SHIFT;
		size >>= PAGE_SHIFT;
		err = mtrr_add_page ((u64) base, size, i, 1);
		if (err < 0)
			return err;
		return len;
	}
	printk (KERN_INFO "mtrr: illegal type: \"%s\"\n", ptr);
	return -EINVAL;
}


static int mtrr_ioctl (struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int err;
	mtrr_type type;
	struct mtrr_sentry sentry;
	struct mtrr_gentry gentry;

	switch (cmd) {
	default:
		return -ENOIOCTLCMD;

	case MTRRIOC_ADD_ENTRY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (copy_from_user (&sentry, (void *) arg, sizeof sentry))
			return -EFAULT;
		err = mtrr_file_add (sentry.base, sentry.size, sentry.type,
				   file, 0);
		if (err < 0)
			return err;
		break;

	case MTRRIOC_SET_ENTRY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (copy_from_user (&sentry, (void *) arg, sizeof sentry))
			return -EFAULT;
		err = mtrr_add (sentry.base, sentry.size, sentry.type, 0);
		if (err < 0)
			return err;
		break;

	case MTRRIOC_DEL_ENTRY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (copy_from_user (&sentry, (void *) arg, sizeof sentry))
			return -EFAULT;
		err = mtrr_file_del (sentry.base, sentry.size, file, 0);
		if (err < 0)
			return err;
		break;

	case MTRRIOC_KILL_ENTRY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (copy_from_user (&sentry, (void *) arg, sizeof sentry))
			return -EFAULT;
		err = mtrr_del (-1, sentry.base, sentry.size);
		if (err < 0)
			return err;
		break;

	case MTRRIOC_GET_ENTRY:
		if (copy_from_user (&gentry, (void *) arg, sizeof gentry))
			return -EFAULT;
		if (gentry.regnum >= get_num_var_ranges ())
			return -EINVAL;
		get_mtrr (gentry.regnum, (u64*) &gentry.base, &gentry.size, &type);

		/* Hide entries that go above 4GB */
		if (gentry.base + gentry.size > 0x100000
		    || gentry.size == 0x100000)
			gentry.base = gentry.size = gentry.type = 0;
		else {
			gentry.base <<= PAGE_SHIFT;
			gentry.size <<= PAGE_SHIFT;
			gentry.type = type;
		}

		if (copy_to_user ((void *) arg, &gentry, sizeof gentry))
			return -EFAULT;
		break;

	case MTRRIOC_ADD_PAGE_ENTRY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (copy_from_user (&sentry, (void *) arg, sizeof sentry))
			return -EFAULT;
		err = mtrr_file_add (sentry.base, sentry.size, sentry.type, file, 1);
		if (err < 0)
			return err;
		break;

	case MTRRIOC_SET_PAGE_ENTRY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (copy_from_user (&sentry, (void *) arg, sizeof sentry))
			return -EFAULT;
		err = mtrr_add_page (sentry.base, sentry.size, sentry.type, 0);
		if (err < 0)
			return err;
		break;

	case MTRRIOC_DEL_PAGE_ENTRY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (copy_from_user (&sentry, (void *) arg, sizeof sentry))
			return -EFAULT;
		err = mtrr_file_del (sentry.base, sentry.size, file, 1);
		if (err < 0)
			return err;
		break;

	case MTRRIOC_KILL_PAGE_ENTRY:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (copy_from_user (&sentry, (void *) arg, sizeof sentry))
			return -EFAULT;
		err = mtrr_del_page (-1, sentry.base, sentry.size);
		if (err < 0)
			return err;
		break;

	case MTRRIOC_GET_PAGE_ENTRY:
		if (copy_from_user (&gentry, (void *) arg, sizeof gentry))
			return -EFAULT;
		if (gentry.regnum >= get_num_var_ranges ())
			return -EINVAL;
		get_mtrr (gentry.regnum, (u64*) &gentry.base, &gentry.size, &type);
		gentry.type = type;

		if (copy_to_user ((void *) arg, &gentry, sizeof gentry))
			return -EFAULT;
		break;
	}
	return 0;
}


static int mtrr_close (struct inode *ino, struct file *file)
{
	int i, max;
	unsigned int *fcount = file->private_data;

	if (fcount == NULL)
		return 0;

	lock_kernel ();
	max = get_num_var_ranges ();
	for (i = 0; i < max; ++i) {
		while (fcount[i] > 0) {
			if (mtrr_del (i, 0, 0) < 0)
				printk ("mtrr: reg %d not used\n", i);
			--fcount[i];
		}
	}
	unlock_kernel ();
	kfree (fcount);
	file->private_data = NULL;
	return 0;
}


static struct file_operations mtrr_fops = {
	owner:	THIS_MODULE,
	read:	mtrr_read,
	write:	mtrr_write,
	ioctl:	mtrr_ioctl,
	release:mtrr_close,
};

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *proc_root_mtrr;
#endif

static devfs_handle_t devfs_handle;

static void compute_ascii (void)
{
	char factor;
	int i, max;
	mtrr_type type;
	u64 base;
	u32 size;

	ascii_buf_bytes = 0;
	max = get_num_var_ranges ();
	for (i = 0; i < max; i++) {
		get_mtrr (i, &base, &size, &type);
		if (size == 0)
			usage_table[i] = 0;
		else {
			if (size < (0x100000 >> PAGE_SHIFT)) {
				/* less than 1MB */
				factor = 'K';
				size <<= PAGE_SHIFT - 10;
			} else {
				factor = 'M';
				size >>= 20 - PAGE_SHIFT;
			}
			sprintf (ascii_buffer + ascii_buf_bytes,
				"reg%02i: base=0x%05Lx000 (%4iMB), size=%4i%cB: %s, count=%d\n",
				i, base, (u32) base >> (20 - PAGE_SHIFT), size, factor,
				attrib_to_str (type), usage_table[i]);
			ascii_buf_bytes += strlen (ascii_buffer + ascii_buf_bytes);
		}
	}
	devfs_set_file_size (devfs_handle, ascii_buf_bytes);
#ifdef CONFIG_PROC_FS
	if (proc_root_mtrr)
		proc_root_mtrr->size = ascii_buf_bytes;
#endif
}

#endif	/*  USERSPACE_INTERFACE  */

EXPORT_SYMBOL (mtrr_add);
EXPORT_SYMBOL (mtrr_del);


static void __init mtrr_setup (void)
{
	printk ("mtrr: v%s)\n", MTRR_VERSION);

	if (test_bit (X86_FEATURE_MTRR, boot_cpu_data.x86_capability)) {
		/* Query the width (in bits) of the physical
		   addressable memory on the Hammer family. */
		if ((cpuid_eax (0x80000000) >= 0x80000008)) {
			u32 phys_addr;
			phys_addr = cpuid_eax (0x80000008) & 0xff;
			size_or_mask = ~((1L << phys_addr) - 1);
			/*
			 * top bits MBZ as its beyond the addressable range.
			 * bottom bits MBZ as we don't care about lower 12 bits of addr.
			 */
			size_and_mask = (~size_or_mask) & 0x000ffffffffff000L;
		}
	}
}

#ifdef CONFIG_SMP

static volatile u32 smp_changes_mask __initdata = 0;
static struct mtrr_state smp_mtrr_state __initdata = { 0, 0 };

void __init mtrr_init_boot_cpu (void)
{
	mtrr_setup();
	get_mtrr_state (&smp_mtrr_state);
}


void __init mtrr_init_secondary_cpu (void)
{
	u64 mask;
	int count;
	struct set_mtrr_context ctxt;

	/* Note that this is not ideal, since the cache is only flushed/disabled
	   for this CPU while the MTRRs are changed, but changing this requires
	   more invasive changes to the way the kernel boots  */
	set_mtrr_prepare (&ctxt);
	set_mtrr_disable (&ctxt);
	mask = set_mtrr_state (&smp_mtrr_state, &ctxt);
	set_mtrr_done (&ctxt);

	/*  Use the atomic bitops to update the global mask  */
	for (count = 0; count < sizeof mask * 8; ++count) {
		if (mask & 0x01)
			set_bit (count, &smp_changes_mask);
		mask >>= 1;
	}
}

#endif	/*  CONFIG_SMP  */


int __init mtrr_init (void)
{
#ifdef CONFIG_SMP
	/* mtrr_setup() should already have been called from mtrr_init_boot_cpu() */

	finalize_mtrr_state (&smp_mtrr_state);
	mtrr_state_warn (smp_changes_mask);
#else
	mtrr_setup();
#endif

#ifdef CONFIG_PROC_FS
	proc_root_mtrr = create_proc_entry ("mtrr", S_IWUSR | S_IRUGO, &proc_root);
	if (proc_root_mtrr) {
		proc_root_mtrr->owner = THIS_MODULE;
		proc_root_mtrr->proc_fops = &mtrr_fops;
	}
#endif
#ifdef CONFIG_DEVFS_FS
	devfs_handle = devfs_register (NULL, "cpu/mtrr", DEVFS_FL_DEFAULT, 0, 0,
				S_IFREG | S_IRUGO | S_IWUSR,
				&mtrr_fops, NULL);
#endif
	init_table ();
	return 0;
}
