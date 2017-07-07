[bits 32]
[CPU intelnop]

; Copyright (c) 2010, Intel Corporation
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
;     * Redistributions of source code must retain the above copyright notice,
;       this list of conditions and the following disclaimer.
;     * Redistributions in binary form must reproduce the above copyright notice,
;       this list of conditions and the following disclaimer in the documentation
;       and/or other materials provided with the distribution.
;     * Neither the name of Intel Corporation nor the names of its contributors
;       may be used to endorse or promote products derived from this software
;       without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
; IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
; INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
; BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
; LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
; OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
; ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%define _iEncExpandKey128 k5_iEncExpandKey128
%define _iEncExpandKey256 k5_iEncExpandKey256
%define _iDecExpandKey128 k5_iDecExpandKey128
%define _iDecExpandKey256 k5_iDecExpandKey256
%define _iEnc128_CBC      k5_iEnc128_CBC
%define _iEnc256_CBC      k5_iEnc256_CBC
%define _iDec128_CBC      k5_iDec128_CBC
%define _iDec256_CBC      k5_iDec256_CBC

%macro inversekey 1
	movdqu  xmm1,%1
	aesimc	xmm0,xmm1
	movdqu	%1,xmm0
%endmacro


%macro aesdec4 1
	movdqa	xmm4,%1

	aesdec	xmm0,xmm4
	aesdec	xmm1,xmm4
	aesdec	xmm2,xmm4
	aesdec	xmm3,xmm4

%endmacro


%macro aesdeclast4 1
	movdqa	xmm4,%1

	aesdeclast	xmm0,xmm4
	aesdeclast	xmm1,xmm4
	aesdeclast	xmm2,xmm4
	aesdeclast	xmm3,xmm4

%endmacro


%macro aesenc4 1
	movdqa	xmm4,%1

	aesenc	xmm0,xmm4
	aesenc	xmm1,xmm4
	aesenc	xmm2,xmm4
	aesenc	xmm3,xmm4

%endmacro

%macro aesenclast4 1
	movdqa	xmm4,%1

	aesenclast	xmm0,xmm4
	aesenclast	xmm1,xmm4
	aesenclast	xmm2,xmm4
	aesenclast	xmm3,xmm4

%endmacro


%macro aesdeclast1 1
	aesdeclast	xmm0,%1
%endmacro

%macro aesenclast1 1
	aesenclast	xmm0,%1
%endmacro

%macro aesdec1 1
	aesdec	xmm0,%1
%endmacro

;abab
%macro aesenc1 1
	aesenc	xmm0,%1
%endmacro


%macro aesdeclast1_u 1
	movdqu xmm4,%1
	aesdeclast	xmm0,xmm4
%endmacro

%macro aesenclast1_u 1
	movdqu xmm4,%1
	aesenclast	xmm0,xmm4
%endmacro

%macro aesdec1_u 1
	movdqu xmm4,%1
	aesdec	xmm0,xmm4
%endmacro

%macro aesenc1_u 1
	movdqu xmm4,%1
	aesenc	xmm0,xmm4
%endmacro


%macro load_and_xor4 2
	movdqa	xmm4,%2
	movdqu	xmm0,[%1 + 0*16]
	pxor	xmm0,xmm4
	movdqu	xmm1,[%1 + 1*16]
	pxor	xmm1,xmm4
	movdqu	xmm2,[%1 + 2*16]
	pxor	xmm2,xmm4
	movdqu	xmm3,[%1 + 3*16]
	pxor	xmm3,xmm4
%endmacro


%macro xor_with_input4 1
	movdqu xmm4,[%1]
	pxor xmm0,xmm4
	movdqu xmm4,[%1+16]
	pxor xmm1,xmm4
	movdqu xmm4,[%1+32]
	pxor xmm2,xmm4
	movdqu xmm4,[%1+48]
	pxor xmm3,xmm4
%endmacro

%macro store4 1
	movdqu [%1 + 0*16],xmm0
	movdqu [%1 + 1*16],xmm1
	movdqu [%1 + 2*16],xmm2
	movdqu [%1 + 3*16],xmm3
%endmacro


%macro copy_round_keys 3
	movdqu xmm4,[%2 + ((%3)*16)]
	movdqa [%1 + ((%3)*16)],xmm4
%endmacro

;abab
%macro copy_round_keyx 3
	movdqu xmm4,[%2 + ((%3)*16)]
	movdqa %1,xmm4
%endmacro



%macro key_expansion_1_192 1
		;; Assumes the xmm3 includes all zeros at this point.
        pshufd xmm2, xmm2, 11111111b
        shufps xmm3, xmm1, 00010000b
        pxor xmm1, xmm3
        shufps xmm3, xmm1, 10001100b
        pxor xmm1, xmm3
		pxor xmm1, xmm2
		movdqu [edx+%1], xmm1
%endmacro

; Calculate w10 and w11 using calculated w9 and known w4-w5
%macro key_expansion_2_192 1
		movdqa xmm5, xmm4
		pslldq xmm5, 4
		shufps xmm6, xmm1, 11110000b
		pxor xmm6, xmm5
		pxor xmm4, xmm6
		pshufd xmm7, xmm4, 00001110b
		movdqu [edx+%1], xmm7
%endmacro





section .rodata
align 16
shuffle_mask:
DD 0FFFFFFFFh
DD 03020100h
DD 07060504h
DD 0B0A0908h


section .text



align 16
key_expansion256:

    pshufd xmm2, xmm2, 011111111b

    movdqu xmm4, xmm1
    pshufb xmm4, xmm5
    pxor xmm1, xmm4
    pshufb xmm4, xmm5
    pxor xmm1, xmm4
    pshufb xmm4, xmm5
    pxor xmm1, xmm4
    pxor xmm1, xmm2

    movdqu [edx], xmm1
    add edx, 0x10

    aeskeygenassist xmm4, xmm1, 0
    pshufd xmm2, xmm4, 010101010b

    movdqu xmm4, xmm3
    pshufb xmm4, xmm5
    pxor xmm3, xmm4
    pshufb xmm4, xmm5
    pxor xmm3, xmm4
    pshufb xmm4, xmm5
    pxor xmm3, xmm4
    pxor xmm3, xmm2

    movdqu [edx], xmm3
    add edx, 0x10

    ret



align 16
key_expansion128:
    pshufd xmm2, xmm2, 0xFF;
    movdqu xmm3, xmm1
    pshufb xmm3, xmm5
    pxor xmm1, xmm3
    pshufb xmm3, xmm5
    pxor xmm1, xmm3
    pshufb xmm3, xmm5
    pxor xmm1, xmm3
    pxor xmm1, xmm2

    ; storing the result in the key schedule array
    movdqu [edx], xmm1
    add edx, 0x10
    ret



align 16
global _iEncExpandKey128
_iEncExpandKey128:

	mov ecx,[esp-4+8]		;input
	mov edx,[esp-4+12]		;ctx

        movdqu xmm1, [ecx]    ; loading the key

        movdqu [edx], xmm1

        call .next
.next:
        pop ecx
        movdqa xmm5, [ecx-.next+shuffle_mask]

        add edx,16

        aeskeygenassist xmm2, xmm1, 0x1     ; Generating round key 1
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x2     ; Generating round key 2
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x4     ; Generating round key 3
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x8     ; Generating round key 4
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x10    ; Generating round key 5
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x20    ; Generating round key 6
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x40    ; Generating round key 7
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x80    ; Generating round key 8
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x1b    ; Generating round key 9
        call key_expansion128
        aeskeygenassist xmm2, xmm1, 0x36    ; Generating round key 10
        call key_expansion128

	ret



align 16
global _iDecExpandKey128
_iDecExpandKey128:
	push DWORD [esp+8]
	push DWORD [esp+8]

	call _iEncExpandKey128
	add esp,8

	mov edx,[esp-4+12]		;ctx

	inversekey	[edx + 1*16]
	inversekey	[edx + 2*16]
	inversekey	[edx + 3*16]
	inversekey	[edx + 4*16]
	inversekey	[edx + 5*16]
	inversekey	[edx + 6*16]
	inversekey	[edx + 7*16]
	inversekey	[edx + 8*16]
	inversekey	[edx + 9*16]

	ret



align 16
global _iDecExpandKey256
_iDecExpandKey256:
	push DWORD [esp+8]
	push DWORD [esp+8]

	call _iEncExpandKey256
	add esp, 8

	mov edx, [esp-4+12]		;expanded key

	inversekey	[edx + 1*16]
	inversekey	[edx + 2*16]
	inversekey	[edx + 3*16]
	inversekey	[edx + 4*16]
	inversekey	[edx + 5*16]
	inversekey	[edx + 6*16]
	inversekey	[edx + 7*16]
	inversekey	[edx + 8*16]
	inversekey	[edx + 9*16]
	inversekey	[edx + 10*16]
	inversekey	[edx + 11*16]
	inversekey	[edx + 12*16]
	inversekey	[edx + 13*16]

	ret




