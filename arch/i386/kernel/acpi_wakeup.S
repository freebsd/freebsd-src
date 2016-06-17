
.text
#include <linux/linkage.h>
#include <asm/segment.h>


ALIGN
wakeup_start:
wakeup_code:
	wakeup_code_start = .
	.code16

	cli
	cld

	# setup data segment
	movw	%cs, %ax

	addw	$(wakeup_data - wakeup_code) >> 4, %ax
	movw	%ax, %ds
	movw	%ax, %ss

	# Private stack is needed for ASUS board
	mov	$(wakeup_stack - wakeup_data), %sp

	# set up page table
	movl	(real_save_cr3 - wakeup_data), %eax
	movl	%eax, %cr3

	# make sure %cr4 is set correctly (features, etc)
	movl	(real_save_cr4 - wakeup_data), %eax
	movl	%eax, %cr4

	# need a gdt
	lgdt	real_save_gdt - wakeup_data

	# Flush the prefetch queue
	jmp 1f
1:

	movl	%cr0, %eax
	orl     $0x80000001, %eax
	movl	%eax, %cr0

	ljmpl	$__KERNEL_CS,$SYMBOL_NAME(wakeup_pmode_return)

	.code32
	ALIGN

.org	0x100
wakeup_data:
		.word 0
real_save_gdt:	.word 0
		.long 0
real_save_cr3:	.long 0
real_save_cr4:	.long 0

.org	0x300
wakeup_stack:
wakeup_end:

wakeup_pmode_return:
	# restore data segment
	movl	$__KERNEL_DS, %eax
	movw	%ax, %ds
	movw	%ax, %es

	# and restore the stack
	movw	%ax, %ss
	movl	saved_esp, %esp

	# restore other segment registers
	xorl	%eax, %eax
	movw	%ax, %fs
	movw	%ax, %gs

	# reload the gdt, as we need the full 32 bit address
	lgdt	saved_gdt
	lidt	saved_idt
	lldt	saved_ldt

	# restore the other general registers
	movl	saved_ebx, %ebx
	movl	saved_edi, %edi
	movl	saved_esi, %esi
	movl	saved_ebp, %ebp

	# jump to place where we left off
	movl	saved_eip,%eax
	jmp	*%eax

##
# acpi_copy_wakeup_routine
#
# Copy the above routine to low memory.
#
# Parameters:
# %eax:	place to copy wakeup routine to
#
# Returned address is location of code in low memory (past data and stack)
#
ENTRY(acpi_copy_wakeup_routine)

	pushl	%esi
	pushl	%edi

	sgdt	saved_gdt
	sidt	saved_idt
	sldt	saved_ldt
	str	saved_tss

	movl	%eax, %edi
	leal	wakeup_start, %esi
	movl	$(wakeup_end - wakeup_start) >> 2, %ecx

	rep ;  movsl

	movl    %cr3, %edx
	movl    %edx, real_save_cr3 - wakeup_start (%eax)
	movl    %cr4, %edx
	movl    %edx, real_save_cr4 - wakeup_start (%eax)
	sgdt    real_save_gdt - wakeup_start (%eax)

	# restore the regs we used
	popl	%edi
	popl	%esi
	ret


.data
ALIGN
# saved registers
saved_gdt:	.long	0,0
saved_idt:	.long	0,0
saved_ldt:	.long	0
saved_tss:	.long	0
saved_cr0:	.long	0

ENTRY(saved_ebp)	.long	0
ENTRY(saved_esi)	.long	0
ENTRY(saved_edi)	.long	0
ENTRY(saved_ebx)	.long	0

ENTRY(saved_eip)	.long	0
ENTRY(saved_esp)	.long	0
