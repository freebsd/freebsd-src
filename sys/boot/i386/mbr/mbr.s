#
# Copyright (c) 1999 Robert Nordier
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

#	$Id: mbr.s,v 1.1.1.1 1999/05/24 22:36:24 rnordier Exp $

# Master boot record

		.set LOAD,0x7c00		# Load address
		.set EXEC,0x600 		# Execution address
		.set PT_OFF,0x1be		# Partition table
		.set MAGIC,0xaa55		# Magic: bootable

		.set NDRIVE,0x8 		# Drives to support

		.globl start			# Entry point

start:		cld				# String ops inc
		xorl %eax,%eax			# Zero
		movl %eax,%es			# Address
		movl %eax,%ds			#  data
		cli				# Disable interrupts
		movl %eax,%ss			# Set up
		movwir(LOAD,_sp)		#  stack
		sti				# Enable interrupts
		movwir(main-EXEC+LOAD,_si)	# Source
		movwir(main,_di)		# Destination
		movwir(0x200-(main-start),_cx)	# Byte count
		rep				# Relocate
		movsb				#  code
		jmpnwi(main-LOAD+EXEC)		# To relocated code

main:		xorl %esi,%esi			# No active partition
		movwir(partbl,_bx)		# Partition table
		movb $0x4,%cl			# Number of entries
main.1: 	cmpbr0(_ch,_bx_)		# Null entry?
		je main.2			# Yes
		jg err_pt			# If 0x1..0x7f
		testl %esi,%esi 		# Active already found?
		jnz err_pt			# Yes
		movl %ebx,%esi			# Point to active
main.2: 	addb $0x10,%bl			# Till
		loop main.1			#  done
		testl %esi,%esi 		# Active found?
		jnz main.3			# Yes
		int $0x18			# BIOS: Diskless boot

main.3: 	cmpb $0x80,%dl			# Drive valid?
		jb main.4			# No
		cmpb $0x80+NDRIVE,%dl		# Within range?
		jb main.5			# Yes
main.4: 	movb0r(_si_,_dl)		# Load drive
main.5: 	movb1r(0x1,_si_,_dh)		# Load head
		movw1r(0x2,_si_,_cx)		# Load cylinder:sector
		movwir(LOAD,_bx)		# Transfer buffer
		movwir(0x201,_ax)		# BIOS: Read from
		int $0x13			#  disk
		jc err_rd			# If error
		cmpwi2(MAGIC,0x1fe,_bx_)	# Bootable?
		jne err_os			# No
		jmp *%ebx			# Invoke bootstrap

err_pt: 	movwir(msg_pt,_si)		# "Invalid partition
		jmp putstr			#  table"

err_rd: 	movwir(msg_rd,_si)		# "Error loading
		jmp putstr			#  operating system"

err_os: 	movwir(msg_os,_si)		# "Missing operating
		jmp putstr			#  system"

putstr.0:	movwir(0x7,_bx) 		# Page:attribute
		movb $0xe,%ah			# BIOS: Display
		int $0x10			#  character
putstr: 	lodsb				# Get character
		testb %al,%al			# End of string?
		jnz putstr.0			# No
putstr.1:	jmp putstr.1			# Await reset

msg_pt: 	.asciz "Invalid partition table"
msg_rd: 	.asciz "Error loading operating system"
msg_os: 	.asciz "Missing operating system"

		.org PT_OFF

partbl: 	.fill 0x10,0x4,0x0		# Partition table
		.word MAGIC			# Magic number
