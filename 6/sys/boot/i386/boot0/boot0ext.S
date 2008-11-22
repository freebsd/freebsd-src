/*
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 *
 * $FreeBSD$
 */

/* A 1024-byte boot manager. */

		.set NHRDRV,0x475		# Number of hard drives
		.set ORIGIN,0x600		# Execution address
		.set SECTOR_SIZE,0x200		# Length of a sector
		.set NUM_SECTORS,2		# Total length in sectors

		.set FAKE,ORIGIN+(SECTOR_SIZE*NUM_SECTORS) # Partition entry
		.set LOAD,0x7c00		# Load address

		.set PRT_OFF,0x1be		# Partition table

		.set TBL0SZ,table0_end-table0	# Table 0 size
		.set TBL1SZ,table1_end-table1	# Table 1 size

		.set MAGIC,0xaa55		# Magic: bootable

		.set KEY_ENTER,0x1c		# Enter key scan code
		.set KEY_F1,0x3b		# F1 key scan code
		.set KEY_1,0x02			# #1 key scan code

/*
 * Flag bits
 */
		.set FL_PACKET,0x80		# Packet mode
		.set FL_NOUPDATE,0x40		# Don't save selection
		.set FL_SETDRV,0x20		# Override drive number
/*
 * Addresses in the sector of embedded data values.
 * Accessed with negative offsets from the end of the relocated sectors (%bp).
 */
		.set _PRT_END,(FAKE-(ORIGIN+SECTOR_SIZE*(NUM_SECTORS-1)))
		.set _NXTDRV,-(_PRT_END+0x48)	# Next drive
		.set _OPT,-(_PRT_END+0x47)	# Default option
		.set _SETDRV,-(_PRT_END+0x46)	# Drive to force
		.set _FLAGS,-(_PRT_END+0x45)	# Flags
		.set _TICKS,-(_PRT_END+0x44)	# Timeout ticks
		.set _FAKE,0x0			# Fake partition entry
		.set _MNUOPT,0xc		# Menu options

		.globl start			# Entry point
		.code16				# This runs in real mode

/*
 * Initialise segments and registers to known values.
 * segments start at 0.
 * The stack is immediately below the address we were loaded to.
 *
 * Note that this section of code is used as the first signature check in
 * boot0cfg(8).
 */
start:		cld				# String ops inc
		xorw %ax,%ax			# Zero
		movw %ax,%es			# Address
		movw %ax,%ds			#  data
		movw %ax,%ss			# Set up
		movw $LOAD,%sp			#  stack
/*
 * End signature code
 */

/*
 * Set address for variable space beyond code, and clear it.
 * Notice that this is also used to point to the values embedded in the block,
 * by using negative offsets.
 */
		movw $fake,%bp			# Address variables
		movw %bp,%di			# %di used in stosw
		movw $0x8,%cx			# Words to clear
		rep				# Zero
		stosw				#  them
		incb -0xe(%di)			# Sector number 1
/*
 * Check what flags were loaded with us; specifically, use a predefined Drive.
 * If what the bios gives us is bad, use the '0' in the block instead, as well.
 */
		testb $FL_SETDRV,LOAD+flags-start # Set number drive?
		jnz boot.1			# Yes
		testb %dl,%dl			# Drive number valid?
		js boot.2			# Possibly (0x80 set)
boot.1:		movb LOAD+setdrv-start,%dl	# Drive number to use
boot.2:
/*
 * Reload all of boot0 (including the extra sectors) into memory at the
 * relocation address.
 */
		push %dx			# Save drive number
		movw $start,%bx			# Origin we were linked for
		movw %bp,%si			# Fake PTE
		movw $0x200+NUM_SECTORS,%ax	# Read in all
		callw intx13			#  of boot0
		pop %dx				# Restore
/*
 * Relocate to the new copy of the code.
 */
		jmp main+ORIGIN-LOAD		# To relocated code
/*
 * Whatever we decided to use, now store it into the fake
 * partition entry that lives in the data space above us.
 */
main:		movb %dl,_FAKE(%bp)		# Save drive number
		callw putn			# To new line
		pushw %dx			# Save drive number
/*
 * Start out with a pointer to the 4th byte of the first table entry
 * so that after 4 iterations it's beyond the end of the sector.
 * and beyond a 256 byte boundary and has overflowed 8 bits (see next comment).
 * (remember that the table starts 2 bytes earlier than you would expect
 * as the bootable flag is after it in the block)
 */
		movw $(partbl+0x4),%bx		# Partition table (+4)
		xorw %dx,%dx			# Item number