align 16
global _iEncExpandKey256
_iEncExpandKey256:
	mov ecx, [esp-4+8]		;input
	mov edx, [esp-4+12]		;expanded key


    movdqu xmm1, [ecx]    ; loading the key
    movdqu xmm3, [ecx+16]
    movdqu [edx], xmm1  ; Storing key in memory where all key schedule will be stored
    movdqu [edx+16], xmm3

    add edx,32

    call .next
.next:
    pop ecx
    movdqa xmm5, [ecx-.next+shuffle_mask]  ; this mask is used by key_expansion

    aeskeygenassist xmm2, xmm3, 0x1     ;
    call key_expansion256
    aeskeygenassist xmm2, xmm3, 0x2     ;
    call key_expansion256
    aeskeygenassist xmm2, xmm3, 0x4     ;
    call key_expansion256
    aeskeygenassist xmm2, xmm3, 0x8     ;
    call key_expansion256
    aeskeygenassist xmm2, xmm3, 0x10    ;
    call key_expansion256
    aeskeygenassist xmm2, xmm3, 0x20    ;
    call key_expansion256
    aeskeygenassist xmm2, xmm3, 0x40    ;
;    call key_expansion256

    pshufd xmm2, xmm2, 011111111b

    movdqu xmm4, xmm1
    pshufb xmm4, xmm5
    pxor xmm1, xmm4
    pshufb xmm4, xmm5
    pxor xmm1, xmm4
    pshufb xmm4, xmm5
    pxor xmm1, xmm4
    pxor xmm1, xmm2

    movdqu [edx], xmm1


	ret



align 16
global _iDec128_CBC
_iDec128_CBC:
	mov ecx,[esp-4+8]

	push esi
	push edi
	push ebp
	mov ebp,esp
	sub esp,16*16
	and esp,0xfffffff0

	mov eax,[ecx+12]
	movdqu xmm5,[eax]	;iv

	mov eax,[ecx+16] ; numblocks
	mov esi,[ecx]
	mov edi,[ecx+4]
	mov ecx,[ecx+8]

	sub edi,esi

	test eax,eax
	jz end_dec128_CBC

	cmp eax,4
	jl	lp128decsingle_CBC

	test	ecx,0xf
	jz		lp128decfour_CBC

	copy_round_keys esp,ecx,0
	copy_round_keys esp,ecx,1
	copy_round_keys esp,ecx,2
	copy_round_keys esp,ecx,3
	copy_round_keys esp,ecx,4
	copy_round_keys esp,ecx,5
	copy_round_keys esp,ecx,6
	copy_round_keys esp,ecx,7
	copy_round_keys esp,ecx,8
	copy_round_keys esp,ecx,9
	copy_round_keys esp,ecx,10
	mov ecx,esp


align 16
lp128decfour_CBC:

	test eax,eax
	jz end_dec128_CBC

	cmp eax,4
	jl	lp128decsingle_CBC

	load_and_xor4 esi, [ecx+10*16]
	add esi,16*4
	aesdec4 [ecx+9*16]
	aesdec4 [ecx+8*16]
	aesdec4 [ecx+7*16]
	aesdec4 [ecx+6*16]
	aesdec4 [ecx+5*16]
	aesdec4 [ecx+4*16]
	aesdec4 [ecx+3*16]
	aesdec4 [ecx+2*16]
	aesdec4 [ecx+1*16]
	aesdeclast4 [ecx+0*16]

	pxor	xmm0,xmm5
	movdqu	xmm4,[esi- 16*4 + 0*16]
	pxor	xmm1,xmm4
	movdqu	xmm4,[esi- 16*4 + 1*16]
	pxor	xmm2,xmm4
	movdqu	xmm4,[esi- 16*4 + 2*16]
	pxor	xmm3,xmm4
	movdqu	xmm5,[esi- 16*4 + 3*16]

	sub eax,4
	store4 esi+edi-(16*4)
	jmp lp128decfour_CBC


	align 16
lp128decsingle_CBC:

	movdqu xmm0, [esi]
	movdqa xmm1,xmm0
	movdqu xmm4,[ecx+10*16]
	pxor xmm0, xmm4
	aesdec1_u  [ecx+9*16]
	aesdec1_u  [ecx+8*16]
	aesdec1_u  [ecx+7*16]
	aesdec1_u  [ecx+6*16]
	aesdec1_u  [ecx+5*16]
	aesdec1_u  [ecx+4*16]
	aesdec1_u  [ecx+3*16]
	aesdec1_u  [ecx+2*16]
	aesdec1_u  [ecx+1*16]
	aesdeclast1_u [ecx+0*16]

	pxor	xmm0,xmm5
	movdqa	xmm5,xmm1

	add esi, 16
	movdqu  [edi+esi - 16], xmm0
	dec eax
	jnz lp128decsingle_CBC

end_dec128_CBC:

	mov esp,ebp
	pop ebp
	pop edi
	pop esi

	mov ecx,[esp-4+8]   ; first arg
	mov ecx,[ecx+12]
	movdqu	[ecx],xmm5 ; store last iv for chaining

	ret



align 16
global _iDec256_CBC
_iDec256_CBC:
	mov ecx,[esp-4+8]

	push esi
	push edi
	push ebp
	mov ebp,esp

	sub esp,16*16
	and esp,0xfffffff0

	mov eax,[ecx+12]
	movdqu xmm5,[eax]	;iv

	mov eax,[ecx+16] ; numblocks
	mov esi,[ecx]
	mov edi,[ecx+4]
	mov ecx,[ecx+8]

	sub edi,esi

	test eax,eax
	jz end_dec256_CBC

	cmp eax,4
	jl	lp256decsingle_CBC

	test	ecx,0xf
	jz	lp256decfour_CBC

	copy_round_keys esp,ecx,0
	copy_round_keys esp,ecx,1
	copy_round_keys esp,ecx,2
	copy_round_keys esp,ecx,3
	copy_round_keys esp,ecx,4
	copy_round_keys esp,ecx,5
	copy_round_keys esp,ecx,6
	copy_round_keys esp,ecx,7
	copy_round_keys esp,ecx,8
	copy_round_keys esp,ecx,9
	copy_round_keys esp,ecx,10
	copy_round_keys esp,ecx,11
	copy_round_keys esp,ecx,12
	copy_round_keys esp,ecx,13
	copy_round_keys esp,ecx,14
	mov ecx,esp

align 16
lp256decfour_CBC:

	test eax,eax
	jz end_dec256_CBC

	cmp eax,4
	jl	lp256decsingle_CBC

	load_and_xor4 esi, [ecx+14*16]
	add esi,16*4
	aesdec4 [ecx+13*16]
	aesdec4 [ecx+12*16]
	aesdec4 [ecx+11*16]
	aesdec4 [ecx+10*16]
	aesdec4 [ecx+9*16]
	aesdec4 [ecx+8*16]
	aesdec4 [ecx+7*16]
	aesdec4 [ecx+6*16]
	aesdec4 [ecx+5*16]
	aesdec4 [ecx+4*16]
	aesdec4 [ecx+3*16]
	aesdec4 [ecx+2*16]
	aesdec4 [ecx+1*16]
	aesdeclast4 [ecx+0*16]

	pxor	xmm0,xmm5
	movdqu	xmm4,[esi- 16*4 + 0*16]
	pxor	xmm1,xmm4
	movdqu	xmm4,[esi- 16*4 + 1*16]
	pxor	xmm2,xmm4
	movdqu	xmm4,[esi- 16*4 + 2*16]
	pxor	xmm3,xmm4
	movdqu	xmm5,[esi- 16*4 + 3*16]

	sub eax,4
	store4 esi+edi-(16*4)
	jmp lp256decfour_CBC


	align 16
lp256decsingle_CBC:

	movdqu xmm0, [esi]
	movdqa xmm1,xmm0
	movdqu xmm4, [ecx+14*16]
	pxor xmm0, xmm4
	aesdec1_u  [ecx+13*16]
	aesdec1_u  [ecx+12*16]
	aesdec1_u  [ecx+11*16]
	aesdec1_u  [ecx+10*16]
	aesdec1_u  [ecx+9*16]
	aesdec1_u  [ecx+8*16]
	aesdec1_u  [ecx+7*16]
	aesdec1_u  [ecx+6*16]
	aesdec1_u  [ecx+5*16]
	aesdec1_u  [ecx+4*16]
	aesdec1_u  [ecx+3*16]
	aesdec1_u  [ecx+2*16]
	aesdec1_u  [ecx+1*16]
	aesdeclast1_u  [ecx+0*16]

	pxor	xmm0,xmm5
	movdqa	xmm5,xmm1

	add esi, 16
	movdqu  [edi+esi - 16], xmm0
	dec eax
	jnz lp256decsingle_CBC

end_dec256_CBC:


	mov esp,ebp
	pop ebp
	pop edi
	pop esi

	mov ecx,[esp-4+8]  ; first arg
	mov ecx,[ecx+12]
	movdqu	[ecx],xmm5 ; store last iv for chaining

	ret



align 16
global _iEnc128_CBC
_iEnc128_CBC:
	mov ecx,[esp-4+8]

	push esi
	push edi
	push ebp
	mov ebp,esp

	sub esp,16*16
	and esp,0xfffffff0

	mov	eax,[ecx+12]
	movdqu xmm1,[eax]	;iv

	mov eax,[ecx+16] ; numblocks
	mov esi,[ecx]
	mov edi,[ecx+4]
	mov ecx,[ecx+8]
	sub edi,esi

	test	ecx,0xf
	jz		lp128encsingle_CBC

	copy_round_keys esp,ecx,0
	copy_round_keys esp,ecx,1
	copy_round_keys esp,ecx,2
	copy_round_keys esp,ecx,3
	copy_round_keys esp,ecx,4
	copy_round_keys esp,ecx,5
	copy_round_keys esp,ecx,6
	copy_round_keys esp,ecx,7
	copy_round_keys esp,ecx,8
	copy_round_keys esp,ecx,9
	copy_round_keys esp,ecx,10
	mov ecx,esp

	align 16

lp128encsingle_CBC:

	movdqu xmm0, [esi]
	add esi, 16
	pxor xmm0, xmm1
	movdqu xmm4,[ecx+0*16]
	pxor xmm0, xmm4
	aesenc1  [ecx+1*16]
	aesenc1  [ecx+2*16]
	aesenc1  [ecx+3*16]
	aesenc1  [ecx+4*16]
	aesenc1  [ecx+5*16]
	aesenc1  [ecx+6*16]
	aesenc1  [ecx+7*16]
	aesenc1  [ecx+8*16]
	aesenc1  [ecx+9*16]
	aesenclast1  [ecx+10*16]
		; Store output encrypted data into CIPHERTEXT array
	movdqu  [esi+edi-16], xmm0
	movdqa xmm1,xmm0
	dec eax
	jnz lp128encsingle_CBC


	mov esp,ebp
	pop ebp
	pop edi
	pop esi
	mov ecx,[esp-4+8]  ; first arg
	mov ecx,[ecx+12]
	movdqu	[ecx],xmm1 ; store last iv for chaining

	ret



align 16
global _iEnc256_CBC
_iEnc256_CBC:
	mov ecx,[esp-4+8]  ; first arg

	push esi
	push edi
	push ebp
	mov ebp,esp

	sub esp,16*16
	and esp,0xfffffff0

	mov	eax,[ecx+12]
	movdqu xmm1,[eax]	;iv

	mov eax,[ecx+16] ; numblocks
	mov esi,[ecx]
	mov edi,[ecx+4]
	mov ecx,[ecx+8]
	sub edi,esi

	test	ecx,0xf
	jz		lp256encsingle_CBC

	copy_round_keys esp,ecx,0
	copy_round_keys esp,ecx,1
	copy_round_keys esp,ecx,2
	copy_round_keys esp,ecx,3
	copy_round_keys esp,ecx,4
	copy_round_keys esp,ecx,5
	copy_round_keys esp,ecx,6
	copy_round_keys esp,ecx,7
	copy_round_keys esp,ecx,8
	copy_round_keys esp,ecx,9
	copy_round_keys esp,ecx,10
	copy_round_keys esp,ecx,11
	copy_round_keys esp,ecx,12
	copy_round_keys esp,ecx,13
	copy_round_keys esp,ecx,14
	mov ecx,esp

	align 16

lp256encsingle_CBC:

;abab
	movdqu xmm0, [esi]
	add esi, 16
	pxor xmm0, xmm1
	movdqu xmm4,[ecx+0*16]
	pxor xmm0, xmm4
	aesenc1 [ecx+1*16]
	aesenc1 [ecx+2*16]
	aesenc1 [ecx+3*16]
	aesenc1 [ecx+4*16]
	aesenc1 [ecx+5*16]
	aesenc1 [ecx+6*16]
	aesenc1 [ecx+7*16]
	aesenc1 [ecx+8*16]
	aesenc1 [ecx+9*16]
	aesenc1 [ecx+10*16]
	aesenc1 [ecx+11*16]
	aesenc1 [ecx+12*16]
	aesenc1 [ecx+13*16]
	aesenclast1 [ecx+14*16]
		; Store output encrypted data into CIPHERTEXT array
	movdqu  [esi+edi-16], xmm0
	movdqa xmm1,xmm0
	dec eax
	jnz lp256encsingle_CBC


	mov esp,ebp
	pop ebp
	pop edi
	pop esi
	mov ecx,[esp-4+8]
	mov ecx,[ecx+12]
	movdqu	[ecx],xmm1 ; store last iv for chaining

	ret

; Mark this file as not needing an executable stack.
%ifidn __OUTPUT_FORMAT__,elf
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
%ifidn __OUTPUT_FORMAT__,elf32
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
%ifidn __OUTPUT_FORMAT__,elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
