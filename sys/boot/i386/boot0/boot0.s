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

#	$Id: boot0.s,v 1.2 1998/10/09 17:19:51 rnordier Exp $

# A 512-byte boot manager.

		.set LOAD,0x7c00		# Load address
		.set ORIGIN,0x600		# Relocation address
		.set PRT_OFF,0x1be		# Partition table
		.set FAKE,0x810			# Partition entry
		.set MNUOPT,0x81c		# Menu options

		.globl start			# Entry point

start:		cld 				# String ops inc
		xorl %eax,%eax			# Zero
		movl %eax,%es			# Address
		movl %eax,%ds			#  data
		movl %eax,%ss			# Set up
		movwir(LOAD,_sp)		#  stack
		movl %esp,%esi			# Source
		movwir(start,_di)		# Destination
		movwir(0x100,_cx)		# Word count
		rep				# Relocate
		movsl				#  code
		movb $0x10,%cl			# Words to clear
		rep				# Zero
		stosl				#  them
		incb1(-0xe,_di_)		# Sector number
		jmpnwi(main-LOAD+ORIGIN)	# To relocated code

main:		movbrm(_dl,FAKE)		# Save drive number
		callwi(putn)			# To new line
		movwir(partbl,_bx)		# Partition table
		xorl %edx,%edx			# Item
main.1:		movbr0(_dh,_bx_)		# Mark inactive
		movb1r(0x4,_bx_,_al)		# Load type
		movwir(table0,_di)		# Exclusion table
		movb $0x3,%cl			# Entries
		repne				# Exclude
		scasb				#  partition?
		je main.3			# Yes
		movb $0xa,%cl			# Entries
		repne				# Known
		scasb				#  type?
		jne main.2			# No
		leaw1r(0xa,_di_,_di)		# Name table
main.2:		movwir(item,_si)		# Display start
		callwi(putkey)			#  of menu item
		movl %edi,%esi			# Set pointer
		lodsb				#  to
		cwde				#  partition
		add %eax,%esi			#  description
		callwi(puts)			# Display it
		btswrm(_dx,MNUOPT)		# Flag option enabled
main.3:		addb $0x10,%bl			# Next entry
		incl %edx			# Next item
		cmpb $0x4,%dl			# Done?
		jb main.1			# No
		movwir(prompt,_si)		# Display
		callwi(putstr)			#  prompt
		movbmr(opt,_dl)			# Display
		decl %esi			#  default
		callwi(putkey)			#  key
		xorb %ah,%ah			# BIOS: Get
		int $0x1a			#  system time
		movl %edx,%edi			# Save
main.4:		movb $0x1,%ah			# BIOS: Check
		int $0x16			#  for keypress
		jnz main.6			# Have one
		xorb %ah,%ah			# BIOS: Get
		int $0x1a			#  system time
		subl %edi,%edx			# Elapsed time
		cmpwmr(ticks,_dx)		# Timeout?
		jb main.4			# No
		jmp main.7			# Join common code
main.5:		movb $0x7,%al			# Signal
		callwi(putchr)			#  error
main.6:		xorb %ah,%ah			# BIOS: Get
		int $0x16			#  keypress
		movb %ah,%al			# Scan code
		cmpb $0x1c,%al			# Enter pressed?
		jne main.8			# No
main.7:		movbma(opt)			# Load
		jmp main.9			#  default
main.8:		subb $0x3b,%al			# Less F1 scan code
		jb main.5			# Not a function key
main.9:		cmpb $0x4,%al			# F1..F5?
		ja main.5			# No
		movwir(FAKE,_si)		# Partition entry
		movb0r(_si_,_dl)		# Load drive number
		jne main.10			# If not F5
		xorb $0x1,%dl			# Toggle drive 
		jmp main.11			#  number
main.10:	cwtl 				# Option
		btwrm(_ax,MNUOPT)		#  enabled?
		jnc main.5			# No
		movbam(opt)			# Save option
		shlb $0x4,%al			# Point to
		addwia(partbl)			#  partition
		xchgl %esi,%eax			#  entry
		movbi0(0x80,_si_)		# Flag active
		pushl %esi			# Save
		movl %eax,%esi			# Fake partition entry
		movwir(start,_bx)		# Data to write
		movwir(0x301,_ax)		# Write sector
		callwi(intx13)			#  to disk
		popl %esi			# Restore
		jc main.5			# If error
main.11:	movwir(LOAD,_bx)		# Address for read
		movwir(0x201,_ax)		# Read sector
		callwi(intx13)			#  from disk
		jc main.5			# If error
		cmpwi2(0xaa55,0x1fe,_bx_)	# Bootable?
		jne main.5			# No
		movwir(crlf,_si)		# Leave some
		callwi(puts)			#  space
		jmp *%ebx			# Invoke bootstrap

# Display routines

putkey:		movb $'F',%al			# Display
		callwi(putchr)			#  'F'
		movb $'1',%al			# Prepare
		addb %dl,%al			#  digit
		jmp putstr.1			# Display the rest

puts:		callwi(putstr)			# Display string

putn:		movwir(crlf,_si)		# To next line

putstr:		lodsb				# Get byte
		testb $0x80,%al			# End of string?
		jnz putstr.2			# Yes
putstr.1:	callwi(putchr)			# Display char
		jmp putstr			# Continue
putstr.2:	andb $~0x80,%al			# Clear MSB

putchr:		pushl %ebx			# Save
		movwir(0x7,_bx)			# Page:attribute
		movb $0xe,%ah			# BIOS: Display
		int $0x10			#  character
		popl %ebx			# Restore
		ret				# To caller

# Disk I/O

intx13:		movb1r(0x1,_si_,_dh)		# Load head
		movw1r(0x2,_si_,_cx)		# Load cylinder:sector
		o16				# Load
		movw1r(0x8,_si_,_di)		#  offset
		pushl %ecx			# Save
		pushl %ebx			#  caller's
		movwir(0x55aa,_bx)		# Magic
		pushl %eax			# Save
		movb $0x41,%ah			# BIOS: EDD extensions
		int $0x13			#  present?
		popl %eax			# Restore
		jc intx13.1			# No
		cmpwir(0xaa55,_bx)		# Magic?
		jne intx13.1			# No
		testb $0x1,%cl			# Use packet?
		jz intx13.1			# No
		orb $0x40,%ah			# Use EDD
intx13.1:	popl %ebx			# Restore
		popl %ecx			#  caller's
		testb $0x40,%ah			# Use EDD?
		jz intx13.2			# No
		movwir(break,_si)		# Packet pointer
		movbi0(0x10,_si_)		# Packet size
		movbr1(_al,0x2,_si_)		# Block count
		movwr1(_bx,0x4,_si_)		# Transfer
		movws1(_es,0x6,_si_)		#  buffer
		o16				# LBA
		movwr1(_di,0x8,_si_)		#  address
		xorb %al,%al			# Verify off
intx13.2:	int $0x13			# BIOS: Disk I/O
		ret				# To caller

# Menu strings

crlf:		.ascii "\r";         .byte '\n'|0x80
item:		.ascii "  ";         .byte ' '|0x80
prompt: 	.ascii "\nDefault:"; .byte ' '|0x80

# Partition type tables

table0: 	.byte 0x0, 0x5, 0xf
table1: 	.byte 0x1, 0x4, 0x6, 0xb, 0xc, 0xe, 0x63, 0x83
		.byte 0xa5, 0xa6

		.byte os_misc-.-1		# Unknown
		.byte os_dos-.-1		# DOS
		.byte os_dos-.-1		# DOS
		.byte os_dos-.-1		# DOS
		.byte os_dos-.-1		# Windows
		.byte os_dos-.-1		# Windows
		.byte os_dos-.-1		# Windows
		.byte os_unix-.-1		# UNIX
		.byte os_linux-.-1		# Linux
		.byte os_freebsd-.-1		# FreeBSD
		.byte os_bsd-.-1		# OpenBSD

os_misc:	.ascii "?";    .byte '?'|0x80
os_dos: 	.ascii "DO";   .byte 'S'|0x80
os_unix:	.ascii "UNI";  .byte 'X'|0x80
os_linux:	.ascii "Linu"; .byte 'x'|0x80
os_freebsd:	.ascii "Free"
os_bsd: 	.ascii "BS";   .byte 'D'|0x80

		.org PRT_OFF-0x3,0x90

opt:		.byte 0x1			# Option
ticks:		.word 0xb6			# Delay

partbl:		.fill 0x40,0x1,0x0		# Partition table
		.word 0xaa55			# Magic number

break:						# Uninitialized data
