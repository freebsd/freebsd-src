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

#	$Id: boot1.s,v 1.8 1999/01/13 23:30:07 rnordier Exp $

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

start:		jmp main			# Start recognizably

		.org 0x4,0x90

# External read from disk

xread:		pushl %ss			# Address
		popl %ds			#  data
xread.1:	o16				# Starting
		pushb $0x0			#  absolute
		pushl %ecx			#  block
		pushl %eax			#  number
		pushl %es			# Address of
		pushl %ebx			#  transfer buffer
		xorl %eax,%eax			# Number of
		movb %dh,%al			#  blocks to
		pushl %eax			#  transfer
		pushb $0x10			# Size of packet
		movl %esp,%ebp			# Packet pointer
		callwi(read)			# Read from disk
		leaw1r(0x10,_bp_,_sp)		# Clear stack
		lret				# To far caller

# Bootstrap

main:		cld				# String ops inc
		xorl %ecx,%ecx			# Zero
		movl %cx,%es			# Address
		movl %cx,%ds			#  data
		movl %cx,%ss			# Set up
		movwir(start,_sp)		#  stack
		movl %esp,%esi			# Source
		movwir(MEM_REL,_di)		# Destination
		incb %ch			# Word count
		rep				# Copy
		movsl				#  code
		movwir(part4,_si)		# Partition
		cmpb $0x80,%dl			# Hard drive?
		jb main.4			# No
		movb $0x1,%dh			# Block count
		callwi(nread)			# Read MBR
		movwir(0x1,_cx) 		# Two passes
main.1: 	movwir(MEM_BUF+PRT_OFF,_si)	# Partition table
		movb $0x1,%dh			# Partition
main.2: 	cmpbi1(PRT_BSD,0x4,_si_)	# Our partition type?
		jne main.3			# No
		jecxz main.5			# If second pass
		tstbi0(0x80,_si_)		# Active?
		jnz main.5			# Yes
main.3: 	addl $0x10,%esi 		# Next entry
		incb %dh			# Partition
		cmpb $0x1+PRT_NUM,%dh		# In table?
		jb main.2			# Yes
		decl %ecx			# Do two
		jecxz main.1			#  passes
		movwir(msg_part,_si)		# Message
		jmp error			# Error
main.4: 	xorl %edx,%edx			# Partition:drive
main.5: 	movwrm(_dx,MEM_ARG)		# Save args
		movb $0x10,%dh			# Sector count
		callwi(nread)			# Read disk
		movwir(MEM_BTX,_bx)		# BTX
		movw1r(0xa,_bx_,_si)		# Point past
		addl %ebx,%esi			#  it
		movwir(MEM_USR+SIZ_PAG,_di)	# Client page 1
		movwir(MEM_BTX+0xe*SIZ_SEC,_cx) # Byte
		subl %esi,%ecx			#  count
		rep				# Relocate
		movsb				#  client
		subl %edi,%ecx			# Byte count
		xorb %al,%al			# Zero
		rep				#  assumed
		stosb				#  bss
		callwi(seta20)			# Enable A20
		jmpnwi(start+MEM_JMP-MEM_ORG)	# Start BTX

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
		ret				# To caller

# Local read from disk

nread:		movwir(MEM_BUF,_bx)		# Transfer buffer
		movw1r(0x8,_si_,_ax)		# Get
		movw1r(0xa,_si_,_cx)		#  LBA
		pushl %cs			# Read from
		callwi(xread.1) 		#  disk
		jnc return			# If success
		movwir(msg_read,_si)		# Message

# Error exit

error:		callwi(putstr)			# Display message
		movwir(prompt,_si)		# Display
		callwi(putstr)			#  prompt
		xorb %ah,%ah			# BIOS: Get
		int $0x16			#  keypress
		int $0x19			# BIOS: Reboot

# Display string

putstr.0:	movwir(0x7,_bx) 		# Page:attribute
		movb $0xe,%ah			# BIOS: Display
		int $0x10			#  character
putstr: 	lodsb				# Get char
		testb %al,%al			# End of string?
		jne putstr.0			# No

ereturn:	movb $0x1,%ah			# Invalid
		stc				#  argument
return: 	ret				# To caller

# Read from disk

read:		cs_				# LBA support
		tstbim(0x80,MEM_REL+flags-start)#  enabled?
		jz read.1			# No
		movwir(0x55aa,_bx)		# Magic
		pushl %edx			# Save
		movb $0x41,%ah			# BIOS: Check
		int $0x13			#  extensions present
		popl %edx			# Restore
		jc read.1			# If error
		cmpwir(0xaa55,_bx)		# Magic?
		jne read.1			# No
		testb $0x1,%cl			# Packet interface?
		jz read.1			# No
		movl %ebp,%esi			# Disk packet
		movb $0x42,%ah			# BIOS: Extended
		int $0x13			#  read
		ret				# To caller

read.1: 	pushl %edx			# Save
		movb $0x8,%ah			# BIOS: Get drive
		int $0x13			#  parameters
		movb %dh,%ch			# Max head number
		popl %edx			# Restore
		jc return			# If error
		andb $0x3f,%cl			# Sectors per track
		jz ereturn			# If zero
		cli				# Disable interrupts
		o16				# Get
		movw1r(0x8,_bp_,_ax)		#  LBA
		pushl %edx			# Save
		movzbw %cl,%bx			# Divide by
		xorw %dx,%dx			#  sectors
		divw %bx,%ax			#  per track
		movb %ch,%bl			# Max head number
		movb %dl,%ch			# Sector number
		incl %ebx			# Divide by
		xorb %dl,%dl			#  number
		divw %bx,%ax			#  of heads
		movb %dl,%bh			# Head number
		popl %edx			# Restore
		o16				# Cylinder number
		cmpl $0x3ff,%eax		#  supportable?
		sti				# Enable interrupts
		ja ereturn			# No
		xchgb %al,%ah			# Set up cylinder
		rorb $0x2,%al			#  number
		orb %ch,%al			# Merge
		incl %eax			#  sector
		xchgl %eax,%ecx 		#  number
		movb %bh,%dh			# Head number
		subb %ah,%al			# Sectors this track
		movb1r(0x2,_bp_,_ah)		# Blocks to read
		cmpb %ah,%al			# To read
		jb read.2			#  this
		movb %ah,%al			#  track
read.2: 	movwir(0x5,_di) 		# Try count
read.3: 	lesw1r(0x4,_bp_,_bx)		# Transfer buffer
		pushl %eax			# Save
		movb $0x2,%ah			# BIOS: Read
		int $0x13			#  from disk
		popl %ebx			# Restore
		jnc read.4			# If success
		decl %edi			# Retry?
		jz read.6			# No
		xorb %ah,%ah			# BIOS: Reset
		int $0x13			#  disk system
		xchgl %ebx,%eax 		# Block count
		jmp read.3			# Continue
read.4: 	movzbl %bl,%eax 		# Sectors read
		addwr1(_ax,0x8,_bp_)		# Adjust
		jnc read.5			#  LBA,
		incw1(0xa,_bp_) 		#  transfer
read.5: 	shlb %bl			#  buffer
		addbr1(_bl,0x5,_bp_)		#  pointer,
		subbr1(_al,0x2,_bp_)		#  block count
		ja read.1			# If not done
read.6: 	ret				# To caller

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
