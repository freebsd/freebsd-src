/*
 * Copyright 2013-2014 Andrew Turner.
 * Copyright 2013-2014 Ian Lepore.
 * Copyright 2013-2014 Rui Paulo.
 * Copyright 2013 Eitan Adler.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <machine/machdep.h>
#include <machine/stack.h>

#include "linker_if.h"

/*
 * Definitions for the instruction interpreter.
 *
 * The ARM EABI specifies how to perform the frame unwinding in the
 * Exception Handling ABI for the ARM Architecture document. To perform
 * the unwind we need to know the initial frame pointer, stack pointer,
 * link register and program counter. We then find the entry within the
 * index table that points to the function the program counter is within.
 * This gives us either a list of three instructions to process, a 31-bit
 * relative offset to a table of instructions, or a value telling us
 * we can't unwind any further.
 *
 * When we have the instructions to process we need to decode them
 * following table 4 in section 9.3. This describes a collection of bit
 * patterns to encode that steps to take to update the stack pointer and
 * link register to the correct values at the start of the function.
 */

/* A special case when we are unable to unwind past this function */
#define	EXIDX_CANTUNWIND	1

/*
 * Entry types.
 * These are the only entry types that have been seen in the kernel.
 */
#define	ENTRY_MASK	0xff000000
#define	ENTRY_ARM_SU16	0x80000000
#define	ENTRY_ARM_LU16	0x81000000

/* Instruction masks. */
#define	INSN_VSP_MASK		0xc0
#define	INSN_VSP_SIZE_MASK	0x3f
#define	INSN_STD_MASK		0xf0
#define	INSN_STD_DATA_MASK	0x0f
#define	INSN_POP_TYPE_MASK	0x08
#define	INSN_POP_COUNT_MASK	0x07
#define	INSN_VSP_LARGE_INC_MASK	0xff

/* Instruction definitions */
#define	INSN_VSP_INC		0x00
#define	INSN_VSP_DEC		0x40
#define	INSN_POP_MASKED		0x80
#define	INSN_VSP_REG		0x90
#define	INSN_POP_COUNT		0xa0
#define	INSN_FINISH		0xb0
#define	INSN_POP_REGS		0xb1
#define	INSN_VSP_LARGE_INC	0xb2

/* An item in the exception index table */
struct unwind_idx {
	uint32_t offset;
	uint32_t insn;
};

/*
 * Local cache of unwind info for loaded modules.
 *
 * To unwind the stack through the code in a loaded module, we need to access
 * the module's exidx unwind data.  To locate that data, one must search the
 * elf section headers for the SHT_ARM_EXIDX section.  Those headers are
 * available at the time the module is being loaded, but are discarded by time
 * the load process has completed.  Code in kern/link_elf.c locates the data we
 * need and stores it into the linker_file structure before calling the arm
 * machdep routine for handling loaded modules (in arm/elf_machdep.c).  That
 * function calls into this code to pass along the unwind info, which we save
 * into one of these module_info structures.
 *
 * Because we have to help stack(9) gather stack info at any time, including in
 * contexts where sleeping is not allowed, we cannot use linker_file_foreach()
 * to walk the kernel's list of linker_file structs, because doing so requires
 * acquiring an exclusive sx_lock.  So instead, we keep a local list of these
 * structures, one for each loaded module (and one for the kernel itself that we
 * synthesize at init time).  New entries are added to the end of this list as
 * needed, but entries are never deleted from the list.  Instead, they are
 * cleared out in-place to mark them as unused.  That means the code doing stack
 * unwinding can always safely walk the list without locking, because the
 * structure of the list never changes in a way that would cause the walker to
 * follow a bad link.
 *
 * A cleared-out entry on the list has module start=UINTPTR_MAX and end=0, so
 * start <= addr < end cannot be true for any value of addr being searched for.
 * We also don't have to worry about races where we look up the unwind info just
 * before a module is unloaded and try to access it concurrently with or just
 * after the unloading happens in another thread, because that means the path of
 * execution leads through a now-unloaded module, and that's already well into
 * undefined-behavior territory.
 *
 * List entries marked as unused get reused when new modules are loaded.  We
 * don't worry about holding a few unused bytes of memory in the list after
 * unloading a module.
 */
struct module_info {
	uintptr_t	module_start;   /* Start of loaded module */
	uintptr_t	module_end;     /* End of loaded module */
	uintptr_t	exidx_start;    /* Start of unwind data */
	uintptr_t	exidx_end;      /* End of unwind data */
	STAILQ_ENTRY(module_info)
			link;           /* Link to next entry */
};
static STAILQ_HEAD(, module_info) module_list;

/*
 * Hide ugly casting in somewhat-less-ugly macros.
 *  CADDR - cast a pointer or number to caddr_t.
 *  UADDR - cast a pointer or number to uintptr_t.
 */
#define	CADDR(addr)	((caddr_t)(void*)(uintptr_t)(addr))
#define	UADDR(addr)	((uintptr_t)(addr))

/*
 * Clear the info in an existing module_info entry on the list.  The
 * module_start/end addresses are set to values that cannot match any real
 * memory address.  The entry remains on the list, but will be ignored until it
 * is populated with new data.
 */
static void
clear_module_info(struct module_info *info)
{
	info->module_start = UINTPTR_MAX;
	info->module_end   = 0;
}

/*
 * Populate an existing module_info entry (which is already on the list) with
 * the info for a new module.
 */
static void
populate_module_info(struct module_info *info, linker_file_t lf)
{

	/*
	 * Careful!  The module_start and module_end fields must not be set
	 * until all other data in the structure is valid.
	 */
	info->exidx_start  = UADDR(lf->exidx_addr);
	info->exidx_end    = UADDR(lf->exidx_addr) + lf->exidx_size;
	info->module_start = UADDR(lf->address);
	info->module_end   = UADDR(lf->address) + lf->size;
}

/*
 * Create a new empty module_info entry and add it to the tail of the list.
 */
static struct module_info *
create_module_info(void)
{
	struct module_info *info;

	info = malloc(sizeof(*info), M_CACHE, M_WAITOK | M_ZERO);
	clear_module_info(info);
	STAILQ_INSERT_TAIL(&module_list, info, link);
	return (info);
}

/*
 * Search for a module_info entry on the list whose address range contains the
 * given address.  If the search address is zero (no module will be loaded at
 * zero), then we're looking for an empty item to reuse, which is indicated by
 * module_start being set to UINTPTR_MAX in the entry.
 */
static struct module_info *
find_module_info(uintptr_t addr)
{
	struct module_info *info;

	STAILQ_FOREACH(info, &module_list, link) {
		if ((addr >= info->module_start && addr < info->module_end) ||
		    (addr == 0 && info->module_start == UINTPTR_MAX))
			return (info);
	}
	return (NULL);
}

/*
 * Handle the loading of a new module by populating a module_info for it.  This
 * is called for both preloaded and dynamically loaded modules.
 */
void
unwind_module_loaded(struct linker_file *lf)
{
	struct module_info *info;

	/*
	 * A module that contains only data may have no unwind info; don't
	 * create any module info for it.
	 */
	if (lf->exidx_size == 0)
		return;

	/*
	 * Find an unused entry in the existing list to reuse.  If we don't find
	 * one, create a new one and link it into the list.  This is the only
	 * place the module_list is modified.  Adding a new entry to the list
	 * will not perturb any other threads currently walking the list.  This
	 * function is invoked while kern_linker is still holding its lock
	 * to prevent its module list from being modified, so we don't have to
	 * worry about racing other threads doing an insert concurrently.
	 */
	if ((info = find_module_info(0)) == NULL) {
		info = create_module_info();
	}
	populate_module_info(info, lf);
}

/* Handle the unloading of a module. */
void
unwind_module_unloaded(struct linker_file *lf)
{
	struct module_info *info;

	/*
	 * A module that contains only data may have no unwind info and there
	 * won't be a list entry for it.
	 */
	if (lf->exidx_size == 0)
		return;

	/*
	 * When a module is unloaded, we clear the info out of its entry in the
	 * module list, making that entry available for later reuse.
	 */
	if ((info = find_module_info(UADDR(lf->address))) == NULL) {
		printf("arm unwind: module '%s' not on list at unload time\n",
		    lf->filename);
		return;
	}
	clear_module_info(info);
}

/*
 * Initialization must run fairly early, as soon as malloc(9) is available, and
 * definitely before witness, which uses stack(9).  We synthesize a module_info
 * entry for the kernel, because unwind_module_loaded() doesn't get called for
 * it.  Also, it is unlike other modules in that the elf metadata for locating
 * the unwind tables might be stripped, so instead we have to use the
 * _exidx_start/end symbols created by ldscript.arm.
 */
