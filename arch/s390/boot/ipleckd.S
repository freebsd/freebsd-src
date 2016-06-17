#
#  arch/s390/boot/ipleckd.S
#    IPL record for 3380/3390 DASD
#
#  S390 version
#    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
#    Author(s): Holger Smolinski <Holger.Smolinski@de.ibm.com>
#
#
# FIXME:	 should use the countarea to determine the blocksize
# FIXME:	 should insert zeroes into memory when filling holes
# FIXME:	 calculate blkpertrack from rdc data and blksize

# change 09/20/00       removed obsolete store of ipldevice to textesegment

# Usage of registers
# r1:	ipl subchannel ( general use, dont overload without save/restore !)
# r10:
# r13:	base register 	index to 0x0000
# r14:	callers address
# r15:	temporary save register (we have no stack!)

# storage layout:

#include <asm/lowcore.h>
	
	.org 0
.psw:	.long 0x00080000,0x80000000+_start
.ccw1:	.long 0x06000000,0x00001000     # Re-Read enough of bootsector to start
.ccw2:	.long 0x00000000,0x00000000	# read countarea of record 1 to s/w.

	.org 0x58
.Lextn:	.long	0x000a0000,0x00000000+.Lextn
.Lsvcn:	.long	0x000a0000,0x00000000+.Lsvcn
.Lprgn:	.long	0x00080000,0x00000000+.Lecs
.Lmcn:	.long	0x000a0000,0x00000000+.Lmcn
.Lion:	.long   0x00080000,0x80000000+.Lionewaddr

	.org 0xe0
.Llstad:.long 	0x00000000,0x00000000	# sectorno + ct of bootlist

	.org 0xf0			# Lets start now...
_start: .globl _start
	l  	%r1,__LC_SUBCHANNEL_ID	# get IPL-subchannel from lowcore
	st	%r1,__LC_IPLDEV         # keep it for reipl
	stsch	.Lrdcdata
	oi      .Lrdcdata+5,0x84        # enable ssch and multipath mode
.Lecs:  xi	.Lrdcdata+27,0x01	# enable concurrent sense
	msch	.Lrdcdata	
        xi      .Lprgn,6                # restore Wait and d/a bit in PCnew PSW
	l	%r2,.Lparm
	mvc     0x0(8,%r2),.Lnull       # set parmarea to null
	lctl	%c6,%c6,.Lc6     	# enable all interrupts
.Lrdc:					# read device characteristics
	la	%r6,.Lrdcccw     
	st      %r6,.Lorb+8		# store cp-address to orb
	bras    %r15,.Lssch		# start I/O
	oi	.Llodata+1,0x80
	lh	%r5,.Lcountarea+6	# init r5 from countarea
	stcm	%r5,3,.Lrdccw+2		# and store into rd template *FIXME*
	stcm	%r5,3,.Llodata+14	# and store into lodata *FIXME*
.Lbootlist:	
	l	%r2,.Llstad	
	l	%r3,.Lblklst     
	lhi	%r4,1
	bras	%r14,.Lreadblks
.Lloader:	
	l	%r10,.Lblklst     	# r10 is index to bootlist
	lhi	%r5,4			# r5:	skip 4 blocks = firstpage....
.Lkloop:
	clc	.Lnull(8),0(%r10)	# test blocklist	
	jz	.Lchkparm		# end of list?
	l	%r2,0(%r10)		# get startblock to r2
	slr	%r4,%r4			# erase r4
	icm	%r4,1,7(%r10)		# get blockcount
	slr	%r3,%r3			# get address to r3
	icm 	%r3,0xe,4(%r10)
	chi	%r5,0			# still blocks to skip?
	jz	.Ldoread		# no: start reading
	cr	%r5,%r4			# #skipblocks >= blockct?
	jm	.L007			# no: skip the blocks one by one
.L006:	
	sr	%r5,%r4			# decrease number of blocks to skip
	j	.Lkcont			# advance to next entry
.L007:	
	ahi	%r2,1			# skip 1 block...
	bctr    %r4,0                   # update blockct
	ah	%r3,.Lcountarea+6       # increment address
	bct	%r5,.L007		# 4 blocks skipped?
.Ldoread:
	ltr	%r2,%r2			# test startblock
	jz	.Lzeroes		# startblocks is zero (hole)
.Ldiskread:	
	bras	%r14,.Lreadblks
	j	.Lkcont
.Lzeroes:
	lr	%r2,%r3
.L001:	slr	%r3,%r3
	icm	%r3,3,.Lcountarea+6     # get blocksize
	slr	%r5,%r5			# no bytes to move
.L008:	mvcle	%r2,%r4,0   		# fill zeroes to storage
	jo	.L008			# until block is filled
	brct	%r4,.L001   		# skip to next block
.Lkcont:	
	ahi	%r10,8
	j	.Lkloop
.Lchkparm:
	lm	%r3,%r4,.Lstart         # load .Lstart and .Lparm
	clc	0x0(4,%r4),.Lnull     
	je	.Lrunkern
	mvc	0x480(128,%r3),0(%r4)	# move 1k-0x80 to parmarea
	mvc	0x500(256,%r3),0x80(%r4)
	mvc	0x600(256,%r3),0x180(%r4)
	mvc	0x700(256,%r3),0x280(%r4)
.Lrunkern:
#	lhi	%r2,17
#	sll	%r2,12
#	st	%r1,0xc6c(%r2)		# store iplsubchannel to lowcore
#	st	%r1,0xc6c		# store iplsubchannel to lowcore
	br	%r3
# This function does the start IO
# r2:	number of first block to read ( input by caller )
# r3:	address to read data to ( input by caller )
# r4:	number of blocks to read ( input by caller )
# r5:	destroyed
# r6:	blocks per track ( input by caller )
# r7:	number of heads 
# r8:	
# r9:	
# r10:	
# r11:	temporary register
# r12:	local use for base address
# r13:	base address for module
# r14:	address of caller for subroutine
# r15:	temporary save register (since we have no stack)
.Lreadblks:
	la	%r12,.Ldeccw     
	st	%r12,8+.Lorb		# store cpaddr to orb
	ahi	%r12,0x10		# increment r12 to point to rdccw
	oi	1(%r12),0x40		# set CC in rd template
	# first setup the read CCWs
	lr	%r15,%r4		# save number or blocks
	slr	%r7,%r7
	icm	%r7,3,.Lrdcdata+14      # load heads to r7
	lhi     %r6,9
	clc     .Lrdcdata+3(2),.L9345
	je	.L011
	lhi	%r6,10
	clc	.Lrdcdata+3(2),.L3380
	je	.L011
	lhi	%r6,12
	clc	.Lrdcdata+3(2),.L3390     
	je	.L011			
        bras 	%r14,.Ldisab
.L011:	
	# loop for nbl times
.Lrdloop:	
	mvc	0(8,%r12),.Lrdccw     	# copy template to this ccw
	st	%r3,4(%r12)		# store target address to this ccw
	bct	%r4,.L005		# decrement no of blks still to do
	ni	1(%r12),0x3f		# delete CC from last ccw
	lr	%r4,%r15		# restore number of blocks
	# read CCWs are setup now		
	stcm	%r4,3,.Llodata+2     	# store blockno to lodata clears r4
	ar	%r4,%r2			# r4 (clear): ebl = blk + nbl
	bctr    %r4,0			# decrement r4 ( last blk touched 
	srda	%r2,32			# trk = blk / bpt, bot = blk % bpt 
	dr	%r2,%r6			# r3: trk, r2: bot
	ahi	%r2,1			# bot++ ( we start counting at 1 )
	stcm	%r2,1,.Llodata+12     	# store bot to lodata
	xr 	%r2,%r2			# cy  = trk / heads, hd  = trk % heads
	dr	%r2,%r7			# r3: cy, r2: hd
	sll	%r3,16			# combine to CCHH in r3
	or	%r3,%r2
	st	%r3,.Ldedata+8     	# store cchh to dedata	
	st	%r3,.Llodata+4     	# store cchh to lodata	
	st	%r3,.Llodata+8     	# store cchh to lodata	
	lr	%r15,%r5		# save r5
	srda	%r4,32			# tr2 = ebl / bpt
	dr	%r4,%r6			# r5: tr2, r4: bot2
	xr 	%r4,%r4			# cy2 = tr2 / heads, hd2 = hd2 % heads
	dr	%r4,%r7			# r5: cy2, r4: hd2 
	stcm	%r5,3,.Ldedata+12     	# store cy2,hd2 to dedata
	stcm	%r4,3,.Ldedata+14     	# store cy2,hd2 to dedata
	lr	%r5,%r15		# restore r5
	# CCWs are setup now, arent they?
	bras	%r15,.Lssch		# start I/O
	br	%r14			# return to caller
.L005:	
	ah 	%r3,.Lcountarea+6     	# add blocksize to target address
	ahi	%r12,8			# add sizeof(ccw) to base address
	j	.Lrdloop
# end of function
# This function does the start IO
# r1:	Subchannel number
# r8:	ORB address
# r9:	IRB address
.Lssch:
	lhi     %r13,10			# initialize retries
.L012:	
	ssch	.Lorb			# start I/O
	jz	.Ltpi			# ok?
	bras	%r14,.Ldisab		# error
.Ltpi:	
	lpsw	.Lwaitpsw     		# load wait-PSW
.Lionewaddr:	
	c	%r1,0xb8     		# compare to ipl subhchannel
	jnz	.Ltpi			# not equal: loop
	clc	0xbc(4),.Lorb 		# cross check the intparm
	jnz	.Ltpi               	# not equal:	loop 
	tsch    .Lirb			# get status
	tm	.Lirb+9,0xff		# channel status ?
	jz	.L003			# CS == 0x00
	bras	%r14,.Ldisab		# error
.L003:
	tm	.Lirb+8,0xf3		# DS different from CE/DE
	jz	.L004			# ok ?
	bct	%r13,.L012		# retries <= 5 ?
	bras	%r14,.Ldisab		# error
.L004:
	tm	.Lirb+8,0x04		# DE set?
	jz	.Ltpi			# DE not set, loop
.Lsschend:
	br	%r15			# return to caller
# end of function
# In case of error goto disabled wait with %r14 containing the caller
.Ldisab:
	st	%r14,.Ldisabpsw+4     
	lpsw	.Ldisabpsw     

# FIXME pre-initialized data should be listed first
# NULLed storage can be taken from anywhere ;) 
.Lblklst:	
	.long   0x00002000     
	.align 8
.Ldisabpsw: 
	.long 0x000a0000,0x00000000
.Lwaitpsw:
	.long 0x020a0000,0x00000000+.Ltpi
.Lorb:	
	.long 0x0049504c,0x0080ff00	# intparm is " IPL"
.Lc6:	.long 0xff000000
.Lstart:
	.long	0x00010000              # do not separate .Lstart and .Lparm
.Lparm:	
	.long	0x00008000              # they are loaded with a LM
.L3390:
	.word 	0x3390
.L9345:
	.word	0x9345
.L3380:
	.word	0x3380
.Lnull:	
	.long 0x00000000,0x00000000
	.align 4
.Lrdcdata:
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
.Lirb:
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
	.long 0x00000000,0x00000000
.Lcountarea:	
	.word 0x0000			# cyl;
	.word 0x0000			# head;
	.byte 0x00			# record;
	.byte 0x00			# key length;
	.word 0x0000			# data length == blocksize;
.Ldedata:
	.long 0x40c00000,0x00000000
	.long 0x00000000,0x00000000
.Llodata:
	.long 0x06000001,0x00000000
	.long 0x00000000,0x01000000
	.long 0x12345678
	.org 0x7c8
.Lrdcccw:				# CCW read device characteristics
	.long 0x64400040,0x00000000+.Lrdcdata
	.long 0x63400010,0x00000000+.Ldedata
	.long 0x47400010,0x00000000+.Llodata
	.long 0x12000008,0x00000000+.Lcountarea
.Ldeccw:
	.long 0x63400010,0x00000000+.Ldedata
.Lloccw:
	.long 0x47400010,0x00000000+.Llodata
.Lrdccw:	
	.long 0x86400000,0x00000000
	.org 0x800
# end of pre initialized data is here CCWarea follows
# from here we load 1k blocklist 
# end of function

