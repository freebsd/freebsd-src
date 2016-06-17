#
#  Ipl block for fba devices
#    Copyright (C) 1998 IBM Corporation
#    Author(s): Martin Schwidefsky
#
#  startup for ipl at address 0
#  start with restart

# The first 24 byes are loaded by ipl to addresses 0-23 (a PSW and two CCWs).
# The CCWs on 8-23 are used as a continuation of the implicit ipl channel
# program. The fba ipl loader only uses the CCW on 8-15 to load the first 512
# byte block to location 0-511 (the reading starts again at block 0, byte 0).
# The second CCW is used to store the location of the load list.
        .org   0
        .long  0x00080000,0x80000000+_start    # The first 24 byte are loaded
        .long  0x02000000,0x20000200           # by ipl to addresses 0-23.
        .long  0x00000001,0x00000001           # (PSW, one CCW & loadlist info).

        .globl _start
_start:
	basr  %r13,0
.LPG0:	
	l     %r1,0xb8                         # load ipl subchannel number
        lhi   %r2,0x200                        # location for the loadlist
        lm    %r3,%r4,0x10                     # blocknr and length of loadlist
        bras  %r14,.Lloader                    # load loadlist

        lhi   %r11,0x400
        lhi   %r12,0x200                       # load address of loadlist
        l     %r3,0(%r12)                      # get first block number
        l     %r4,4(%r12)                      # get first block count
        la    %r12,8(%r12)
        j     .Llistloop
        .org  0x50
.Llistloop:
        lr    %r2,%r11                         # load address
        lr    %r5,%r4                          # block count
        mhi   %r5,512
        la    %r11,0(%r5,%r11)                 # update load address
        bras  %r14,.Lloader                    # load chunk of the image
        l     %r3,0(%r12)                      # get next block number
        icm   %r4,15,4(%r12)                   # get next block count
        la    %r12,8(%r12)
        jnz   .Llistloop

#
# everything loaded, go for it
#
        l     %r1,.Lstart-.LPG0(%r13)
	br    %r1

#
# subroutine for loading a sequence of block from fba
# %r2: load address (24 bit address)
# %r3: number of first block (unsigned long)
# %r4: number of blocks to load (unsigned short)
#
        .org  0xC0
.Lloader:
        la    %r5,.Llo-.LPG0(%r13)
        sth   %r4,2(%r5)                       # initialize block count
        st    %r3,4(%r5)                       # initialize block number
        la    %r5,.Lccws-.LPG0(%r13)
        mhi   %r4,512
        sth   %r4,22(%r5)                      # initialize byte count
	icm   %r2,8,16(%r5)
        st    %r2,16(%r5)                      # initialize CCW data address

        slr   %r2,%r2
        la    %r3,.Lorb-.LPG0(%r13)            # r2 = address of orb into r2
        la    %r4,.Ltinfo-.LPG0(%r13)          # r3 = address of tpi info block
        la    %r5,.Lirb-.LPG0(%r13)            # r4 = address of irb

        lctl  %c6,%c6,.Lc6-.LPG0(%r13)
.Lldlp:
        ssch  0(%r3)                           # read blocks
.Ltpi:
        tpi   0(%r4)                           # test pending interrupt
        jz    .Ltpi
        c     %r1,0(%r4)                       # compare subchannel number
        jne   .Ltpi
        tsch  0(%r5)
        slr   %r0,%r0
        tm    8(%r5),0x82                      # do we have a problem ?
        jnz   .Ldwpsw
        tm    8(%r5),0x04                      # got device end ?
        jz    .Ltpi
.Lexit:
        br    %r14

        .align 8
.Ldwpsw:.long  0x000a0000,0x00000000
.Lorb:	.long  0x00000000,0x0000ff00,.Lccws
.Ltinfo:.long  0
.Lirb:	.long  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
.Lc6:   .long  0xff000000
.Lloadp:.long  0,0
.Lparm:	.long  0x10400
.Lstart:.long  0x10000
        .align 8
.Lccws: .long  0x63000000+.Lde,0x60000010      # define extent
        .long  0x43000000+.Llo,0x60000008      # locate
# offset 1 in read CCW: data address (24 bit)
# offset 6 in read CCW: number of bytes (16 bit)
        .long  0x42000000,0x20000000           # read
.Lde:   .long  0x40000200,0x00000000
        .long  0x00000000,0x00001000
# offset 2 in .Llo: block count (unsigned short)
# offset 4 in .Llo: block number (unsigned long)
.Llo:   .long  0x06000000,0x00000000

        .org   0x200
        .long  0x00000002,0x0000007f
        .long  0x00000081,0x0000007f
        .long  0x00000100,0x0000007f
        .long  0x0000017f,0x0000007f
        .long  0x000001fe,0x0000007f
        .long  0x0000027d,0x0000007f
        .long  0x000002fc,0x0000007f
        .long  0x0000037b,0x0000007f
        .long  0x000003fa,0x0000007f
        .long  0x00000479,0x0000007f
        .long  0x000004f8,0x0000007f
        .long  0x00000577,0x0000007f
        .long  0x000005f6,0x0000007f
        .long  0x00000675,0x0000007f
        .long  0x000006f4,0x0000007f
        .long  0x00000773,0x0000003f
        .long  0x00000000,0x00000000
        .org   0x400

