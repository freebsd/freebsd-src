#
# Copyright (c) 2000 Jonathan Lemon <jlemon@FreeBSD.org>
# Copyright (c) 2000 John Baldwin
# All rights reserved.
#
# Redistribution and use in source and binary forms are freely
# permitted provided that the above copyright notice and this
# paragraph and the following disclaimer are duplicated in all
# such forms.
#
# This software is provided "AS IS" and without any express or
# implied warranties, including, without limitation, the implied
# warranties of merchantability and fitness for a particular
# purpose.
#
# $FreeBSD: src/sys/boot/i386/liloldr/liloldr.s,v 1.1 2000/07/17 17:06:04 jlemon Exp $

#
# This simple program is a preloader for the normal boot3 loader.  It is
# simply prepended to the beginning of a fully built and btxld'd loader.
# It then copies the loader to the address boot2 normally loads it, emulates
# the boot[12] environment (protected mode, a bootinfo struct, etc.), and
# then jumps to the start of btxldr to start the boot process.  This method
# allows a stock /boot/loader to be booted from LILO.
#

#
# Memory locations.
#
		.set MEM_PAGE_SIZE,0x1000	# memory page size, 4k
		.set MEM_ARG,0x900		# Arguments at start
		.set MEM_ARG_BTX,0xa100		# Where we move them to so the
						#  BTX client can see them
		.set MEM_ARG_SIZE,0x18		# Size of the arguments
		.set MEM_BTX_ADDRESS,0x9000	# where BTX lives
		.set MEM_BTX_ENTRY,0x9010	# where BTX starts to execute
		.set MEM_BTX_OFFSET,MEM_PAGE_SIZE # offset of BTX in the loader
		.set MEM_BTX_CLIENT,0xa000	# where BTX clients live
#
# a.out header fields
#
		.set AOUT_TEXT,0x04		# text segment size
		.set AOUT_DATA,0x08		# data segment size
		.set AOUT_BSS,0x0c		# zero'd BSS size
		.set AOUT_SYMBOLS,0x10		# symbol table
		.set AOUT_ENTRY,0x14		# entry point
		.set AOUT_HEADER,MEM_PAGE_SIZE	# size of the a.out header
#
# Flags for kargs->bootflags
#
		.set KARGS_FLAGS_LILO,0x4	# flag to indicate booting from
						#  LILO loader
#
# Segment selectors.
#
		.set SEL_SDATA,0x8		# Supervisor data
		.set SEL_RDATA,0x10		# Real mode data
		.set SEL_SCODE,0x18		# PM-32 code
		.set SEL_SCODE16,0x20		# PM-16 code
#
# BTX constants
#
		.set INT_SYS,0x30		# BTX syscall interrupt
#
# LILO constants
#
		.set SETUP_OFF,497		# offset of setup table
		.set SETUP_SECTORS,4		# historical
		.set SEG_LOADER,0x1000		# segment for loader
		.set SEG_BSECT,0x9000		# segment for bootsector
		.set SEG_SETUP,0x9020		# segment for setup
		.set LDR_BASE,0x10000		# where LILO puts loader
		.set SETUP_BASE,0x90200		# base address for us
		.set ROOT_DEV,0x901FC
#
# We expect to be loaded by LILO at 0x90200, and the loader at LDR_BASE
# %cs upon entry == SEG_SETUP == 0x9020
#
		.code16
		.globl start
		.org 0x0, 0x0
#
# BTX program loader for LILO booting
#
start:		jmp code_start

		.ascii	"HdrS"			# signature
		.word	0x0201			# version
		.word 	0x00,0x00,0x00		# LOADLIN hacks
		.word	welcome_msg
		.byte	0x00			# loader type (LILO)
		.byte	0x00			# load kernel high (NO)
		.word	0x00			# LOADLIN hacks
		.long	SEG_LOADER		# load address
		.long	0x00			# ramdisk image
		.long	0x00			# ramdisk size
		.word	0x00,0x00		# bzImage hacks
		.word	0x00			# heap end pointer

