/*
 *	bootsect.S		Copyright (C) 1991, 1992 Linus Torvalds
 *
 *	modified by Drew Eckhardt
 *	modified by Bruce Evans (bde)
 *	modified by Chris Noe (May 1999) (as86 -> gas)
 *
 * 360k/720k disk support: Andrzej Krzysztofowicz <ankry@green.mif.pg.gda.pl>
 *
 * BIG FAT NOTE: We're in real mode using 64k segments.  Therefore segment
 * addresses must be multiplied by 16 to obtain their respective linear
 * addresses. To avoid confusion, linear addresses are written using leading
 * hex while segment addresses are written as segment:offset.
 *
 * bde - should not jump blindly, there may be systems with only 512K low
 * memory.  Use int 0x12 to get the top of memory, etc.
 *
 * It then loads 'setup' directly after itself (0x90200), and the system
 * at 0x10000, using BIOS interrupts. 
 *
 * NOTE! currently system is at most (8*65536-4096) bytes long. This should 
 * be no problem, even in the future. I want to keep it simple. This 508 kB
 * kernel size should be enough, especially as this doesn't contain the
 * buffer cache as in minix (and especially now that the kernel is 
 * compressed :-)
 *
 * The loader has been made as simple as possible, and continuous
 * read errors will result in a unbreakable loop. Reboot by hand. It
 * loads pretty fast by getting whole tracks at a time whenever possible.
 */

#include <asm/boot.h>

SETUPSECTS	= 4			/* default nr of setup-sectors */
BOOTSEG		= 0x07C0		/* original address of boot-sector */
INITSEG		= DEF_INITSEG		/* we move boot here - out of the way */
SETUPSEG	= DEF_SETUPSEG		/* setup starts here */
SYSSEG		= DEF_SYSSEG		/* system loaded at 0x10000 (65536) */
SYSSIZE		= DEF_SYSSIZE		/* system size: # of 16-byte clicks */
					/* to be loaded */
ROOT_DEV	= 0 			/* ROOT_DEV is now written by "build" */
SWAP_DEV	= 0			/* SWAP_DEV is now written by "build" */

#ifndef SVGA_MODE
#define SVGA_MODE ASK_VGA
#endif

#ifndef RAMDISK
#define RAMDISK 0
#endif

#ifndef ROOT_RDONLY
#define ROOT_RDONLY 1
#endif

.code16
.text

.global _start
_start:

# First things first. Move ourself from 0x7C00 -> 0x90000 and jump there.

	movw	$BOOTSEG, %ax
	movw	%ax, %ds		# %ds = BOOTSEG
	movw	$INITSEG, %ax
	movw	%ax, %es		# %ax = %es = INITSEG
	movw	$256, %cx
	subw	%si, %si
	subw	%di, %di
	cld
	rep
	movsw
	ljmp	$INITSEG, $go

# bde - changed 0xff00 to 0x4000 to use debugger at 0x6400 up (bde).  We
# wouldn't have to worry about this if we checked the top of memory.  Also
# my BIOS can be configured to put the wini drive tables in high memory
# instead of in the vector table.  The old stack might have clobbered the
# drive table.

go:	movw	$0x4000-12, %di		# 0x4000 is an arbitrary value >=
					# length of bootsect + length of
					# setup + room for stack;
					# 12 is disk parm size.
	movw	%ax, %ds		# %ax and %es already contain INITSEG
	movw	%ax, %ss
	movw	%di, %sp		# put stack at INITSEG:0x4000-12.

# Many BIOS's default disk parameter tables will not recognize
# multi-sector reads beyond the maximum sector number specified
# in the default diskette parameter tables - this may mean 7
# sectors in some cases.
#
# Since single sector reads are slow and out of the question,
# we must take care of this by creating new parameter tables
# (for the first disk) in RAM.  We will set the maximum sector
# count to 36 - the most we will encounter on an ED 2.88.  
#
# High doesn't hurt.  Low does.
#
# Segments are as follows: %cs = %ds = %es = %ss = INITSEG, %fs = 0,
# and %gs is unused.

	movw	%cx, %fs		# %fs = 0
	movw	$0x78, %bx		# %fs:%bx is parameter table address
	pushw	%ds
	ldsw	%fs:(%bx), %si		# %ds:%si is source
	movb	$6, %cl			# copy 12 bytes
	pushw	%di			# %di = 0x4000-12.
	rep				# don't worry about cld
	movsw				# already done above
	popw	%di
	popw	%ds
	movb	$36, 0x4(%di)		# patch sector count
	movw	%di, %fs:(%bx)
	movw	%es, %fs:2(%bx)

# Get disk drive parameters, specifically number of sectors/track.

# It seems that there is no BIOS call to get the number of sectors.
# Guess 36 sectors if sector 36 can be read, 18 sectors if sector 18
# can be read, 15 if sector 15 can be read.  Otherwise guess 9.
# Note that %cx = 0 from rep movsw above.

	movw	$disksizes, %si		# table of sizes to try
probe_loop:
	lodsb
	cbtw				# extend to word
	movw	%ax, sectors
	cmpw	$disksizes+4, %si
	jae	got_sectors		# If all else fails, try 9

	xchgw	%cx, %ax		# %cx = track and sector
	xorw	%dx, %dx		# drive 0, head 0
	movw	$0x0200, %bx		# address = 512, in INITSEG (%es = %cs)
	movw	$0x0201, %ax		# service 2, 1 sector
	int	$0x13
	jc	probe_loop		# try next value

got_sectors:
	movb	$0x03, %ah		# read cursor pos
	xorb	%bh, %bh
	int	$0x10
	movw	$9, %cx
	movb	$0x07, %bl		# page 0, attribute 7 (normal)
					# %bh is set above; int10 doesn't
					# modify it
	movw	$msg1, %bp
	movw	$0x1301, %ax		# write string, move cursor
	int	$0x10			# tell the user we're loading..

# Load the setup-sectors directly after the moved bootblock (at 0x90200).
# We should know the drive geometry to do it, as setup may exceed first
# cylinder (for 9-sector 360K and 720K floppies).

	movw	$0x0001, %ax		# set sread (sector-to-read) to 1 as
	movw	$sread, %si		# the boot sector has already been read
	movw	%ax, (%si)

	xorw	%ax, %ax		# reset FDC
	xorb	%dl, %dl
	int	$0x13
	movw	$0x0200, %bx		# address = 512, in INITSEG
next_step:
	movb	setup_sects, %al
	movw	sectors, %cx
	subw	(%si), %cx		# (%si) = sread
	cmpb	%cl, %al
	jbe	no_cyl_crossing
	movw	sectors, %ax
	subw	(%si), %ax		# (%si) = sread
no_cyl_crossing:
	call	read_track
	pushw	%ax			# save it
	call	set_next		# set %bx properly; it uses %ax,%cx,%dx
	popw	%ax			# restore
	subb	%al, setup_sects	# rest - for next step
	jnz	next_step

	pushw	$SYSSEG
	popw	%es			# %es = SYSSEG
	call	read_it
	call	kill_motor
	call	print_nl

# After that we check which root-device to use. If the device is
# defined (!= 0), nothing is done and the given device is used.
# Otherwise, one of /dev/fd0H2880 (2,32) or /dev/PS0 (2,28) or /dev/at0 (2,8)
# depending on the number of sectors we pretend to know we have.

# Segments are as follows: %cs = %ds = %ss = INITSEG,
#	%es = SYSSEG, %fs = 0, %gs is unused.

	movw	root_dev, %ax
	orw	%ax, %ax
	jne	root_defined

	movw	sectors, %bx
	movw	$0x0208, %ax		# /dev/ps0 - 1.2Mb
	cmpw	$15, %bx
	je	root_defined

	movb	$0x1c, %al		# /dev/PS0 - 1.44Mb
	cmpw	$18, %bx
	je	root_defined

	movb	$0x20, %al		# /dev/fd0H2880 - 2.88Mb
	cmpw	$36, %bx
	je	root_defined

	movb	$0, %al			# /dev/fd0 - autodetect
root_defined:
	movw	%ax, root_dev

# After that (everything loaded), we jump to the setup-routine
# loaded directly after the bootblock:

	ljmp	$SETUPSEG, $0

# These variables are addressed via %si register as it gives shorter code.

sread:	.word 0				# sectors read of current track
head:	.word 0				# current head
track:	.word 0				# current track

# This routine loads the system at address SYSSEG, making sure
# no 64kB boundaries are crossed. We try to load it as fast as
# possible, loading whole tracks whenever we can.

read_it:
	movw	%es, %ax		# %es = SYSSEG when called
	testw	$0x0fff, %ax
die:	jne	die			# %es must be at 64kB boundary
	xorw	%bx, %bx		# %bx is starting address within segment
rp_read:
#ifdef __BIG_KERNEL__			# look in setup.S for bootsect_kludge
	bootsect_kludge = 0x220		# 0x200 + 0x20 which is the size of the
	lcall	bootsect_kludge		# bootsector + bootsect_kludge offset
#else
	movw	%es, %ax
	subw	$SYSSEG, %ax
	movw	%bx, %cx
	shr	$4, %cx
	add	%cx, %ax		# check offset
#endif
	cmpw	syssize, %ax		# have we loaded everything yet?
	jbe	ok1_read

	ret

ok1_read:
	movw	sectors, %ax
	subw	(%si), %ax		# (%si) = sread
	movw	%ax, %cx
	shlw	$9, %cx
	addw	%bx, %cx
	jnc	ok2_read

	je	ok2_read

	xorw	%ax, %ax
	subw	%bx, %ax
	shrw	$9, %ax
ok2_read:
	call	read_track
	call	set_next
	jmp	rp_read

read_track:
	pusha
	pusha	
	movw	$0xe2e, %ax 		# loading... message 2e = .
	movw	$7, %bx
 	int	$0x10
	popa		

# Accessing head, track, sread via %si gives shorter code.

	movw	4(%si), %dx		# 4(%si) = track
	movw	(%si), %cx		# (%si)  = sread
	incw	%cx
	movb	%dl, %ch
	movw	2(%si), %dx		# 2(%si) = head
	movb	%dl, %dh
	andw	$0x0100, %dx
	movb	$2, %ah
	pushw	%dx			# save for error dump
	pushw	%cx
	pushw	%bx
	pushw	%ax
	int	$0x13
	jc	bad_rt

	addw	$8, %sp
	popa
	ret

set_next:
	movw	%ax, %cx
	addw	(%si), %ax		# (%si) = sread
	cmp	sectors, %ax
	jne	ok3_set
	movw	$0x0001, %ax
	xorw	%ax, 2(%si)		# change head
	jne	ok4_set
	incw	4(%si)			# next track
ok4_set:
	xorw	%ax, %ax
ok3_set:
	movw	%ax, (%si)		# set sread
	shlw	$9, %cx
	addw	%cx, %bx
	jnc	set_next_fin
	movw	%es, %ax
	addb	$0x10, %ah
	movw	%ax, %es
	xorw	%bx, %bx
set_next_fin:
	ret

bad_rt:
	pushw	%ax			# save error code
	call	print_all		# %ah = error, %al = read
	xorb	%ah, %ah
	xorb	%dl, %dl
	int	$0x13
	addw	$10, %sp
	popa
	jmp read_track

# print_all is for debugging purposes.  
#
# it will print out all of the registers.  The assumption is that this is
# called from a routine, with a stack frame like
#
#	%dx 
#	%cx
#	%bx
#	%ax
#	(error)
#	ret <- %sp
 
print_all:
	movw	$5, %cx			# error code + 4 registers
	movw	%sp, %bp
print_loop:
	pushw	%cx			# save count remaining
	call	print_nl		# <-- for readability
	cmpb	$5, %cl
	jae	no_reg			# see if register name is needed
	
	movw	$0xe05 + 'A' - 1, %ax
	subb	%cl, %al
	int	$0x10
	movb	$'X', %al
	int	$0x10
	movb	$':', %al
	int	$0x10
no_reg:
	addw	$2, %bp			# next register
	call	print_hex		# print it
	popw	%cx
	loop	print_loop
	ret

print_nl:
	movw	$0xe0d, %ax		# CR
	int	$0x10
	movb	$0xa, %al		# LF
	int 	$0x10
	ret

# print_hex is for debugging purposes, and prints the word
# pointed to by %ss:%bp in hexadecimal.

print_hex:
	movw	$4, %cx			# 4 hex digits
	movw	(%bp), %dx		# load word into %dx
print_digit:
	rolw	$4, %dx			# rotate to use low 4 bits
	movw	$0xe0f, %ax		# %ah = request
	andb	%dl, %al		# %al = mask for nybble
	addb	$0x90, %al		# convert %al to ascii hex
	daa				# in only four instructions!
	adc	$0x40, %al
	daa
	int	$0x10
	loop	print_digit
	ret

# This procedure turns off the floppy drive motor, so
# that we enter the kernel in a known state, and
# don't have to worry about it later.
# NOTE: Doesn't save %ax or %dx; do it yourself if you need to.

kill_motor:
	xorw	%ax, %ax		# reset FDC
	xorb	%dl, %dl
	int	$0x13
	ret

sectors:	.word 0
disksizes:	.byte 36, 18, 15, 9
msg1:		.byte 13, 10
		.ascii "Loading"

# XXX: This is a fairly snug fit.

.org 497
setup_sects:	.byte SETUPSECTS
root_flags:	.word ROOT_RDONLY
syssize:	.word SYSSIZE
swap_dev:	.word SWAP_DEV
ram_size:	.word RAMDISK
vid_mode:	.word SVGA_MODE
root_dev:	.word ROOT_DEV
boot_flag:	.word 0xAA55
