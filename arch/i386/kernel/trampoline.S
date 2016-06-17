/*
 *
 *	Trampoline.S	Derived from Setup.S by Linus Torvalds
 *
 *	4 Jan 1997 Michael Chastain: changed to gnu as.
 *
 *	Entry: CS:IP point to the start of our code, we are 
 *	in real mode with no stack, but the rest of the 
 *	trampoline page to make our stack and everything else
 *	is a mystery.
 *
 *	In fact we don't actually need a stack so we don't
 *	set one up.
 *
 *	We jump into the boot/compressed/head.S code. So you'd
 *	better be running a compressed kernel image or you
 *	won't get very far.
 *
 *	On entry to trampoline_data, the processor is in real mode
 *	with 16-bit addressing and 16-bit data.  CS has some value
 *	and IP is zero.  Thus, data addresses need to be absolute
 *	(no relocation) and are taken with regard to r_base.
 *
 *	If you work on this file, check the object module with objdump
 *	--full-contents --reloc to make sure there are no relocation
 *	entries except for the gdt one..
 */

#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/page.h>

.data

.code16

ENTRY(trampoline_data)
r_base = .
	wbinvd			# Needed for NUMA-Q should be harmless for others
	mov	%cs, %ax	# Code and data in the same place
	mov	%ax, %ds

	mov	$1, %bx		# Flag an SMP trampoline
	cli			# We should be safe anyway

	movl	$0xA5A5A5A5, trampoline_data - r_base
				# write marker for master knows we're running

	lidt	idt_48 - r_base	# load idt with 0, 0
	lgdt	gdt_48 - r_base	# load gdt with whatever is appropriate

	xor	%ax, %ax
	inc	%ax		# protected mode (PE) bit
	lmsw	%ax		# into protected mode
	jmp	flush_instr
flush_instr:
	ljmpl	$__KERNEL_CS, $0x00100000
			# jump to startup_32 in arch/i386/kernel/head.S

idt_48:
	.word	0			# idt limit = 0
	.word	0, 0			# idt base = 0L

gdt_48:
	.word	0x0800			# gdt limit = 2048, 256 GDT entries
	.long	gdt_table-__PAGE_OFFSET	# gdt base = gdt (first SMP CPU)

.globl SYMBOL_NAME(trampoline_end)
SYMBOL_NAME_LABEL(trampoline_end)