static int
module_info_init(void *arg __unused)
{
	struct linker_file thekernel;

	STAILQ_INIT(&module_list);

	thekernel.filename   = "kernel";
	thekernel.address    = CADDR(&_start);
	thekernel.size       = UADDR(&_end) - UADDR(&_start);
	thekernel.exidx_addr = CADDR(&_exidx_start);
	thekernel.exidx_size = UADDR(&_exidx_end) - UADDR(&_exidx_start);
	populate_module_info(create_module_info(), &thekernel);

	return (0);
}
SYSINIT(unwind_init, SI_SUB_KMEM, SI_ORDER_ANY, module_info_init, NULL);

/* Expand a 31-bit signed value to a 32-bit signed value */
static __inline int32_t
expand_prel31(uint32_t prel31)
{

	return ((int32_t)(prel31 & 0x7fffffffu) << 1) / 2;
}

/*
 * Perform a binary search of the index table to find the function
 * with the largest address that doesn't exceed addr.
 */
static struct unwind_idx *
find_index(uint32_t addr)
{
	struct module_info *info;
	unsigned int min, mid, max;
	struct unwind_idx *start;
	struct unwind_idx *item;
	int32_t prel31_addr;
	uint32_t func_addr;

	info = find_module_info(addr);
	if (info == NULL)
		return NULL;

	min = 0;
	max = (info->exidx_end - info->exidx_start) / sizeof(struct unwind_idx);
	start = (struct unwind_idx *)CADDR(info->exidx_start);

	while (min != max) {
		mid = min + (max - min + 1) / 2;

		item = &start[mid];

		prel31_addr = expand_prel31(item->offset);
		func_addr = (uint32_t)&item->offset + prel31_addr;

		if (func_addr <= addr) {
			min = mid;
		} else {
			max = mid - 1;
		}
	}

	return &start[min];
}

/* Reads the next byte from the instruction list */
static uint8_t
unwind_exec_read_byte(struct unwind_state *state)
{
	uint8_t insn;

	/* Read the unwind instruction */
	insn = (*state->insn) >> (state->byte * 8);

	/* Update the location of the next instruction */
	if (state->byte == 0) {
		state->byte = 3;
		state->insn++;
		state->entries--;
	} else
		state->byte--;

	return insn;
}

/* Executes the next instruction on the list */
static int
unwind_exec_insn(struct unwind_state *state)
{
	struct thread *td = curthread;
	unsigned int insn;
	uint32_t *vsp = (uint32_t *)state->registers[SP];
	int update_vsp = 0;

	/* This should never happen */
	if (state->entries == 0)
		return 1;

	/* Read the next instruction */
	insn = unwind_exec_read_byte(state);

	if ((insn & INSN_VSP_MASK) == INSN_VSP_INC) {
		state->registers[SP] += ((insn & INSN_VSP_SIZE_MASK) << 2) + 4;

	} else if ((insn & INSN_VSP_MASK) == INSN_VSP_DEC) {
		state->registers[SP] -= ((insn & INSN_VSP_SIZE_MASK) << 2) + 4;

	} else if ((insn & INSN_STD_MASK) == INSN_POP_MASKED) {
		unsigned int mask, reg;

		/* Load the mask */
		mask = unwind_exec_read_byte(state);
		mask |= (insn & INSN_STD_DATA_MASK) << 8;

		/* We have a refuse to unwind instruction */
		if (mask == 0)
			return 1;

		/* Update SP */
		update_vsp = 1;

		/* Load the registers */
		for (reg = 4; mask && reg < 16; mask >>= 1, reg++) {
			if (mask & 1) {
				if (!kstack_contains(td, (uintptr_t)vsp,
				    sizeof(*vsp)))
					return 1;

				state->registers[reg] = *vsp++;
				state->update_mask |= 1 << reg;

				/* If we have updated SP kep its value */
				if (reg == SP)
					update_vsp = 0;
			}
		}

	} else if ((insn & INSN_STD_MASK) == INSN_VSP_REG &&
	    ((insn & INSN_STD_DATA_MASK) != 13) &&
	    ((insn & INSN_STD_DATA_MASK) != 15)) {
		/* sp = register */
		state->registers[SP] =
		    state->registers[insn & INSN_STD_DATA_MASK];

	} else if ((insn & INSN_STD_MASK) == INSN_POP_COUNT) {
		unsigned int count, reg;

		/* Read how many registers to load */
		count = insn & INSN_POP_COUNT_MASK;

		/* Update sp */
		update_vsp = 1;

		/* Pop the registers */
		if (!kstack_contains(td, (uintptr_t)vsp,
		    sizeof(*vsp) * (4 + count)))
			return 1;
		for (reg = 4; reg <= 4 + count; reg++) {
			state->registers[reg] = *vsp++;
			state->update_mask |= 1 << reg;
		}

		/* Check if we are in the pop r14 version */
		if ((insn & INSN_POP_TYPE_MASK) != 0) {
			if (!kstack_contains(td, (uintptr_t)vsp, sizeof(*vsp)))
				return 1;
			state->registers[14] = *vsp++;
		}

	} else if (insn == INSN_FINISH) {
		/* Stop processing */
		state->entries = 0;

	} else if (insn == INSN_POP_REGS) {
		unsigned int mask, reg;

		mask = unwind_exec_read_byte(state);
		if (mask == 0 || (mask & 0xf0) != 0)
			return 1;

		/* Update SP */
		update_vsp = 1;

		/* Load the registers */
		for (reg = 0; mask && reg < 4; mask >>= 1, reg++) {
			if (mask & 1) {
				if (!kstack_contains(td, (uintptr_t)vsp,
				    sizeof(*vsp)))
					return 1;
				state->registers[reg] = *vsp++;
				state->update_mask |= 1 << reg;
			}
		}

	} else if ((insn & INSN_VSP_LARGE_INC_MASK) == INSN_VSP_LARGE_INC) {
		unsigned int uleb128;

		/* Read the increment value */
		uleb128 = unwind_exec_read_byte(state);

		state->registers[SP] += 0x204 + (uleb128 << 2);

	} else {
		/* We hit a new instruction that needs to be implemented */
#if 0
		db_printf("Unhandled instruction %.2x\n", insn);
#endif
		return 1;
	}

	if (update_vsp) {
		state->registers[SP] = (uint32_t)vsp;
	}

#if 0
	db_printf("fp = %08x, sp = %08x, lr = %08x, pc = %08x\n",
	    state->registers[FP], state->registers[SP], state->registers[LR],
	    state->registers[PC]);
#endif

	return 0;
}

/* Performs the unwind of a function */
static int
unwind_tab(struct unwind_state *state)
{
	uint32_t entry;

	/* Set PC to a known value */
	state->registers[PC] = 0;

	/* Read the personality */
	entry = *state->insn & ENTRY_MASK;

	if (entry == ENTRY_ARM_SU16) {
		state->byte = 2;
		state->entries = 1;
	} else if (entry == ENTRY_ARM_LU16) {
		state->byte = 1;
		state->entries = ((*state->insn >> 16) & 0xFF) + 1;
	} else {
#if 0
		db_printf("Unknown entry: %x\n", entry);
#endif
		return 1;
	}

	while (state->entries > 0) {
		if (unwind_exec_insn(state) != 0)
			return 1;
	}

	/*
	 * The program counter was not updated, load it from the link register.
	 */
	if (state->registers[PC] == 0) {
		state->registers[PC] = state->registers[LR];

		/*
		 * If the program counter changed, flag it in the update mask.
		 */
		if (state->start_pc != state->registers[PC])
			state->update_mask |= 1 << PC;
	}

	return 0;
}

/*
 * Unwind a single stack frame.
 * Return 0 on success or 1 if the stack cannot be unwound any further.
 *
 * XXX The can_lock argument is no longer germane; a sweep of callers should be
 * made to remove it after this new code has proven itself for a while.
 */
int
unwind_stack_one(struct unwind_state *state, int can_lock __unused)
{
	struct unwind_idx *index;

	/* Reset the mask of updated registers */
	state->update_mask = 0;

	/* The pc value is correct and will be overwritten, save it */
	state->start_pc = state->registers[PC];

	/* Find the item to run */
	index = find_index(state->start_pc);
	if (index == NULL || index->insn == EXIDX_CANTUNWIND)
		return 1;

	if (index->insn & (1U << 31)) {
		/* The data is within the instruction */
		state->insn = &index->insn;
	} else {
		/* A prel31 offset to the unwind table */
		state->insn = (uint32_t *)
		    ((uintptr_t)&index->insn +
		     expand_prel31(index->insn));
	}

	/* Run the unwind function, return its finished/not-finished status. */
	return (unwind_tab(state));
}
