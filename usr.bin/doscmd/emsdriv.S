! Copyright (c) 1997 Helmut Wirth <hfwirth@ping.at>
! All rights reserved.
!
! Redistribution and use in source and binary forms, with or without
! modification, are permitted provided that the following conditions
! are met:
! 1. Redistributions of source code must retain the above copyright
!    notice immediately at the beginning of the file, witout modification,
!    this list of conditions, and the following disclaimer.
! 2. Redistributions in binary form must reproduce the above copyright
!    notice, this list of conditions and the following disclaimer in the
!    documentation and/or other materials provided with the distribution.
! 3. The name of the author may not be used to endorse or promote products
!    derived from this software without specific prior written permission.
!
! THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
! IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
! OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
! IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
! INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
! NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
! DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
! THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
! (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
! THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
!
! $FreeBSD$


!
! This driver is needed for Expanded Memory emulation (EMS). A driver
! is needed here, because EMS drivers are installed as DOS devices
! with the name "EMMXXXX0" and programs look for such a device while
! checking the existence of EMS. 
! The driver is installed by CONFIG.SYS, it has no options. It uses
! the emulator callback interrupt 0xff to initialize the EMS subsystem.
! If EMS is not configured or if there is an error inside the emulator
! the driver reports failure and does not install itself.
! If all works, the driver changes the interrupt vector for int 0x67 to
! point at itself. The resident part of the drivers simlpy routes calls
! to int 0x67 to the correct subfunction of the emulator callback interrupt.

use16

.text
.bss
.data
.align 0

	.org	0

NumFunc	=	15

! Emulator interrupt entry
EmulatorINT = 0xFF

! Emulator EMS callback function
EMU_EMS 	= 0x2
EMU_EMS_CTL 	= 0
EMU_EMS_CALL 	= 1

! DOS print message
DOSMesg     = 0x9

cr      =       0xd
lf      =       0xa
eom     =       '$'             ! DOS end of string

EMSintnum = 0x67
Intoffset   = 	(EMSintnum * 4)

.globl _main
_main:

driverhead:
	.long	-1		! link to next device driver
	.word	0xC000		! attribute word for driver 
	.word	Strategy	! ptr to strategy routine
	.word	Interrupt	! ptr to interrupt service routine
	.ascii	"EMMXXXX0"	! logical device name

reqhead:
	.long	0

vectordone: 
	.word 	0		! != 0 , if vector installed

FuncTable:
	.word	InitDrv		! initialize driver
	.word	Noop		! media Check 
	.word	Noop		! build BPB
	.word	Noop		! Ioctl
	.word	Noop		! read
	.word	Noop  		! non destructive read
	.word	Noop		! input status
	.word	Noop		! flush input
	.word	Noop		! write
	.word	Noop		! write with verify
	.word	Noop		! output status
	.word	Noop		! flush output
	.word	Noop		! ioctl output
	.word	Noop		! open device
	.word	Noop		! close device
	.word	Noop		! removeable media check

Strategy:
	seg cs
	mov	[reqhead], bx
	seg cs
	mov	[reqhead+2],es
	retf

Interrupt:
	push	ax
	push	bx
	push	cx
	push	dx
	push	ds
	push	es
	push	di
	push	si
	push	bp
	
	push	cs
	pop	ds

	les	di,[reqhead]		! load pointer to request header

	seg es
	movb	bl,[di+2]
	xorb	bh,bh
	cmp	bx, #NumFunc
	jle	dointr
	call	errorhandler
	jmp	intrend

dointr:	
	shl	bx,#1
	call	[bx+FuncTable]
	les	di,[reqhead]		! load pointer to request header

intrend:
	or	ax,#0x100		! done bit
	seg es
	mov	[di+3],ax

	pop	bp
	pop	si
	pop	di
	pop	es
	pop	ds
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	retf

errorhandler:
	mov	ax,#0x8003		! report error to caller
	ret


! This is done for all functions except init. It supports the different
! methods for an EMS installation check described in the LIM EMS 4.0 spec
Noop:
	call 	installvector
	xor	ax,ax
	ret

! The interrupt vector installed for int 0x67 points to this routine
intr67:
	push	ax			! Save original AX
	mov	ah, #EMU_EMS		! Emuint function
	mov	al, #EMU_EMS_CALL	! Emuint subfunction
	int	EmulatorINT		! Call emulator for EMS
	iret

installvector:
	push cs
	pop ds			! load DS to use local data
	mov ax,[vectordone]
	cmp ax,#0
	jne isinstalled		! already installed

	push	di		! save request header pointer
	push	es

	mov	ax, #0		! write the new interrupt vector
	mov	es, ax
	mov	di, #Intoffset
	seg	es
	mov	[di], #intr67
	seg	es
	mov	[di+2], cs

	pop	es
	pop	di

	mov ax,#1
	mov [vectordone],ax

isinstalled:
	ret

InitDrv:
	push	cs
	pop	ds
	push	ax
	mov	ah, #EMU_EMS            ! Emuint function
	mov	al, #EMU_EMS_CTL        ! Emuint subfunction
	int	EmulatorINT
	cmp	ax,#0			! check if successful
	je	Fail
	
	call	installvector

	push	cs
	pop	ds

	mov	ah, #DOSMesg
	mov	dx, #Success
	int	0x21

	seg es
	mov	[di+14], #InitDrv	! address break for driver
	seg es
	mov	[di+16], cs

	xor ax,ax
	ret

Fail:
	push cs
	pop ds
	mov	ah,  #DOSMesg
	mov	dx,  #Failure
	int	0x21

	seg es 
	movb	[di+13],#0
	seg es
	mov	[di+20],cs
	seg es
	mov	[di+14],#0		! address break == 0, no driver
	seg es
	mov	[di+16],cs
	ret


Success:	
	.ascii	"Doscmd EMS 4.0 driver installed"
	.byte	cr,lf,eom

Failure:
	.byte	cr,lf,lf
	.ascii	"EMS emulation is disabled"
	.byte	cr,lf
	.ascii	"Driver not installed"
	.byte	cr,lf,lf,eom

	end