code_start:	cld				# string ops inc
		movw $SEG_BSECT, %ax		# use bootsector area
		movw %ax, %ss			#  for the
		movw $0x1E0, %sp		#  stack
		movw %cs, %ax			# inherit code segment
		movw %ax, %ds			# setup the
		movw %ax, %es			#  data segments
		movw $welcome_msg, %si		# %ds:(%si) -> welcome message
		callw putstr			# display the welcome message
#
# Turn on the A20 address line
#
		callw seta20			# Turn A20 on		
#
# Dealing with segments is a pain, so move into protected mode immediately.
#
		movw $relocate_msg, %si		# Display the
		callw putstr			#  relocation message
		lgdt gdtdesc			# setup our own gdt
		cli				# turn off interrupts
		movl %cr0, %eax			# Turn on
		orb $0x1, %al			#  protected
		movl %eax, %cr0			#  mode
		ljmp $SEL_SCODE,$pm_start	# long jump to clear the
						#  instruction pre-fetch queue
		.code32
pm_start:	movw $SEL_SDATA, %ax		# Initialize
		movw %ax, %ds			#  %ds and
		movw %ax, %es			#  %es to a flat selector
		movl LDR_BASE+AOUT_ENTRY, %edi	# %edi is the destination
		movl $LDR_BASE+AOUT_HEADER, %esi # %esi is 
						#  the start of the text
						#  segment
		movl LDR_BASE+AOUT_TEXT, %ecx	# %ecx = length of the text
						#  segment
		rep				# Relocate the
		movsb				#  text segment
		addl $(MEM_PAGE_SIZE - 1), %edi	# pad %edi out to a new page
		andl $~(MEM_PAGE_SIZE - 1), %edi #  for the data segment
		movl LDR_BASE+AOUT_DATA, %ecx	# size of the data segment
		rep				# Relocate the
		movsb				#  data segment
		movl LDR_BASE+AOUT_BSS, %ecx	# size of the bss
		xorl %eax, %eax			# zero %eax
		addb $3, %cl			# round %ecx up to
		shrl $2, %ecx			#  a multiple of 4
		rep				# zero the
		stosl				#  bss
		movl LDR_BASE+AOUT_ENTRY, %esi	# %esi -> relocated loader
		addl $MEM_BTX_OFFSET, %esi	# %esi -> BTX in the loader
		movl $MEM_BTX_ADDRESS, %edi	# %edi -> where BTX needs to go
		movzwl 0xa(%esi), %ecx		# %ecx -> length of BTX
		rep				# Relocate
		movsb				#  BTX
#
# Copy the BTX client to MEM_BTX_CLIENT
#
		movl $MEM_BTX_CLIENT, %edi	# Prepare to relocate
		movl $SETUP_BASE+btx_client, %esi #  the simple btx client
		movl $(btx_client_end-btx_client), %ecx # length of btx client
		rep				# Relocate the
		movsb				#  simple BTX client
#
# Setup the boot[12] args for the BTX client
#
		movl $MEM_ARG_BTX, %ebx		# %ebx -> boot args
		movl %ebx, %edi			# Destination
		movl $(MEM_ARG_SIZE/4), %ecx	# Size of the arguments in longs
		xorl %eax, %eax			# zero %eax
		rep				# Clear the arguments
		stosl				#  to zero
		movw $KARGS_FLAGS_LILO, 0x8(%ebx) # set kargs->bootflags
		movw ROOT_DEV, %ax
		movw %ax, 0x04(%ebx)
#
# Save the entry point so the client can get to it later on
#
		movl LDR_BASE+AOUT_ENTRY, %eax	# load the entry point
		stosl				# add it to the end of the
						#  arguments
