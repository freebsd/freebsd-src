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

# A 512-byte boot manager.

		.set NHRDRV,0x475		# Number of hard drives
		.set ORIGIN,0x600		# Execution address
		.set FAKE,0x800 		# Partition entry
		.set LOAD,0x7c00		# Load address

		.set PRT_OFF,0x1be		# Partition table

		.set TBL0SZ,0x3 		# Table 0 size
		.set TBL1SZ,0xb 		# Table 1 size

		.set MAGIC,0xaa55		# Magic: bootable

		.set KEY_ENTER,0x1c		# Enter key scan code
		.set KEY_F1,0x3b		# F1 key scan code

#
# Addresses in the sector of embedded data values.
# Accessed with negative offsets from the end of the relocated sector (%ebp).
#
		.set _NXTDRV,-0x48		# Next drive
		.set _OPT,-0x47 		# Default option
		.set _SETDRV,-0x46		# Drive to force
		.set _FLAGS,-0x45		# Flags
		.set _TICKS,-0x44		# Timeout ticks
		.set _FAKE,0x0			# Fake partition entry
		.set _MNUOPT,0xc		# Menu options

		.globl start			# Entry point
		.code16				# This runs in real mode

#
# Initialise segments and registers to known values.
# segments start at 0.
# The stack is immediatly below the address we were loaded to.
#
start:		cld				# String ops inc
		xorw %ax,%ax			# Zero
		movw %ax,%es			# Address
		movw %ax,%ds			#  data
		movw %ax,%ss			# Set up
		movw $LOAD,%sp			#  stack
	
#
# Copy this code to the address it was linked for
#
		movw %sp,%si			# Source
		movw $start,%di			# Destination
		movw $0x100,%cx			# Word count
		rep				# Relocate
		movsw				#  code
#
# Set address for variable space beyond code, and clear it.
# Notice that this is also used to point to the values embedded in in the block,
# by using negative offsets.
#
		movw %di,%bp			# Address variables
		movb $0x8,%cl			# Words to clear
		rep				# Zero
		stosw				#  them
#
# Relocate to the new copy of the code.
#
		incb1(-0xe,_di_)		# Sector number
		/* incb $-0xe(%di) */
		jmpnwi(main-LOAD+ORIGIN)	# To relocated code
		/* jmp main-LOAD+ORIGIN */
#
# Check what flags were loaded with us, specifically, Use a predefined Drive.
# If what the bios gives us is bad, use the '0' in the block instead, as well.
#
main:		tstbi1(0x20,_FLAGS,_bp_)	# Set number drive?
		/* testb $(0x20+_FLAGS)(%bp) */
		.code32
		jnz main.1			# Yes
		.code16
		testb %dl,%dl			# Drive number valid?
		.code32
		js main.2			# Possibly (0x80 set)
		.code16
main.1: 	movb1r(_SETDRV,_bp_,_dl)	# Drive number to use
		/* movb $_SETDRV(%bp),%dl */
#
# Whatever we decided to use, now store it into the fake
# partition entry that lives in the data space above us.
#
main.2: 	movbr1(_dl,_FAKE,_bp_)		# Save drive number
		/* movb %dl,$_FAKE(%bp) */
		callwi(putn)			# To new line
		pushw %dx			# Save drive number
#
# Start out with a pointer to the 4th byte of the first table entry
# so that after 4 iterations it's beyond the end of the sector.
# and beyond a 256 byte boundary and thos overflowed 8 bits (see next comment).
# (remember that the table starts 2 bytes earlier than you would expect
# as the bootable flag is after it in the block)
#
		movw $(partbl+0x4),%bx		# Partition table (+4)
		xorw %dx,%dx			# Item number
#
# Loop around on the partition table, printing values until we
# pass a 256 byte boundary. The end of loop test is at main.5.
#
main.3: 	movbr1(_ch,-0x4,_bx_)		# Zero active flag (ch == 0)
	 	/* movb %ch,$-0x4(%bx) */
		btwr1(_dx,_FLAGS,_bp_)		# Entry enabled?
		/* bt %dx,$_FLAGS(%bp) */
		.code32
		jnc main.5			# No
		.code16
#
# If any of the entries in the table are
# the same as the 'type' in the slice table entry,
# then this is an empty or non bootable partition. Skip it.
#
		movb0r(_bx_,_al)		# Load type
		/* movb (%bx),%al */
		movw $tables,%di		# Lookup tables
		movb $TBL0SZ,%cl		# Number of entries
		repne				# Exclude
		scasb				#  partition?
		.code32
		je main.5			# Yes
		.code16
#
# Now scan the table of known types
#
		movb $TBL1SZ,%cl		# Number of entries
		repne				# Known
		scasb				#  type?
		.code32
		jne main.4			# No
		.code16
#
# If it matches get the matching element in the
# next array. if it doesn't, we are already
# pointing at its first element which points to a "?".
#
		addw $TBL1SZ,%di		# Adjust
main.4: 	movb0r(_di_,_cl)		# Partition
		/* movb (%di),%cl */
		addw %cx,%di			#  description
		callwi(putx)			# Display it
main.5: 	incw %dx			# Next item 
		addb $0x10,%bl			# Next entry
		.code32
		jnc main.3			# Till done
		.code16
#
# Passed a 256 byte boundary..
# table is finished.
# Add one to the drive number and check it is valid, 
#
		popw %ax			# Drive number
		subb $0x80-0x1,%al		# Does next
		cmpb NHRDRV,%al			#  drive exist? (from BIOS?)
		.code32
		jb main.6			# Yes
		.code16
# If not then if there is only one drive,
# Don't display drive as an option.
#
		decw %ax			# Already drive 0?
		.code32
		jz main.7			# Yes
		.code16
# If it was illegal or we cycled through them,
# then go back to drive 0.
#
		xorb %al,%al			# Drive 0
#
# Whatever drive we selected, make it an ascii digit and save it back
# to the "next drive" location in the loaded block in case we
# want to save it for next time.
# This also is part of the printed drive string so add 0x80 to indicate
# end of string.
#
main.6: 	addb $'0'|0x80,%al		# Save next
		movbr1(_al,_NXTDRV,_bp_)	#  drive number
		/* movb %al,$NXTDRV(%bp) */
		movw $drive,%di			# Display
		callwi(putx)			#  item
#
# Now that we've printed the drive (if we needed to), display a prompt.
# Get ready for the input byt noting the time.
#
main.7: 	movw $prompt,%si		# Display
		callwi(putstr)			#  prompt
		movb1r(_OPT,_bp_,_dl)		# Display
		/* movb $_OPT(%bp),%dl */
		decw %si			#  default
		callwi(putkey)			#  key
		xorb %ah,%ah			# BIOS: Get
		int $0x1a			#  system time
		movw %dx,%di			# Ticks when
		addw1r(_TICKS,_bp_,_di) 	#  timeout
		/* addw $_TICKS(%bp),%di */
# 
# Busy loop, looking for keystrokes but
# keeping one eye on the time.
#
main.8: 	movb $0x1,%ah			# BIOS: Check
		int $0x16			#  for keypress
		.code32
		jnz main.11			# Have one
		.code16
		xorb %ah,%ah			# BIOS: Get
		int $0x1a			#  system time
		cmpw %di,%dx			# Timeout?
		.code32
		jb main.8			# No
		.code16
#
# If timed out or defaulting, come here.
#
main.9: 	movb1r(_OPT,_bp_,_al)		# Load default
		/* movb $_OPT(%bp),%al */
		.code32
		jmp main.12			# Join common code
		.code16
#
# User's last try was bad, beep in displeasure.
# Since nothing was printed, just continue on as if the user
# hadn't done anything. This gives the effect of the user getting a beep 
# for all bad keystrokes but no action until either the timeout
# occurs or the user hits a good key.
#
main.10:	movb $0x7,%al			# Signal
		callwi(putchr)			#  error
#
# Get the keystroke.
#
main.11:	xorb %ah,%ah			# BIOS: Get
		int $0x16			#  keypress
		movb %ah,%al			# Scan code
#
# If it's CR act as if timed out.
#
		cmpb $KEY_ENTER,%al		# Enter pressed?
		.code32
		je main.9			# Yes
		.code16
#
# Otherwise check if legal
# If not ask again.
#
		subb $KEY_F1,%al		# Less F1 scan code
		cmpb $0x4,%al			# F1..F5?
		.code32
		ja main.10			# No
		.code16
#
# We have a selection.
# but if it's a bad selection go back to complain.
# The bits in MNUOPT were set when the options were printed.
# Anything not printed is not an option.
#
main.12:	cbtw				# Option
		btwr1(_ax,_MNUOPT,_bp_) 	#  enabled?
		/* btw %ax,$_MNUOPT(%bp) */
		.code32
		jnc main.10			# No
		.code16
#
# Save the info in the original tables
# for rewriting to the disk.
#
		movbr1(_al,_OPT,_bp_)		# Save option
		/* movb %al,$OPT(%bp) */
		movw $FAKE,%si			# Partition for write
		movb0r(_si_,_dl)		# Drive number
		/* movb (%si),%dl */
		movw %si,%bx			# Partition for read
		cmpb $0x4,%al			# F5 pressed?
		pushf				# Save
		.code32
		je main.13			# Yes
		.code16
		shlb $0x4,%al			# Point to
		addw $partbl,%ax		#  selected
		xchgw %bx,%ax	 		#  partition
		movbi0(0x80,_bx_)		# Flag active
		/* movb $0x80,(%bx) */
#
# If not asked to do a write-back (flags 0x40) don't do one.
#
main.13:	pushw %bx			# Save
		tstbi1(0x40,_FLAGS,_bp_)	# No updates?
		/* testb $0x40,$_FLAGS(%bp) */
		.code32
		jnz main.14			# Yes
		.code16
		movw $start,%bx			# Data to write
		movb $0x3,%ah			# Write sector
		callwi(intx13)			#  to disk
main.14:	popw %si			# Restore
		popf				# Restore
#
# If going to next drive, replace drive with selected one.
# Remember to un-ascii it. Hey 0x80 is already set, cool!
#
		.code32
		jne main.15			# If not F5
		.code16
		movb1r(_NXTDRV,_bp_,_dl)	# Next drive
		/* movb $_NXTDRV(%bp),%dl */
		subb $'0',%dl			#  number
# 
# load  selected bootsector to the LOAD location in RAM.
# If it fails to read or isn't marked bootable, treat it
# as a bad selection.
# XXX what does %si carry?
#
main.15:	movw $LOAD,%bx			# Address for read
		movb $0x2,%ah			# Read sector
		callwi(intx13)			#  from disk
		.code32
		jc main.10			# If error
		.code16
		cmpwi2(MAGIC,0x1fe,_bx_)	# Bootable?
		/* cmpw $MAGIC,$0x1fe(%bx) */
		.code32
		jne main.10			# No
		.code16
		pushw %si			# Save
		movw $crlf,%si			# Leave some
		callwi(puts)			#  space
		popw %si			# Restore
		.code32
		jmp *%ebx			# Invoke bootstrap
		/* jmp *%bx */
		.code16
#
# Display routines
#

putkey: 	movb $'F',%al			# Display
		callwi(putchr)			#  'F'
		movb $'1',%al			# Prepare
		addb %dl,%al			#  digit
		.code32
		jmp putstr.1			# Display the rest
		.code16

#
# Display the option and note that it is a valid option.
# That last point is a bit tricky..
#
putx:		btswr1(_dx,_MNUOPT,_bp_)	# Enable menu option
		/* btsw %dx,$_MNUOPT(%bp) */
		movw $item,%si			# Display
		callwi(putkey)			#  key
		movw %di,%si			# Display the rest

puts:		callwi(putstr)			# Display string

putn:		movw $crlf,%si			# To next line

putstr: 	lodsb				# Get byte
		testb $0x80,%al 		# End of string?
		.code32
		jnz putstr.2			# Yes
		.code16
putstr.1:	callwi(putchr)			# Display char
		.code32
		jmp putstr			# Continue
		.code16
putstr.2:	andb $~0x80,%al 		# Clear MSB

putchr: 	pushw %bx			# Save
		movw $0x7,%bx	 		# Page:attribute
		movb $0xe,%ah			# BIOS: Display
		int $0x10			#  character
		popw %bx			# Restore
		retn				# To caller

# One-sector disk I/O routine

intx13: 	movb1r(0x1,_si_,_dh)		# Load head
		/* movb $0x1(%si),%dh */
		movw1r(0x2,_si_,_cx)		# Load cylinder:sector
		/* movw $0x2(%si),%cx */
		movb $0x1,%al			# Sector count
		pushw %si			# Save
		movw %sp,%di			# Save
		tstbi1(0x80,_FLAGS,_bp_)	# Use packet interface?
		/* testb $0x80,$_FLAGS(%bp) */
		.code32
		jz intx13.1			# No
		.code16
		pushl $0x0			# Set the
		o16				#  LBA
		pushw1(0x8,_si_)		#  address
		/* pushl $0x8(%si) */
		pushw %es			# Set the transfer
		pushw %bx			#  buffer address
		push  $0x1			# Block count
		push  $0x10			# Packet size
		movw %sp,%si			# Packet pointer
		decw %ax			# Verify off
		orb $0x40,%ah			# Use disk packet
intx13.1:	int $0x13			# BIOS: Disk I/O
		movw %di,%sp			# Restore
		popw %si			# Restore
		retn				# To caller

# Menu strings

item:		.ascii "  ";	     .byte ' '|0x80
prompt: 	.ascii "\nDefault:"; .byte ' '|0x80
crlf:		.ascii "\r";	     .byte '\n'|0x80

# Partition type tables

tables:
#
# These entries identify invalid or NON BOOT types and partitions.
#
		.byte 0x0, 0x5, 0xf
#
# These valuse indicate botable types we know the names of
#
		.byte 0x1, 0x4, 0x6, 0xb, 0xc, 0xe, 0x63, 0x83
		.byte 0xa5, 0xa6, 0xa9
#
# These are offsets that match the known names above and point to the strings
# that will be printed.
#
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
		.byte os_bsd-.			# NetBSD
#
# And here are the strings themselves. 0x80 or'd into a byte indicates 
# the end of the string. (not so great for Russians but...)
#
os_misc:	.ascii "?";    .byte '?'|0x80
os_dos: 	.ascii "DO";   .byte 'S'|0x80
os_unix:	.ascii "UNI";  .byte 'X'|0x80
os_linux:	.ascii "Linu"; .byte 'x'|0x80
os_freebsd:	.ascii "Free"
os_bsd: 	.ascii "BS";   .byte 'D'|0x80

		.org PRT_OFF-0xc,0x90
#
# These values are sometimes changed before writing back to the drive
# Be especially careful that nxtdrv: must come after drive:, as it 
# is part of the same string.
#
drive:		.ascii "Drive "
nxtdrv: 	.byte 0x0			# Next drive number
opt:		.byte 0x0			# Option
setdrv: 	.byte 0x80			# Drive to force
flags:		.byte FLAGS			# Flags
ticks:		.word TICKS			# Delay

#
# here is the 64 byte partition table that fdisk would fiddle with.
#
partbl: 	.fill 0x40,0x1,0x0		# Partition table
		.word MAGIC			# Magic number
