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

#	$Id: boot1.s,v 1.3 1998/10/27 20:19:24 rnordier Exp $

		.set MEM_REL,0x600		# Relocation address
		.set MEM_ARG,0x800		# Arguments
		.set MEM_PKT,0x810		# Disk packet
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

xread:		pushl %cs			# Address
		popl %ds			#  data
xread.1:	movwir(MEM_PKT,_si)		# Packet
		movbr1(_al,0x2,_si_)		# Blocks to read
		o16				# Transfer
		movwr1(_bx,0x4,_si_)		#  buffer
		o16				# LBA
		movwr1(_cx,0x8,_si_)		#  address
		callwi(read)			# Read from disk
		lret				# To caller

main:		cld				# String ops inc
		xorl %eax,%eax			# Zero
		movl %ax,%es			# Address
		movl %ax,%ds			#  data
		movl %ax,%ss			# Set up
		movwir(start,_sp)		#  stack
		movl %esp,%esi			# Source
		movwir(MEM_REL,_di)		# Destination
		movwir(0x100,_cx)		# Word count
		rep				# Copy
		movsl				#  code
		movb $0x10,%cl			# Words to clear
		rep				# Zero
		stosl				#  them
		movbi1(0x10,-0x10,_di_) 	# Set packet size
		cmpb $0x80,%dl			# Hard drive?
		jb main.4			# No
		movwir(part4,_si)		# Read master
		movb $0x1,%al			#  boot
		callwi(nread)			#  record
		xorl %eax,%eax			# Pass number
main.1: 	movwir(MEM_BUF+PRT_OFF,_si)	# Partition table
		movb $0x1,%dh			# Partition
main.2: 	cmpbi1(PRT_BSD,0x4,_si_)	# Our partition type?
		jne main.3			# No
		tstbi0(0x80,_si_)		# Active?
		jnz main.5			# Yes
		testb %al,%al			# Second pass?
		jnz main.5			# Yes
main.3: 	addl $0x10,%esi 		# Next entry
		incb %dh			# Partition
		cmpb $0x1+PRT_NUM,%dh		# Done?
		jb main.2			# No
		incl %eax			# Pass
		cmpb $0x2,%al			# Done?
		jb main.1			# No
		movwir(msg_part,_si)		# Message
		jmp error			# Error
main.4: 	xorl %edx,%edx			# Partition:drive
		movwir(part4,_si)		# Partition pointer
main.5: 	movwrm(_dx,MEM_ARG)		# Save args
		movb $0x10,%al			# Sector count
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

# Read from disk

nread:		xorw %bx,%bx			# Transfer
		movb $MEM_BUF>>0x8,%bh		#  buffer
		o16				# LBA
		movw1r(0x8,_si_,_cx)		#  address
		pushl %cs			# Read from
		callwi(xread.1) 		#  disk
		jnc return			# If success
		movwir(msg_read,_si)		# Message

# Error exit

error:		callwi(putstr)			# Display message
		movwir(msg_boot,_si)		# Display
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

return: 	ret				# Generic return

# Read from disk

read:		tstbim(0x80,MEM_REL+flags-start)# Extensions enabled?
		jz read.3			# No
		testb %dh,%dh			# Try for extensions?
		jz read.3			# No
		movwir(0x55aa,_bx)		# Magic
		pushl %edx			# Save
		movb $0x41,%ah			# BIOS: Check
		int $0x13			#  extensions present
		popl %edx			# Restore
		jc read.3			# If error
		cmpwir(0xaa55,_bx)		# Magic?
		jne read.3			# No
		testb $0x1,%cl			# Packet interface?
		jz read.3			# No
		movb $0x42,%ah			# BIOS: Extended
		int $0x13			#  read
		ret				# To caller

read.1: 	movb $0x1,%ah			# Invalid
		stc				#  parameter
read.2: 	ret				# To caller

read.3: 	pushl %edx			# Save
		movb $0x8,%ah			# BIOS: Get drive
		int $0x13			#  parameters
		movb %dh,%ch			# Max head number
		popl %edx			# Restore
		jc read.2			# If error
		andb $0x3f,%cl			# Sectors per track
		jz read.1			# If zero
		o16				# Get
		movw1r(0x8,_si_,_ax)		#  LBA
		pushl %edx			# Save
		movzbw %cl,%bx			# Divide by
		xorw %dx,%dx			#  sectors
		divw %bx,%ax			#  per track
		movb %ch,%bl			# Max head number
		movb %dl,%ch			# Sector number
		incl %ebx			# Divide by
		xorb %dl,%dl			#  number of
		divw %bx,%ax			#  heads
		movb %dl,%bh			# Head number
		popl %edx			# Restore
		o16				# Cylinder number
		cmpl $0x3ff,%eax		#  supportable?
		ja read.1			# No
		xchgb %al,%ah			# Set up cylinder
		rorb $0x2,%al			#  number
		orb %ch,%al			# Merge
		incl %eax			#  sector
		xchgl %eax,%ecx 		#  number
		movb %bh,%dh			# Head number
		subb %ah,%al			# Sectors this track
		movb1r(0x2,_si_,_ah)		# Blocks to read
		cmpb %ah,%al			# To read
		jb read.4			#  this
		movb %ah,%al			#  track
read.4: 	movwir(0x5,_bp) 		# Try count
read.5: 	lesw1r(0x4,_si_,_bx)		# Transfer buffer
		pushl %eax			# Save
		movb $0x2,%ah			# BIOS: Conventional
		int $0x13			#  read
		popl %ebx			# Restore
		jnc read.6			# If success
		decl %ebp			# Retry?
		jz read.7			# No
		xorb %ah,%ah			# BIOS: Reset
		int $0x13			#  disk system
		movl %ebx,%eax			# Block count
		jmp read.5			# Continue
read.6: 	movzbw %bl,%ax			# Sectors read
		o16				# Adjust
		addwr1(_ax,0x8,_si_)		#  LBA,
		shlb %bl			#  buffer
		addbr1(_bl,0x5,_si_)		#  pointer,
		subbr1(_al,0x2,_si_)		#  block count
		ja read.3			# If not done
read.7: 	ret				# To caller

# Messages

msg_read:	.asciz "Read error"
msg_part:	.asciz "No bootable partition"
msg_boot:	.asciz "\r\nHit return to reboot: "

flags:		.byte FLAGS			# Flags

		.org PRT_OFF,0x90

# Partition table

		.fill 0x30,0x1,0x0
part4:		.byte 0x80, 0x00, 0x01, 0x00
		.byte 0xa5, 0xff, 0xff, 0xff
		.byte 0x00, 0x00, 0x00, 0x00
		.byte 0x50, 0xc3, 0x00, 0x00

		.word 0xaa55			# Magic number