#
# setup is done, return
#
		ljmp $SEL_SCODE16,$pm_16	# Jump to 16-bit PM
		.code16
pm_16:		movw $SEL_RDATA, %ax		# Initialize
		movw %ax, %ds			#  %ds and
		movw %ax, %es			#  %es to a real mode selector
		movl %cr0, %eax			# Turn off
		andb $~0x1, %al			#  protected
		movl %eax, %cr0			#  mode
		ljmp $SEG_SETUP,$pm_end		# Long jump to clear the
						#  instruction pre-fetch queue
pm_end:		sti				# Turn interrupts back on now
#
# Restore real mode environment
#
		movw %cs, %ax			# inherit code segment
		movw %ax, %ds			# setup the
		movw %ax, %es			#  data segments
#
# Now we just start up BTX and let it do the rest
#
		movw $jump_message, %si		# Display the
		callw putstr			#  jump message
		ljmp $0,$MEM_BTX_ENTRY		# Jump to the BTX entry point
#
# Display a null-terminated string
#
putstr:		lodsb				# load %al from %ds:(%si)
		testb %al,%al			# stop at null
		jnz putc			# if the char != null, output it
		retw				# return when null is hit
putc:		movw $0x7,%bx			# attribute for output
		movb $0xe,%ah			# BIOS: put_char
		int $0x10			# call BIOS, print char in %al
		jmp putstr			# keep looping

#
# Enable A20
#
seta20: 	cli				# Disable interrupts
seta20.1:	inb $0x64,%al			# Get status
		testb $0x2,%al			# Busy?
		jnz seta20.1			# Yes
		movb $0xd1,%al			# Command: Write
		outb %al,$0x64			#  output port
seta20.2:	inb $0x64,%al			# Get status
		testb $0x2,%al			# Busy?
		jnz seta20.2			# Yes
		movb $0xdf,%al			# Enable
		outb %al,$0x60			#  A20
		sti				# Enable interrupts
		retw				# To caller

#
# BTX client to start btxldr
#
		.code32
btx_client:	movl $(MEM_ARG_BTX-MEM_BTX_CLIENT+MEM_ARG_SIZE-4), %esi
						# %ds:(%esi) -> end
						#  of boot[12] args
		movl $(MEM_ARG_SIZE/4), %ecx	# Number of words to push
		std				# Go backwards
push_arg:	lodsl				# Read argument
		pushl %eax			# Push it onto the stack
		loop push_arg			# Push all of the arguments
		cld				# In case anyone depends on this
		pushl MEM_ARG_BTX-MEM_BTX_CLIENT+MEM_ARG_SIZE # Entry point of
						#  the loader
		pushl %eax			# Emulate a near call
		movl $0x1, %eax			# 'exec' system call
		int $INT_SYS			# BTX system call
btx_client_end:
		.code16
	
		.p2align 4
#
# Global descriptor table.
#
gdt:		.word 0x0,0x0,0x0,0x0		# Null entry
		.word 0xffff,0x0,0x9200,0xcf	# SEL_SDATA
		.word 0xffff,0x0200,0x9209,0x0f	# SEL_RDATA
		.word 0xffff,0x0200,0x9a09,0x4f	# SEL_SCODE (32-bit)
		.word 0xffff,0x0200,0x9a09,0x0f	# SEL_SCODE16 (16-bit)		
gdt.1:
#
# Pseudo-descriptors.
#
gdtdesc:	.word gdt.1-gdt-1		# Limit
		.long gdt+SETUP_BASE		# Base
		
welcome_msg:	.asciz	"LILO Loader 1.00\r\n\n"
bootinfo_msg:	.asciz	"Building the boot loader arguments\r\n"
relocate_msg:	.asciz	"Relocating the loader and the BTX\r\n"
jump_message:	.asciz	"Starting the BTX loader\r\n"
#
# pad out to setup sectors.
#
		.org SETUP_SECTORS*512,0x00
end:
