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

#	$Id: boot0.s,v 1.9 1999/06/18 21:49:18 rnordier Exp $

# A 512-byte boot manager.

		.set NHRDRV,0x475		# Number of hard drives
		.set ORIGIN,0x600		# Execution address
		.set FAKE,0x800 		# Partition entry
		.set LOAD,0x7c00		# Load address

		.set PRT_OFF,0x1be		# Partition table

		.set TBL0SZ,0x3 		# Table 0 size
		.set TBL1SZ,0xa 		# Table 1 size

		.set MAGIC,0xaa55		# Magic: bootable

		.set KEY_ENTER,0x1c		# Enter key scan code
		.set KEY_F1,0x3b		# F1 key scan code

		.set _NXTDRV,-0x48		# Next drive
		.set _OPT,-0x47 		# Default option
		.set _SETDRV,-0x46		# Drive to force
		.set _FLAGS,-0x45		# Flags
		.set _TICKS,-0x44		# Timeout ticks
		.set _FAKE,0x0			# Fake partition entry
		.set _MNUOPT,0xc		# Menu options

		.globl start			# Entry point

start:		cld				# String ops inc
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
		movl %edi,%ebp			# Address variables
		movb $0x8,%cl			# Words to clear
		rep				# Zero
		stosl				#  them
		incb1(-0xe,_di_)		# Sector number
		jmpnwi(main-LOAD+ORIGIN)	# To relocated code

main:		tstbi1(0x20,_FLAGS,_bp_)	# Set number drive?
		jnz main.1			# Yes
		testb %dl,%dl			# Drive number valid?
		js main.2			# Possibly
main.1: 	movb1r(_SETDRV,_bp_,_dl)	# Drive number to use
main.2: 	movbr1(_dl,_FAKE,_bp_)		# Save drive number
		callwi(putn)			# To new line
		pushl %edx			# Save drive number
		movwir(partbl+0x4,_bx)		# Partition table
		xorl %edx,%edx			# Item number
main.3: 	movbr1(_ch,-0x4,_bx_)		# Zero active flag
		btwr1(_dx,_FLAGS,_bp_)		# Entry enabled?
		jnc main.5			# No
		movb0r(_bx_,_al)		# Load type
		movwir(tables,_di)		# Lookup tables
		movb $TBL0SZ,%cl		# Number of entries
		repne				# Exclude
		scasb				#  partition?
		je main.5			# Yes
		movb $TBL1SZ,%cl		# Number of entries
		repne				# Known
		scasb				#  type?
		jne main.4			# No
		addwir(TBL1SZ,_di)		# Adjust
main.4: 	movb0r(_di_,_cl)		# Partition
		addl %ecx,%edi			#  description
		callwi(putx)			# Display it
main.5: 	incl %edx			# Next item
		addb $0x10,%bl			# Next entry
		jnc main.3			# Till done
		popl %eax			# Drive number
		subb $0x80-0x1,%al		# Does next
		cmpbmr(NHRDRV,_al)		#  drive exist?
		jb main.6			# Yes
		decl %eax			# Already drive 0?
		jz main.7			# Yes
		xorb %al,%al			# Drive 0
main.6: 	addb $'0'|0x80,%al		# Save next
		movbr1(_al,_NXTDRV,_bp_)	#  drive number
		movwir(drive,_di)		# Display
		callwi(putx)			#  item
main.7: 	movwir(prompt,_si)		# Display
		callwi(putstr)			#  prompt
		movb1r(_OPT,_bp_,_dl)		# Display
		decl %esi			#  default
		callwi(putkey)			#  key
		xorb %ah,%ah			# BIOS: Get
		int $0x1a			#  system time
		movl %edx,%edi			# Ticks when
		addw1r(_TICKS,_bp_,_di) 	#  timeout
main.8: 	movb $0x1,%ah			# BIOS: Check
		int $0x16			#  for keypress
		jnz main.11			# Have one
		xorb %ah,%ah			# BIOS: Get
		int $0x1a			#  system time
		cmpl %edi,%edx			# Timeout?
		jb main.8			# No
main.9: 	movb1r(_OPT,_bp_,_al)		# Load default
		jmp main.12			# Join common code
main.10:	movb $0x7,%al			# Signal
		callwi(putchr)			#  error
main.11:	xorb %ah,%ah			# BIOS: Get
		int $0x16			#  keypress
		movb %ah,%al			# Scan code
		cmpb $KEY_ENTER,%al		# Enter pressed?
		je main.9			# Yes
		subb $KEY_F1,%al		# Less F1 scan code
		cmpb $0x4,%al			# F1..F5?
		ja main.10			# No
main.12:	cwtl				# Option
		btwr1(_ax,_MNUOPT,_bp_) 	#  enabled?
		jnc main.10			# No
		movbr1(_al,_OPT,_bp_)		# Save option
		movwir(FAKE,_si)		# Partition for write
		movb0r(_si_,_dl)		# Drive number
		movl %esi,%ebx			# Partition for read
		cmpb $0x4,%al			# F5 pressed?
		pushfl				# Save
		je main.13			# Yes
		shlb $0x4,%al			# Point to
		addwia(partbl)			#  selected
		xchgl %ebx,%eax 		#  partition
		movbi0(0x80,_bx_)		# Flag active
main.13:	pushl %ebx			# Save
		tstbi1(0x40,_FLAGS,_bp_)	# No updates?
		jnz main.14			# Yes
		movwir(start,_bx)		# Data to write
		movb $0x3,%ah			# Write sector
		callwi(intx13)			#  to disk
main.14:	popl %esi			# Restore
		popfl				# Restore
		jne main.15			# If not F5
		movb1r(_NXTDRV,_bp_,_dl)	# Next drive
		subb $'0',%dl			#  number
main.15:	movwir(LOAD,_bx)		# Address for read
		movb $0x2,%ah			# Read sector
		callwi(intx13)			#  from disk
		jc main.10			# If error
		cmpwi2(MAGIC,0x1fe,_bx_)	# Bootable?
		jne main.10			# No
		pushl %esi			# Save
		movwir(crlf,_si)		# Leave some
		callwi(puts)			#  space
		popl %esi			# Restore
		jmp *%ebx			# Invoke bootstrap

# Display routines

putkey: 	movb $'F',%al			# Display
		callwi(putchr)			#  'F'
		movb $'1',%al			# Prepare
		addb %dl,%al			#  digit
		jmp putstr.1			# Display the rest

putx:		btswr1(_dx,_MNUOPT,_bp_)	# Enable menu option
		movwir(item,_si)		# Display
		callwi(putkey)			#  key
		movl %edi,%esi			# Display the rest

puts:		callwi(putstr)			# Display string

putn:		movwir(crlf,_si)		# To next line

putstr: 	lodsb				# Get byte
		testb $0x80,%al 		# End of string?
		jnz putstr.2			# Yes
putstr.1:	callwi(putchr)			# Display char
		jmp putstr			# Continue
putstr.2:	andb $~0x80,%al 		# Clear MSB

putchr: 	pushl %ebx			# Save
		movwir(0x7,_bx) 		# Page:attribute
		movb $0xe,%ah			# BIOS: Display
		int $0x10			#  character
		popl %ebx			# Restore
		ret				# To caller

# One-sector disk I/O routine

intx13: 	movb1r(0x1,_si_,_dh)		# Load head
		movw1r(0x2,_si_,_cx)		# Load cylinder:sector
		movb $0x1,%al			# Sector count
		pushl %esi			# Save
		movl %esp,%edi			# Save
		tstbi1(0x80,_FLAGS,_bp_)	# Use packet interface?
		jz intx13.1			# No
		o16				# Set
		pushb $0x0			#  the
		o16				#  LBA
		pushw1(0x8,_si_)		#  address
		pushl %es			# Set the transfer
		pushl %ebx			#  buffer address
		pushb $0x1			# Block count
		pushb $0x10			# Packet size
		movl %esp,%esi			# Packet pointer
		decl %eax			# Verify off
		orb $0x40,%ah			# Use disk packet
intx13.1:	int $0x13			# BIOS: Disk I/O
		movl %edi,%esp			# Restore
		popl %esi			# Restore
		ret				# To caller

# Menu strings

item:		.ascii "  ";	     .byte ' '|0x80
prompt: 	.ascii "\nDefault:"; .byte ' '|0x80
crlf:		.ascii "\r";	     .byte '\n'|0x80

# Partition type tables

tables:
		.byte 0x0, 0x5, 0xf

		.byte 0x1, 0x4, 0x6, 0xb, 0xc, 0xe, 0x63, 0x83
		.byte 0xa5, 0xa6

		.byte os_misc-. 		# Unknown
		.byte os_dos-.			# DOS
		.byte os_dos-.			# DOS
		.byte os_dos-.			# DOS
		.byte os_dos-.			# Windows
		.byte os_dos-.			# Windows
		.byte os_dos-.			# Windows
		.byte os_unix-. 		# UNIX
		.byte os_linux-.		# Linux
		.byte os_freebsd-.		# FreeBSD
		.byte os_bsd-.			# OpenBSD

os_misc:	.ascii "?";    .byte '?'|0x80
os_dos: 	.ascii "DO";   .byte 'S'|0x80
os_unix:	.ascii "UNI";  .byte 'X'|0x80
os_linux:	.ascii "Linu"; .byte 'x'|0x80
os_freebsd:	.ascii "Free"
os_bsd: 	.ascii "BS";   .byte 'D'|0x80

		.org PRT_OFF-0xc,0x90

drive:		.ascii "Drive "
nxtdrv: 	.byte 0x0			# Next drive number
opt:		.byte 0x0			# Option
setdrv: 	.byte 0x80			# Drive to force
flags:		.byte FLAGS			# Flags
ticks:		.word TICKS			# Delay

partbl: 	.fill 0x40,0x1,0x0		# Partition table
		.word MAGIC			# Magic number