/*
 * Loop around on the partition table, printing values until we
 * pass a 256 byte boundary. The end of loop test is at main.5.
 */
main.3:		movb %ch,-0x4(%bx)		# Zero active flag (ch == 0)
		btw %dx,_FLAGS(%bp)		# Entry enabled?
		jnc main.5			# No
/*
 * If any of the entries in the table are
 * the same as the 'type' in the slice table entry,
 * then this is an empty or non bootable partition. Skip it.
 */
		movb (%bx),%al			# Load type
		movw $tables,%di		# Lookup tables
		movb $TBL0SZ,%cl		# Number of entries
		repne				# Exclude
		scasb				#  partition?
		je main.5			# Yes
/*
 * Now scan the table of known types
 */
		movb $TBL1SZ,%cl		# Number of entries
		repne				# Known
		scasb				#  type?
		jne main.4			# No
/*
 * If it matches get the matching element in the
 * next array.  If it doesn't, we are already
 * pointing at its first element which points to a "?".
 */
		addw $TBL1SZ,%di		# Adjust
main.4:		movb (%di),%cl			# Partition
		addw %cx,%di			#  description
		callw putx			# Display it
main.5:		incw %dx			# Next item
		addb $0x10,%bl			# Next entry
		jnc main.3			# Till done
/*
 * Passed a 256 byte boundary..
 * table is finished.
 * Add one to the drive number and check it is valid,
 */
		popw %ax			# Drive number
		subb $0x80-0x1,%al		# Does next
		cmpb NHRDRV,%al			#  drive exist? (from BIOS?)
		jb main.6			# Yes
/*
 * If not then if there is only one drive,
 * Don't display drive as an option.
 */
		decw %ax			# Already drive 0?
		jz main.7			# Yes
/*
 * If it was illegal or we cycled through them,
 * then go back to drive 0.
 */
		xorb %al,%al			# Drive 0
/*
 * Whatever drive we selected, make it an ascii digit and save it back
 * to the "next drive" location in the loaded block in case we
 * want to save it for next time.
 * This also is part of the printed drive string so add 0x80 to indicate
 * end of string.
 */
main.6:		addb $'0'|0x80,%al		# Save next
		movb %al,_NXTDRV(%bp)		#  drive number
		movw $drive,%di			# Display
		callw putx			#  item
/*
 * Now that we've printed the drive (if we needed to), display a prompt.
 * Get ready for the input by noting the time.
 */
main.7:		movw $prompt,%si		# Display
		callw putstr			#  prompt
		movb _OPT(%bp),%dl		# Display
		decw %si			#  default
		callw putkey			#  key
		xorb %ah,%ah			# BIOS: Get
		int $0x1a			#  system time
		movw %dx,%di			# Ticks when
		addw _TICKS(%bp),%di	 	#  timeout
/*
 * Busy loop, looking for keystrokes but
 * keeping one eye on the time.
 */
main.8:		movb $0x1,%ah			# BIOS: Check
		int $0x16			#  for keypress
		jnz main.11			# Have one
		xorb %ah,%ah			# BIOS: Get
		int $0x1a			#  system time
		cmpw %di,%dx			# Timeout?
		jb main.8			# No
/*
 * If timed out or defaulting, come here.
 */
main.9:		movb _OPT(%bp),%al		# Load default
		jmp main.12			# Join common code
/*
 * User's last try was bad, beep in displeasure.
 * Since nothing was printed, just continue on as if the user
 * hadn't done anything. This gives the effect of the user getting a beep
 * for all bad keystrokes but no action until either the timeout
 * occurs or the user hits a good key.
 */
main.10:	movb $0x7,%al			# Signal
		callw putchr			#  error
/*
 * Get the keystroke.
 */
main.11:	xorb %ah,%ah			# BIOS: Get
		int $0x16			#  keypress
		movb %ah,%al			# Scan code
/*
 * If it's CR act as if timed out.
 */
		cmpb $KEY_ENTER,%al		# Enter pressed?
		je main.9			# Yes
/*
 * Otherwise check if legal
 * If not ask again.
 */
		subb $KEY_F1,%al		# Less F1 scan code
		cmpb $0x4,%al			# F1..F5?
		jna main.12			# Yes
		subb $(KEY_1 - KEY_F1),%al	# Less #1 scan code
		cmpb $0x4,%al			# #1..#5?
		ja main.10			# No
/*
 * We have a selection.
 * but if it's a bad selection go back to complain.
 * The bits in MNUOPT were set when the options were printed.
 * Anything not printed is not an option.
 */
main.12:	cbtw				# Option
		btw %ax,_MNUOPT(%bp)	 	#  enabled?
		jnc main.10			# No
/*
 * Save the info in the original tables
 * for rewriting to the disk.
 */
		movb %al,_OPT(%bp)		# Save option
		movw $fake,%si			# Partition for write
		movb (%si),%dl			# Drive number
		movw %si,%bx			# Partition for read
		cmpb $0x4,%al			# F5/#5 pressed?
		pushf				# Save
		je main.13			# Yes
		shlb $0x4,%al			# Point to
		addw $partbl,%ax		#  selected
		xchgw %bx,%ax	 		#  partition
		movb $0x80,(%bx)		# Flag active
/*
 * If not asked to do a write-back (flags 0x40) don't do one.
 */
main.13:	pushw %bx			# Save
		testb $FL_NOUPDATE,_FLAGS(%bp)	# Skip update?
		jnz main.14			# Yes
		movw $start,%bx			# Data to write
		movw $0x301,%ax			# Write 1 sector
		callw intx13			#  to disk
main.14:	popw %si			# Restore
		popf				# Restore
/*
 * If going to next drive, replace drive with selected one.
 * Remember to un-ascii it. Hey 0x80 is already set, cool!
 */
		jne main.15			# If not F5/#5
		movb _NXTDRV(%bp),%dl		# Next drive
		subb $'0',%dl			#  number
/*
 * load  selected bootsector to the LOAD location in RAM.
 * If it fails to read or isn't marked bootable, treat it
 * as a bad selection.
 */
main.15:	movw $LOAD,%bx			# Address for read
		movw $0x201,%ax			# Read 1 sector
		callw intx13			#  from disk
		jc main.10			# If error
		cmpw $MAGIC,0x1fe(%bx)		# Bootable?
		jne main.10			# No
		callw putn			# Leave some space
		jmp *%bx			# Invoke bootstrap

/*
 * Display routines
 */

putkey:		movb $'F',%al			# Display
		callw putchr			#  'F'
		movb $'1',%al			# Prepare
		addb %dl,%al			#  digit
		jmp putstr.1			# Display the rest

/*
 * Display the option and note that it is a valid option.
 * That last point is a bit tricky..
 */
putx:		btsw %dx,_MNUOPT(%bp)		# Enable menu option
		movw $item,%si			# Display
		callw putkey			#  key
		movw %di,%si			# Display the rest

puts:		callw putstr			# Display string

putn:		movw $crlf,%si			# To next line

putstr:		lodsb				# Get byte
		testb $0x80,%al 		# End of string?
		jnz putstr.2			# Yes
putstr.1:	callw putchr			# Display char
		jmp putstr			# Continue
putstr.2:	andb $~0x80,%al 		# Clear MSB

putchr:		pushw %bx			# Save
		movw $0x7,%bx	 		# Page:attribute
		movb $0xe,%ah			# BIOS: Display
		int $0x10			#  character
		popw %bx			# Restore
		retw				# To caller

/*
 * One-sector disk I/O routine
 *
 * Calling conventions: (assumes %si -> partition table entry)
 *
 * 0x1(%si)	- byte - head
 * 0x2(%si)	- word - cylinder/sector
 * 0x8(%si)	- long - LBA to use if needed
 * %ah		- byte - operation, 2 = read, 3 = write
 * %al		- byte - sector count
 * %dl		- byte - drive number
 * %es:(%bx)	- void - buffer to use for transfer
 *
 * If the head == 0xff, and cylinder/sector == 0xffff, then try
 * to use the EDD stuff with the LBA offset instead of CHS.  However,
 * use CHS if at all possible.
 */

intx13:		movb 0x1(%si),%dh		# Load head
		movw 0x2(%si),%cx		# Load cylinder:sector
		pushw %si			# Save
		movw %sp,%di			# Save
		cmpb $0xff,%dh			# Might we need LBA?
		jne intx13.2			# No, just use CHS
		cmpw $0xffff,%cx		# Do we need LBA?
		jne intx13.2			# No
		testb $FL_PACKET,_FLAGS(%bp)	# Try the packet interface?
		jz intx13.2			# No
		pushw %cx			# Save
		pushw %bx			# Save
		movw $0x55aa,%bx		# Magic
		pushw %ax			# Save
		movb $0x41,%ah			# BIOS: EDD extensions
		int $0x13			#  present?
		popw %ax			# Restore
		jc intx13.1			# Not present, use CHS
		cmpw $0xaa55,%bx		# Magic?
		jne intx13.1			# Not present, use CHS
		testb $0x1,%cl			# Packet mode available?
		jz intx13.1			# No, use CHS
		orb $0x40,%ah			# Use disk packet
