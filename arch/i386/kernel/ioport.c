/*
 *	linux/arch/i386/kernel/ioport.c
 *
 * This contains the io-permission bitmap code - written by obz, with changes
 * by Linus.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>

/* Set EXTENT bits starting at BASE in BITMAP to value TURN_ON. */
static void set_bitmap(unsigned long *bitmap, short base, short extent, int new_value)
{
	int mask;
	unsigned long *bitmap_base = bitmap + (base >> 5);
	unsigned short low_index = base & 0x1f;
	int length = low_index + extent;

	if (low_index != 0) {
		mask = (~0 << low_index);
		if (length < 32)
				mask &= ~(~0 << length);
		if (new_value)
			*bitmap_base++ |= mask;
		else
			*bitmap_base++ &= ~mask;
		length -= 32;
	}

	mask = (new_value ? ~0 : 0);
	while (length >= 32) {
		*bitmap_base++ = mask;
		length -= 32;
	}

	if (length > 0) {
		mask = ~(~0 << length);
		if (new_value)
			*bitmap_base++ |= mask;
		else
			*bitmap_base++ &= ~mask;
	}
}

/*
 * this changes the io permissions bitmap in the current task.
 */
asmlinkage int sys_ioperm(unsigned long from, unsigned long num, int turn_on)
{
	struct thread_struct * t = &current->thread;
	struct tss_struct * tss = init_tss + smp_processor_id();

	if ((from + num <= from) || (from + num > IO_BITMAP_SIZE*32))
		return -EINVAL;
	if (turn_on && !capable(CAP_SYS_RAWIO))
		return -EPERM;
	/*
	 * If it's the first ioperm() call in this thread's lifetime, set the
	 * IO bitmap up. ioperm() is much less timing critical than clone(),
	 * this is why we delay this operation until now:
	 */
	if (!t->ioperm) {
		/*
		 * just in case ...
		 */
		memset(t->io_bitmap,0xff,(IO_BITMAP_SIZE+1)*4);
		t->ioperm = 1;
	}

	/*
	 * do it in the per-thread copy and in the TSS ...
	 */
	set_bitmap(t->io_bitmap, from, num, !turn_on);
	if (tss->bitmap == IO_BITMAP_OFFSET) { /* already active? */
		set_bitmap(tss->io_bitmap, from, num, !turn_on);
	} else {
		memcpy(tss->io_bitmap, t->io_bitmap, IO_BITMAP_BYTES);
		tss->bitmap = IO_BITMAP_OFFSET; /* Activate it in the TSS */
	}

	return 0;
}

/*
 * sys_iopl has to be used when you want to access the IO ports
 * beyond the 0x3ff range: to get the full 65536 ports bitmapped
 * you'd need 8kB of bitmaps/process, which is a bit excessive.
 *
 * Here we just change the eflags value on the stack: we allow
 * only the super-user to do it. This depends on the stack-layout
 * on system-call entry - see also fork() and the signal handling
 * code.
 */

asmlinkage int sys_iopl(unsigned long unused)
{
	struct pt_regs * regs = (struct pt_regs *) &unused;
	unsigned int level = regs->ebx;
	unsigned int old = (regs->eflags >> 12) & 3;

	if (level > 3)
		return -EINVAL;
	/* Trying to gain more privileges? */
	if (level > old) {
		if (!capable(CAP_SYS_RAWIO))
			return -EPERM;
	}
	regs->eflags = (regs->eflags & 0xffffcfff) | (level << 12);
	return 0;
}
