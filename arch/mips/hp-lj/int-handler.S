#include <asm/asm.h>

#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	.text
	.set    mips1
	.set    reorder
	.set    macro
	.set    noat
	.align	5

# MIPS has 16 exception vectors numbered 0 to 15
# vector number 0 is for interrupts and the others are for various exceptions
# The following code is installed as the handler for exception 0
# There are 8 possible interrupts that can cause this exception.
# The cause register indicates which are pending
# The status register indicates which are enabled
# This code segment basically will decipher which interrup occurred (7 downto 0)
# and pass an integer indicating which was the highest priority pending interrupt
# to the do_IRQ routine.

NESTED(hpIRQ, PT_SIZE, sp)
	SAVE_ALL
	CLI				# Important: mark KERNEL mode !
	/*
	 * Get pending interrupts
	 */

	mfc0	t0,CP0_CAUSE		# get pending interrupts
	mfc0	t1,CP0_STATUS		# get enabled interrupts
	and	t0,t1			# isolate allowed ones
	andi	t0,0xff00		# isolate pending bits
	sll	t0,16			# shift the pending bits down
	beqz	t0,3f			# no pending intrs, then spurious
	nop				# delay slot

	/*
	 * Find irq with highest priority
	 * FIXME: This is slow - use binary search
	 */

	la	a0,7
1:	bltz	t0,2f			# found pending irq
	subu	a0,1
	sll	t0,1
	b	1b
	nop				# delay slot


call_do_IRQ:
2:	move	a1,sp
	jal	do_IRQ
	nop				# delay slot
	j       ret_from_irq
	nop

/*
	mfc0	t0,CP0_STATUS		# disable interrupts
	ori	t0,1
	xori	t0,1
	mtc0	t0,CP0_STATUS

	la      a1, ret_from_irq
	jr	a1
*/
3:	j	spurious_interrupt
END(hpIRQ)

