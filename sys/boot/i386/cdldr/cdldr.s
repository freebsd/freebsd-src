#
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

# $FreeBSD: src/sys/boot/i386/cdldr/cdldr.s,v 1.1 2000/01/27 21:21:00 jhb Exp $

#
# This simple program is a preloader for the normal boot3 loader.  It is simply
# prepended to the beginning of a fully built and btxld'd loader.  It then
# copies the loader to the address boot2 normally loads it, emulates the
# boot[12] environment (protected mode, a bootinfo struct, etc.), and then jumps
# to the start of btxldr to start the boot process.  This method allows a stock
# /boot/loader to be used w/o having to fully rewrite boot[12] to handle the
# cd9660 file system.
#

#
# Memory locations.
#
		.set MEM_LOADER_ADDRESS,0x100000 # where the loader lives
		.set MEM_LDR_ENTRY,0x7c00	# our entry point
		.set MEM_ARG,0x900		# Arguments at start
		.set MEM_ARG_BTX,0xa100		# Where we move them to so the
						#  BTX client can see them
		.set MEM_ARG_SIZE,0x18		# Size of the arguments
		.set MEM_BTX_ADDRESS,0x9000	# where BTX lives
		.set MEM_BTX_ENTRY,0x9010	# where BTX starts to execute
		.set MEM_AOUT_HEADER,0x1000	# size of the a.out header
		.set MEM_BTX_OFFSET,0x1000	# offset of BTX in the loader
		.set MEM_BTX_IMAGE,MEM_LOADER_ADDRESS+MEM_BTX_OFFSET # where
						#  BTX is in the loader
		.set MEM_BTX_CLIENT,0xa000	# where BTX clients live
#
# Flags for kargs->bootflags
#
		.set KARGS_FLAGS_CD,0x1		# flag to indicate booting from
						#  CD loader
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
# We expect to be loaded by the BIOS at 0x7c00 (standard boot loader entry point)
#
		.code16
		.globl start
		.org 0x0, 0x0
#
# BTX program loader for CD booting
#
start:		jmp begin			# skip the boot info table
		.org 0x8, 0x90			# fill with nops up to the table
#
# Boot information table that is filled in by mkisofs(8), see the man page for
# details
#
boot_info_table:
bi_pvd_LBA:	.long 0x0
bi_file_LBA:	.long 0x0
bi_file_length:	.long 0x0
bi_checksum:	.long 0x0
bi_reserved:	.byte 0x0
		.org 0x40, 0x0
#
# Actual start of execution
#
begin:		cld				# string ops inc
		xorw %ax, %ax			# zero %ax
		movw %ax, %ss			# setup the
		movw $MEM_LDR_ENTRY, %sp	#  stack
		pushw %dx			# save the BIOS boot device in
						#  %dl for later
		movw $(MEM_LDR_ENTRY/0x10), %ax # setup the
		movw %ax, %ds			#  data segment
		movl $welcome_msg, %si		# %ds:(%si) -> welcome message
		call putstr			# display the welcome message
#
# Setup the arguments that the loader is expecting from boot[12]
#
		movl $bootinfo_msg, %si		# %ds:(%si) -> boot args message
		call putstr			# display the message
		pushw %ss			# Copy %ss
		popw %es			#  to %es
		movl $MEM_ARG, %ebx		# %es:(%ebx) -> boot args
		movw %bx, %di			# %es:(%di) -> boot args
		xorl %eax, %eax			# zero %eax
		movw $(MEM_ARG_SIZE/4), %cx	# Size of arguments in 32-bit
						#  dwords
		rep				# Clear the arguments
		stosl				#  to zero
		popw %dx			# restore BIOS boot device
		movb %dl, %es:0x4(%ebx)		# set kargs->bootdev
		orb $KARGS_FLAGS_CD, %es:0x8(%ebx) # kargs->bootflags |= KARGS_FLAGS_CD
#
# Turn on the A20 address line
#
		call seta20			# Turn A20 on		
#
# Relocate the loader and BTX using a very lazy protected mode
#
		movw $relocate_msg, %si		# Display the
		call putstr			#  relocation message
		movl $MEM_LOADER_ADDRESS, %edi	# %edi is the destination
		movl $(MEM_LDR_ENTRY+end-start+MEM_AOUT_HEADER), %esi # %esi is 
						#  the start of the raw loader
		movl bi_file_length, %ecx	# Set %ecx to the length
		subl $(end-start+MEM_AOUT_HEADER), %ecx	# of the raw loader
		lgdt gdtdesc			# setup our own gdt
		cli				# turn off interrupts
		movl %cr0, %eax			# Turn on
		orl $0x1, %eax			#  protected
		movl %eax, %cr0			#  mode
		.byte 0xea			# long jump to
		.word MEM_LDR_ENTRY+pm_start	#   clear the instruction
		.word SEL_SCODE			#   pre-fetch
		.code32
pm_start:	movw $SEL_SDATA, %ax		# Initialize
		movw %ax, %ds			#  %ds and
		movw %ax, %es			#  %es to a flat selector
		rep				# Relocate
		movsb				#  the loader
		movl $MEM_BTX_IMAGE, %esi	# %esi -> BTX in the loader
		movl $MEM_BTX_ADDRESS, %edi	# %edi -> where BTX needs to go
		movzwl 0xa(%esi), %ecx		# %ecx -> length of BTX
		rep				# Relocate
		movsb				#  BTX
		ljmp $SEL_SCODE16,$(MEM_LDR_ENTRY+pm_16) # Jump to 16-bit PM
		.code16
pm_16:		movw $SEL_RDATA, %ax		# Initialize
		movw %ax, %ds			#  %ds and
		movw %ax, %es			#  %es to a real mode selector
		movl %cr0, %eax			# Turn off
		andl $~0x1, %eax		#  protected
		movl %eax, %cr0			#  mode
		.byte 0xea			# Long jump to
		.word pm_end			#   clear the instruction
		.word MEM_LDR_ENTRY/0x10	#   pre-fetch
pm_end:		sti				# Turn interrupts back on now
#
# Copy the BTX client to MEM_BTX_CLIENT
#
		movw $(MEM_LDR_ENTRY/0x10), %ax # Initialize
		movw %ax, %ds			#   %ds to local data segment
		xorw %ax, %ax			# zero %ax and initialize
		movw %ax, %es			#  %es to segment 0
		movw $MEM_BTX_CLIENT, %di	# Prepare to relocate
		movw $btx_client, %si		#  the simple btx client
		movw $(btx_client_end-btx_client), %cx # length of btx client
		rep				# Relocate the
		movsb				#  simple BTX client
#
# Copy the boot[12] args to where the BTX client can see them
#
		movw $MEM_ARG, %si		# where the args are at now
		movw %ax, %ds			# need segment 0 in %ds
		movw $MEM_ARG_BTX, %di		# where the args are moving to
		movw $(MEM_ARG_SIZE/4), %cx	# size of the arguments in longs
		rep				# Relocate
		movsl				#  the words
#
# Now we just start up BTX and let it do the rest
#
		movw $(MEM_LDR_ENTRY/0x10), %ax	# Initialize
		movw %ax, %ds			#  %ds to the local data segment
		movl $jump_message, %si		# Display the
		call putstr			#  jump message
		.byte 0xea			# Jump to
		.word MEM_BTX_ENTRY 		# BTX entry
		.word 0x0			#  point

#
# Display a null-terminated string
#
putstr:		lodsb				# load %al from %ds:(%si)
		testb %al,%al			# stop at null
		jnz putc			# if the char != null, output it
		ret				# return when null is hit
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
		ret				# To caller

#
# BTX client to start btxld
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
		pushl $(MEM_LOADER_ADDRESS)	# Address to jump to
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
		.word 0xffff,0x0,0x9200,0x0	# SEL_RDATA
		.word 0xffff,0x0,0x9a00,0xcf	# SEL_SCODE (32-bit)
		.word 0xffff,0x0,0x9a00,0x8f	# SEL_SCODE16 (16-bit)		
gdt.1:
#
# Pseudo-descriptors.
#
gdtdesc:	.word gdt.1-gdt-1		# Limit
		.long gdt+MEM_LDR_ENTRY		# Base
		
welcome_msg:	.asciz	"CD Loader 1.00\r\n\n"
bootinfo_msg:	.asciz	"Building the boot loader arguments\r\n"
relocate_msg:	.asciz	"Relocating the loader and the BTX\r\n"
jump_message:	.asciz	"Starting the BTX loader\r\n"
	
end:
