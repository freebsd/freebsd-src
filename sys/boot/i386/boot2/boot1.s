#
# Copyright (c) 1998 Robert Nordier
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

# $FreeBSD$

		.set MEM_REL,0x700		# Relocation address
		.set MEM_ARG,0x900		# Arguments
		.set MEM_ORG,0x7c00		# Origin
		.set MEM_BUF,0x8c00		# Load area
		.set MEM_BTX,0x9000		# BTX start
		.set MEM_JMP,0x9010		# BTX entry point
		.set MEM_USR,0xa000		# Client start

		.set PRT_OFF,0x1be		# Partition offset
		.set PRT_NUM,0x4		# Partitions
		.set PRT_BSD,0xa5		# Partition type

		.set SIZ_PAG,0x1000		# Page size
		.set SIZ_SEC,0x200		# Sector size

		.globl start
		.globl xread
		.code16

start:		jmp main			# Start recognizably

		.org 0x4,0x90

# External read from disk

xread:		push %ss			# Address
		pop %ds				#  data
xread.1:					# Starting
		pushl $0x0			#  absolute
		push %cx			#  block
		push %ax			#  number
		push %es			# Address of
		push %bx			#  transfer buffer
		xor %ax,%ax			# Number of
		movb %dh,%al			#  blocks to
		push %ax			#  transfer
		push $0x10			# Size of packet
		mov %sp,%bp			# Packet pointer
		callw read			# Read from disk
		lea 0x10(%bp),%sp		# Clear stack
		lret				# To far caller

# Bootstrap

main:		cld				# String ops inc
		xor %cx,%cx			# Zero
		mov %cx,%es			# Address
		mov %cx,%ds			#  data
		mov %cx,%ss			# Set up
		mov $start,%sp			#  stack
		mov %sp,%si			# Source
		mov $MEM_REL,%di		# Destination
		incb %ch			# Word count
		rep				# Copy
		movsw				#  code
		mov $part4,%si			# Partition
		cmpb $0x80,%dl			# Hard drive?
		jb main.4			# No
		movb $0x1,%dh			# Block count
		callw nread			# Read MBR
		mov $0x1,%cx	 		# Two passes
main.1: 	mov $MEM_BUF+PRT_OFF,%si	# Partition table
		movb $0x1,%dh			# Partition
main.2: 	cmpb $PRT_BSD,0x4(%si)		# Our partition type?
		jne main.3			# No
		jcxz main.5			# If second pass
		testb $0x80,(%si)		# Active?
		jnz main.5			# Yes
main.3: 	add $0x10,%si	 		# Next entry
		incb %dh			# Partition
		cmpb $0x1+PRT_NUM,%dh		# In table?
		jb main.2			# Yes
		dec %cx				# Do two
		jcxz main.1			#  passes
		mov $msg_part,%si		# Message
		jmp error			# Error
main.4: 	xor %dx,%dx			# Partition:drive
main.5: 	mov %dx,MEM_ARG			# Save args
		movb $0x10,%dh			# Sector count
		callw nread			# Read disk
		mov $MEM_BTX,%bx		# BTX
		mov 0xa(%bx),%si		# Point past
		add %bx,%si			#  it
		mov $MEM_USR+SIZ_PAG,%di	# Client page 1
		mov $MEM_BTX+0xe*SIZ_SEC,%cx	# Byte
		sub %si,%cx			#  count
		rep				# Relocate
		movsb				#  client
		sub %di,%cx			# Byte count
		xorb %al,%al			# Zero
		rep				#  assumed
		stosb				#  bss
		callw seta20			# Enable A20
		jmp start+MEM_JMP-MEM_ORG	# Start BTX

# Enable A20

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

# Local read from disk

nread:		mov $MEM_BUF,%bx		# Transfer buffer
		mov 0x8(%si),%ax		# Get
		mov 0xa(%si),%cx		#  LBA
		push %cs			# Read from
		callw xread.1	 		#  disk
		jnc return			# If success
		mov $msg_read,%si		# Message

# Error exit

error:		callw putstr			# Display message
		mov $prompt,%si			# Display
		callw putstr			#  prompt
		xorb %ah,%ah			# BIOS: Get
		int $0x16			#  keypress
		int $0x19			# BIOS: Reboot

# Display string

putstr.0:	mov $0x7,%bx	 		# Page:attribute
		movb $0xe,%ah			# BIOS: Display
		int $0x10			#  character
putstr: 	lodsb				# Get char
		testb %al,%al			# End of string?
		jne putstr.0			# No

ereturn:	movb $0x1,%ah			# Invalid
		stc				#  argument
return: 	retw				# To caller

# Read from disk

read:		testb $0x80,%cs:MEM_REL+flags-start # LBA support enabled?
		jz read.1			# No
		mov $0x55aa,%bx			# Magic
		push %dx			# Save
		movb $0x41,%ah			# BIOS: Check
		int $0x13			#  extensions present
		pop %dx				# Restore
		jc read.1			# If error
		cmp $0xaa55,%bx			# Magic?
		jne read.1			# No
		testb $0x1,%cl			# Packet interface?
		jz read.1			# No
		mov %bp,%si			# Disk packet
		movb $0x42,%ah			# BIOS: Extended
		int $0x13			#  read
		retw				# To caller

read.1: 	push %dx			# Save
		movb $0x8,%ah			# BIOS: Get drive
		int $0x13			#  parameters
		movb %dh,%ch			# Max head number
		pop %dx				# Restore
		jc return			# If error
		andb $0x3f,%cl			# Sectors per track
		jz ereturn			# If zero
		cli				# Disable interrupts
		mov 0x8(%bp),%eax		# Get LBA
		push %dx			# Save
		movzbl %cl,%ebx			# Divide by
		xor %edx,%edx			#  sectors
		div %ebx			#  per track
		movb %ch,%bl			# Max head number
		movb %dl,%ch			# Sector number
		inc %bx				# Divide by
		xorb %dl,%dl			#  number
		div %ebx			#  of heads
		movb %dl,%bh			# Head number
		pop %dx				# Restore
		cmpl $0x3ff,%eax		# Cylinder number supportable?
		sti				# Enable interrupts
		ja ereturn			# No
		xchgb %al,%ah			# Set up cylinder
		rorb $0x2,%al			#  number
		orb %ch,%al			# Merge
		inc %ax				#  sector
		xchg %ax,%cx	 		#  number
		movb %bh,%dh			# Head number
		subb %ah,%al			# Sectors this track
		mov 0x2(%bp),%ah		# Blocks to read
		cmpb %ah,%al			# To read
		jb read.2			#  this
		movb %ah,%al			#  track
read.2: 	mov $0x5,%di	 		# Try count
read.3: 	les 0x4(%bp),%bx		# Transfer buffer
		push %ax			# Save
		movb $0x2,%ah			# BIOS: Read
		int $0x13			#  from disk
		pop %bx				# Restore
		jnc read.4			# If success
		dec %di				# Retry?
		jz read.6			# No
		xorb %ah,%ah			# BIOS: Reset
		int $0x13			#  disk system
		xchg %bx,%ax	 		# Block count
		jmp read.3			# Continue
read.4: 	movzbw %bl,%ax	 		# Sectors read
		add %ax,0x8(%bp)		# Adjust
		jnc read.5			#  LBA,
		incw 0xa(%bp)	 		#  transfer
read.5: 	shlb %bl			#  buffer
		add %bl,0x5(%bp)		#  pointer,
		sub %al,0x2(%bp)		#  block count
		ja read.1			# If not done
read.6: 	retw				# To caller

# Messages

msg_read:	.asciz "Read"
msg_part:	.asciz "Boot"

prompt: 	.asciz " error\r\n"

flags:		.byte FLAGS			# Flags

		.org PRT_OFF,0x90

# Partition table

		.fill 0x30,0x1,0x0
part4:		.byte 0x80, 0x00, 0x01, 0x00
		.byte 0xa5, 0xff, 0xff, 0xff
		.byte 0x00, 0x00, 0x00, 0x00
		.byte 0x50, 0xc3, 0x00, 0x00

		.word 0xaa55			# Magic number
