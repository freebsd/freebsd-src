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
! This is the new redirector program, it replaces instbsdi.exe
! The program fetches some pointers from DOS and reports them back to
! the emulator via the emulator interrupt 0xff. It does not stay resident.

use16

.text
.bss
.data
.align 0

! Emulator interrupt entry
EmulatorINT = 0xFF
! Emulator redirector function
EmulatorRED = 0x1

! DOS interrupt
DOSInt      = 0x21

! DOS get list of lists call, returns pointer to system vars in ES:BX 
DOSGetList  = 0x52

! DOS get swappable area, returns DOS swappable area in DS:SI
DOSGetSwapD = 0x5D06

! DOS terminate program with return code
DOSExit	    = 0x4C

! DOS print message
DOSMesg	    = 0x9

cr      =       0xd
lf      =       0xa
eom     =       '$'             ! DOS end of string

	.org 0x100

.globl _main
_main:
	
	mov 	ah, #DOSGetList
	int 	DOSInt
	jc	Fail
	mov 	[_list], bx
	mov 	ax, es
	mov	[_list+2], ax
	push 	ds
	mov 	ax, #DOSGetSwapD
	int 	DOSInt
	mov	ax, ds
	pop	ds
	jc	Fail
	mov 	[_swap], si
	mov	[_swap+2], ax
	push	ax
	mov	ah, #EmulatorRED
	mov	dx, [_list]
	mov	bx, [_list+2]
	mov	si, [_swap]
	mov	di, [_swap+2]
	int	EmulatorINT
	xor	al, al			! return 0
	jmp	Exit
Fail:
	mov	ah, #DOSMesg
	mov	dx, #ErrorMessage
	int	DOSInt
	mov	al, #1			! return 1
Exit:
	mov	ah, #DOSExit
	int 	DOSInt
!
!	Should never get to this point
!
	nop
	nop
	hlt

! The two pointers are found using the DOS calls
! and passed to the redirector interface via int FF
! The two pointers are passed in BX:DX (list) and DI:SI (swap)

.align	2

_list:	.word 0
	.word 0
_swap:	.word 0
	.word 0

ErrorMessage:
	.ascii	"Error installing redirector interface"
	.byte	cr,lf,eom, 0
