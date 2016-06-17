/*
 *  arch/s390/boot/ipldump.S
 *
 *  S390 version
 *    Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *
 *  Tape dump ipl record. Put it on a tape and ipl from it and it will
 *  write a dump of the real storage after the ipl record on that tape.
 */

#include <asm/setup.h>
#include <asm/lowcore.h>

#define IPL_BS 1024
        .org   0
        .long  0x00080000,0x80000000+_start    # The first 24 bytes are loaded
        .long  0x07000000,0x60000001           # by ipl to addresses 0-23.
        .long  0x02000000,0x20000000+IPL_BS    # (a PSW and two CCWs).
        .long  0x00000000,0x00000000
        .long  0x00000000,0x00000000           # svc old psw
        .long  0x00000000,0x00000000           # program check old psw
        .long  0x00000000,0x00000000           # machine check old psw
        .long  0x00000000,0x00000000           # io old psw
        .long  0x00000000,0x00000000
        .long  0x00000000,0x00000000
        .long  0x00000000,0x00000000
        .long  0x000a0000,0x00000058           # external new psw
        .long  0x000a0000,0x00000060           # svc new psw
        .long  0x000a0000,0x00000068           # program check new psw
        .long  0x000a0000,0x00000070           # machine check new psw
        .long  0x00080000,0x80000000+.Lioint   # io new psw

        .org   0x100
        .globl _start
_start:
	l     %r1,0xb8                         # load ipl subchannel number
#
# find out memory size
#
        mvc   104(8),.Lpcmem0            # setup program check handler
        slr   %r3,%r3
        lhi   %r2,1
        sll   %r2,20
.Lloop0:
        l     %r0,0(%r3)                 # test page
        ar    %r3,%r2                    # add 1M
        jnm   .Lloop0                    # r1 < 0x80000000 -> loop
.Lchkmem0:
        n     %r3,.L4malign0             # align to multiples of 4M
        st    %r3,.Lmemsize              # store memory size
.Lmemok:

#
# first write a tape mark
#
        bras  %r14,.Ltapemark
#
# write real storage to tape
#	
 	slr   %r2,%r2                          # start at address 0
        bras  %r14,.Lwriter                    # load ramdisk
#
# write another tape mark
#
        bras  %r14,.Ltapemark
#
# everything written, stop processor
#
        lpsw  .Lstopped
#
# subroutine for writing to tape
# Paramters:	
#  R1 = device number
#  R2 = start address
#  R3 = length
.Lwriter:	
        st    %r14,.Lldret
        la    %r12,.Lorbread                   # r12 = address of orb 
	la    %r5,.Lirb                        # r5 = address of irb
        st    %r2,.Lccwwrite+4                 # initialize CCW data addresses
        lctl  %c6,%c6,.Lcr6               
        slr   %r2,%r2
.Lldlp:
        lhi   %r6,3                            # 3 retries
.Lssch:
        ssch  0(%r12)                          # write chunk of IPL_BS bytes
        jnz   .Llderr
.Lw4end:
        bras  %r14,.Lwait4io
        tm    8(%r5),0x82                      # do we have a problem ?
        jnz   .Lrecov
        l     %r0,.Lccwwrite+4                 # update CCW data addresses
        ahi   %r0,IPL_BS
        st    %r0,.Lccwwrite+4
        clr   %r0,%r3                          # enough ?
        jl    .Lldlp
.Ldone:
        l     %r14,.Lldret
        br    %r14                             # r2 contains the total size
.Lrecov:
        bras  %r14,.Lsense                     # do the sensing
        brct  %r6,.Lssch                       # dec. retry count & branch
        j     .Llderr
.Ltapemark:
        st    %r14,.Lldret
        la    %r12,.Lorbmark                   # r12 = address of orb
        la    %r5,.Lirb                        # r5 = address of irb
        lctl  %c6,%c6,.Lcr6
        ssch  0(%r12)                          # write a tape mark
        jnz   .Llderr
        bras  %r14,.Lwait4io
        l     %r14,.Lldret
        br    %r14
#
# Sense subroutine
#
.Lsense:
        st    %r14,.Lsnsret
        la    %r7,.Lorbsense              
        ssch  0(%r7)                           # start sense command
        jnz   .Llderr
        bras  %r14,.Lwait4io
        l     %r14,.Lsnsret
        tm    8(%r5),0x82                      # do we have a problem ?
        jnz   .Llderr
        br    %r14
#
# Wait for interrupt subroutine
#
.Lwait4io:
        lpsw  .Lwaitpsw                 
.Lioint:
        c     %r1,0xb8                         # compare subchannel number
        jne   .Lwait4io
        tsch  0(%r5)
        slr   %r0,%r0
        tm    8(%r5),0x82                      # do we have a problem ?
        jnz   .Lwtexit
        tm    8(%r5),0x04                      # got device end ?
        jz    .Lwait4io
.Lwtexit:
        br    %r14
.Llderr:
        lpsw  .Lcrash              

        .align 8
.Lorbread:
	.long  0x00000000,0x0080ff00,.Lccwwrite
        .align 8
.Lorbsense:
        .long  0x00000000,0x0080ff00,.Lccwsense
        .align 8
.Lorbmark:
        .long  0x00000000,0x0080ff00,.Lccwmark
        .align 8
.Lccwwrite:
        .long  0x01200000+IPL_BS,0x00000000
.Lccwsense:
        .long  0x04200001,0x00000000
.Lccwmark:
        .long  0x1f200001,0x00000000
.Lwaitpsw:
	.long  0x020a0000,0x80000000+.Lioint

.Lirb:	.long  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
.Lcr6:  .long  0xff000000
        .align 8
.Lcrash:.long  0x000a0000,0x00000000
.Lstopped: .long 0x000a0000,0x00001234
.Lpcmem0:.long  0x00080000,0x80000000 + .Lchkmem0
.L4malign0:.long 0xffc00000
.Lmemsize:.long 0
.Lldret:.long  0
.Lsnsret: .long 0

         .org IPL_BS

