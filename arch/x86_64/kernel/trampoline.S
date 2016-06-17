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
 *	On entry to trampoline_data, the processor is in real mode
 *	with 16-bit addressing and 16-bit data.  CS has some value
 *	and IP is zero.  Thus, data addresses need to be absolute
 *	(no relocation) and are taken with regard to r_base.
 *
 *	If you work on this file, check the object module with objdump
 *	--full-contents --reloc to make sure there are no relocation
 *	entries. For the GDT entry we do hand relocation in smpboot.c
 *	because of 64bit linker limitations.
 */

#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/page.h>

.data

.code16

ENTRY(trampoline_data)
r_base = .

	mov	%cs, %ax	# Code and data in the same place
	mov	%ax, %ds

	mov	$1, %bx		# Flag an SMP trampoline
	cli			# We should be safe anyway

	movl	$0xA5A5A5A5, trampoline_data - r_base
				# write marker for master knows we're running

	lidt	idt_48 - r_base	# load idt with 0, 0
	lgdt	gdt_48 - r_base	# load gdt with whatever is appropriate

	movw    $__KERNEL_DS,%ax
	movw    %ax,%ds
	movw    %ax,%es
	
	xor	%ax, %ax
	inc	%ax		# protected mode (PE) bit
	lmsw	%ax		# into protected mode
	jmp	flush_instr
flush_instr:
	ljmpl	$__KERNEL32_CS, $0x00100000
			# jump to startup_32 in arch/x86_64/kernel/head.S

idt_48:
	.word	0			# idt limit = 0
	.word	0, 0			# idt base = 0L

gdt_48:
	.short	0x0800			# gdt limit = 2048, 256 GDT entries
	.globl tramp_gdt_ptr
tramp_gdt_ptr:
	.long	0			# gdt base = gdt (first SMP CPU)
					# this is filled in by C because the 64bit
					# linker doesn't support absolute 32bit
					# relocations.

	
.globl trampoline_end
trampoline_end:	
