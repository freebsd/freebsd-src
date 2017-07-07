
; -------------------------------------------------------------------------
; Copyright (c) 2001, Dr Brian Gladman <brg@gladman.uk.net>, Worcester, UK.
; All rights reserved.
;
; LICENSE TERMS
;
; The free distribution and use of this software in both source and binary 
; form is allowed (with or without changes) provided that:
;
;   1. distributions of this source code include the above copyright 
;      notice, this list of conditions and the following disclaimer;
;
;   2. distributions in binary form include the above copyright
;      notice, this list of conditions and the following disclaimer
;      in the documentation and/or other associated materials;
;
;   3. the copyright holder's name is not used to endorse products 
;      built using this software without specific written permission.
;
; DISCLAIMER
;
; This software is provided 'as is' with no explcit or implied warranties
; in respect of any properties, including, but not limited to, correctness 
; and fitness for purpose.
; -------------------------------------------------------------------------
; Issue Date: 15/01/2002

; An AES (Rijndael) implementation for the Pentium MMX family using the NASM
; assembler <http://www.web-sites.co.uk/nasm/>. This version only implements
; the standard AES block length (128 bits, 16 bytes) with the same interface
; as that used in my C/C++ implementation.   This code does not preserve the
; eax, ecx or edx registers or the artihmetic status flags. However, the ebx, 
; esi, edi, and ebp registers are preserved across calls.    Only encryption
; and decryption are implemented here, the key schedule code being that from
; compiling aes.c with USE_ASM defined.  This code uses VC++ register saving
; conentions; if it is used with another compiler, its conventions for using
; and saving registers will need to be checked.

    section .text use32

; aes_rval aes_enc_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1]);
; aes_rval aes_dec_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1]);

    global  _aes_enc_blk
    global  _aes_dec_blk
    
    extern  _ft_tab
    extern  _fl_tab
    extern  _it_tab
    extern  _il_tab

;%define USE_MMX   ; include this to use MMX registers for temporary storage
;%define USE_EMMS    ; include this if you make use of floating point operations

%ifdef  USE_MMX
%ifdef  USE_EMMS
%define EMMS_ON
%endif
%endif

tlen:   equ  1024   ; length of each of 4 'xor' arrays (256 32-bit words)

; offsets to parameters with one register pushed onto stack

in_blk: equ     8   ; input byte array address parameter
out_blk:equ    12   ; output byte array address parameter
ctx:    equ    16   ; AES context structure

; offsets in context structure

ksch:   equ     0   ; encryption key schedule base address
nrnd:   equ   256   ; number of rounds
nblk:   equ   260   ; number of rounds

; register mapping for encrypt and decrypt subroutines

%define r0  eax
%define r1  ebx
%define r2  ecx
%define r3  edx
%define r4  esi
%define r5  edi
%define r6  ebp

%define eaxl  al
%define eaxh  ah
%define ebxl  bl
%define ebxh  bh
%define ecxl  cl
%define ecxh  ch
%define edxl  dl
%define edxh  dh

; This macro takes a 32-bit word representing a column and uses
; each of its four bytes to index into four tables of 256 32-bit
; words to obtain values that are then xored into the appropriate
; output registers r0, r1, r4 or r5.  

; Parameters:
;   %1  out_state[0]
;   %2  out_state[1]
;   %3  out_state[2]
;   %4  out_state[3]
;   %5  table base address
;   %6  input register for the round (destroyed)
;   %7  scratch register for the round

%macro do_col 7

    movzx   %7,%6l
    xor     %1,[4*%7+%5]
    movzx   %7,%6h
    shr     %6,16
    xor     %2,[4*%7+%5+tlen]
    movzx   %7,%6l
    movzx   %6,%6h
    xor     %3,[4*%7+%5+2*tlen] 
    xor     %4,[4*%6+%5+3*tlen]

%endmacro

; initialise output registers from the key schedule

%macro do_fcol 8

    mov     %1,[%8]
    movzx   %7,%6l
    mov     %2,[%8+12]
    xor     %1,[4*%7+%5]
    mov     %4,[%8+ 4]
    movzx   %7,%6h
    shr     %6,16
    xor     %2,[4*%7+%5+tlen]
    movzx   %7,%6l
    movzx   %6,%6h
    xor     %4,[4*%6+%5+3*tlen]
    mov     %6,%3
    mov     %3,[%8+ 8]
    xor     %3,[4*%7+%5+2*tlen] 

%endmacro

; initialise output registers from the key schedule

%macro do_icol 8

    mov     %1,[%8]
    movzx   %7,%6l
    mov     %2,[%8+ 4]
    xor     %1,[4*%7+%5]
    mov     %4,[%8+12]
    movzx   %7,%6h
    shr     %6,16
    xor     %2,[4*%7+%5+tlen]
    movzx   %7,%6l
    movzx   %6,%6h
    xor     %4,[4*%6+%5+3*tlen]
    mov     %6,%3
    mov     %3,[%8+ 8]
    xor     %3,[4*%7+%5+2*tlen] 

%endmacro

; These macros implement either MMX or stack based local variables

%ifdef  USE_MMX

%macro  save 2
    movd    mm%1,%2
%endmacro

%macro  restore 2
    movd    %1,mm%2
%endmacro

%else

%macro  save 2
    mov     [esp+4*%1],%2
%endmacro

%macro  restore 2
    mov     %1,[esp+4*%2]
%endmacro

%endif

; This macro performs a forward encryption cycle. It is entered with
; the first previous round column values in r0, r1, r4 and r5 and
; exits with the final values in the same registers, using the MMX
; registers mm0-mm1 for temporary storage

%macro fwd_rnd 1-2 _ft_tab

; mov current column values into the MMX registers

    mov     r2,r0
    save    0,r1
    save    1,r5

; compute new column values

    do_fcol r0,r5,r4,r1, %2, r2,r3, %1
    do_col  r4,r1,r0,r5, %2, r2,r3
    restore r2,0
    do_col  r1,r0,r5,r4, %2, r2,r3
    restore r2,1
    do_col  r5,r4,r1,r0, %2, r2,r3

%endmacro

; This macro performs an inverse encryption cycle. It is entered with
; the first previous round column values in r0, r1, r4 and r5 and
; exits with the final values in the same registers, using the MMX
; registers mm0-mm1 for temporary storage

%macro inv_rnd 1-2 _it_tab

; mov current column values into the MMX registers

    mov     r2,r0
    save    0,r1
    save    1,r5

; compute new column values

    do_icol r0,r1,r4,r5, %2, r2,r3, %1
    do_col  r4,r5,r0,r1, %2, r2,r3
    restore r2,0
    do_col  r1,r4,r5,r0, %2, r2,r3
    restore r2,1
    do_col  r5,r0,r1,r4, %2, r2,r3

%endmacro

; AES (Rijndael) Encryption Subroutine

_aes_enc_blk:
    push    ebp
    mov     ebp,[esp+ctx]       ; pointer to context
    xor     eax,eax
    test    [ebp+nblk],byte 1
    je      .0
    cmp     eax,[ebp+nrnd]      ; encryption/decryption flags
    jne     short .1
.0: pop     ebp
    ret            

; CAUTION: the order and the values used in these assigns 
; rely on the register mappings

.1: push    ebx
    mov     r2,[esp+in_blk+4]
    push    esi
    mov     r3,[ebp+nrnd]   ; number of rounds
    push    edi
    lea     r6,[ebp+ksch]   ; key pointer

; input four columns and xor in first round key

    mov     r0,[r2]
    mov     r1,[r2+4]
    mov     r4,[r2+8]
    mov     r5,[r2+12]
    xor     r0,[r6]
    xor     r1,[r6+4]
    xor     r4,[r6+8]
    xor     r5,[r6+12]

%ifndef USE_MMX
    sub     esp,8           ; space for register saves on stack
%endif
    add     r6,16           ; increment to next round key   
    sub     r3,10          
    je      .4              ; 10 rounds for 128-bit key
    add     r6,32  
    sub     r3,2
    je      .3              ; 12 rounds for 128-bit key
    add     r6,32  

.2: fwd_rnd r6-64           ; 14 rounds for 128-bit key
    fwd_rnd r6-48  
.3: fwd_rnd r6-32           ; 12 rounds for 128-bit key
    fwd_rnd r6-16  
.4: fwd_rnd r6              ; 10 rounds for 128-bit key
    fwd_rnd r6+ 16 
    fwd_rnd r6+ 32
    fwd_rnd r6+ 48
    fwd_rnd r6+ 64
    fwd_rnd r6+ 80
    fwd_rnd r6+ 96
    fwd_rnd r6+112
    fwd_rnd r6+128
    fwd_rnd r6+144,_fl_tab  ; last round uses a different table

; move final values to the output array.  CAUTION: the 
; order of these assigns rely on the register mappings

%ifndef USE_MMX
    add     esp,8
%endif
    mov     r6,[esp+out_blk+12]
    mov     [r6+12],r5
    pop     edi
    mov     [r6+8],r4
    pop     esi
    mov     [r6+4],r1
    pop     ebx
    mov     [r6],r0
    pop     ebp
    mov     eax,1
%ifdef  EMMS_ON
    emms
%endif
    ret

; AES (Rijndael) Decryption Subroutine

_aes_dec_blk:
    push    ebp
    mov     ebp,[esp+ctx]       ; pointer to context
    xor     eax,eax
    test    [ebp+nblk],byte 2
    je      .0
    cmp     eax,[ebp+nrnd]      ; encryption/decryption flags
    jne     short .1
.0: pop     ebp
    ret            

; CAUTION: the order and the values used in these assigns 
; rely on the register mappings

.1: push    ebx
    mov     r2,[esp+in_blk+4]
    push    esi
    mov     r3,[ebp+nrnd]   ; number of rounds
    push    edi
    lea     r6,[ebp+ksch]   ; key pointer
    mov     r0,r3
    shl     r0,4
    add     r6,r0
    
; input four columns and xor in first round key

    mov     r0,[r2]
    mov     r1,[r2+4]
    mov     r4,[r2+8]
    mov     r5,[r2+12]
    xor     r0,[r6]
    xor     r1,[r6+4]
    xor     r4,[r6+8]
    xor     r5,[r6+12]

%ifndef USE_MMX
    sub     esp,8           ; space for register saves on stack
%endif
    sub     r6,16           ; increment to next round key   
    sub     r3,10          
    je      .4              ; 10 rounds for 128-bit key
    sub     r6,32  
    sub     r3,2
    je      .3              ; 12 rounds for 128-bit key
    sub     r6,32  

.2: inv_rnd r6+64           ; 14 rounds for 128-bit key 
    inv_rnd r6+48  
.3: inv_rnd r6+32           ; 12 rounds for 128-bit key
    inv_rnd r6+16  
.4: inv_rnd r6              ; 10 rounds for 128-bit key
    inv_rnd r6- 16 
    inv_rnd r6- 32
    inv_rnd r6- 48
    inv_rnd r6- 64
    inv_rnd r6- 80
    inv_rnd r6- 96
    inv_rnd r6-112
    inv_rnd r6-128
    inv_rnd r6-144,_il_tab  ; last round uses a different table

; move final values to the output array.  CAUTION: the 
; order of these assigns rely on the register mappings

%ifndef USE_MMX
    add     esp,8
%endif
    mov     r6,[esp+out_blk+12]
    mov     [r6+12],r5
    pop     edi
    mov     [r6+8],r4
    pop     esi
    mov     [r6+4],r1
    pop     ebx
    mov     [r6],r0
    pop     ebp
    mov     eax,1
%ifdef  EMMS_ON
    emms
%endif
    ret

    end