intx13.1:	popw %bx			# Restore
		popw %cx			# Restore
		testb $0x40,%ah			# Using packet mode?
		jz intx13.2			# No, so skip the rest
		pushl $0x0			# Set the
		pushl 0x8(%si)			# LBA address
		pushw %es			# Set the transfer
		pushw %bx			#  buffer address
		push $0x0			# Punch a hole in the stack
		push $0x10			# Packet size
		movw %sp,%si			# Packet pointer
		xchgb %al,0x2(%si)		# Set the block count in the
						#  packet and zero %al,
						#  turning verify off for writes
intx13.2:	int $0x13			# BIOS: Disk I/O
		movw %di,%sp			# Restore
		popw %si			# Restore
		retw				# To caller

		.org PRT_OFF-0xe,0x90
/*
 * These values are sometimes changed before writing back to the drive
 * Be especially careful that nxtdrv: must come after drive:, as it
 * is part of the same string.
 *
 * Note that the 'drive' string variable is used as the second signature
 * check in boot0cfg(8).
 */
version_minor:	.byte 0x1			# minor version
version_major:	.byte 0x1			# major version
drive:		.ascii "Drive "
nxtdrv:		.byte 0x0			# Next drive number
opt:		.byte 0x0			# Option
setdrv:		.byte 0x80			# Drive to force
flags:		.byte FLAGS			# Flags
ticks:		.word TICKS			# Delay

/*
 * here is the 64 byte partition table that fdisk would fiddle with.
 */
partbl:		.fill 0x40,0x1,0x0		# Partition table
		.word MAGIC			# Magic number

/*
 * start of sector two.. ugh
 */
		.org SECTOR_SIZE,0x90

/* Menu strings */

item:		.ascii "  ";	     .byte ' '|0x80
prompt:		.ascii "\nDefault:"; .byte ' '|0x80
crlf:		.ascii "\r";	     .byte '\n'|0x80

/* Partition type tables */

tables:
/*
 * These entries identify invalid or NON BOOT types and partitions.
 */
table0:		.byte 0x0, 0x5, 0xf
table0_end:
/*
 * These values indicate bootable types we know the names of
 */
table1:		.byte 0x1, 0x4, 0x6, 0x7, 0xb, 0xc, 0xe, 0x42, 0x63, 0x83
		.byte 0x9f, 0xa5, 0xa6, 0xa9
table1_end:
/*
 * These are offsets that match the known names above and point to the strings
 * that will be printed.
 */
		.byte os_misc-. 		# Unknown
		.byte os_dos-.			# DOS
		.byte os_dos-.			# DOS
		.byte os_dos-.			# DOS
		.byte os_nt-.			# NT/XP or OS/2
		.byte os_windows-.		# Windows 32-bit FAT
		.byte os_windows-.		# Windows 32-bit FAT ext int 13
		.byte os_windows-.		# Windows
		.byte os_windows-.		# Windows 2000 dyn ext
		.byte os_unix-. 		# UNIX
		.byte os_linux-.		# Linux
		.byte os_bsdos-.		# BSD/OS
		.byte os_freebsd-.		# FreeBSD
		.byte os_openbsd-.		# OpenBSD
		.byte os_netbsd-.		# NetBSD
/*
 * And here are the strings themselves. 0x80 or'd into a byte indicates
 * the end of the string. (not so great for Russians but...)
 */
os_misc:	.ascii "Unknow";	.byte 'n'|0x80
os_dos:		.ascii "DO";		.byte 'S'|0x80
os_nt:		.ascii "Windows NT/X";	.byte 'P'|0x80
os_windows:	.ascii "Window";	.byte 's'|0x80
os_unix:	.ascii "UNI";		.byte 'X'|0x80
os_linux:	.ascii "Linu";		.byte 'x'|0x80
os_freebsd:	.ascii "FreeBS";	.byte 'D'|0x80
os_openbsd:	.ascii "OpenBS";	.byte 'D'|0x80
os_netbsd:	.ascii "NetBS";		.byte 'D'|0x80
os_bsdos:	.ascii "BSD/O";		.byte 'S'|0x80

/*
 * Fake partition entry created at the end of the table used when loading
 * boot0 at the very beginning and when loading an MBR from another disk when
 * F5 is pressed.
 */
		.org SECTOR_SIZE*NUM_SECTORS, 0x0
fake:
