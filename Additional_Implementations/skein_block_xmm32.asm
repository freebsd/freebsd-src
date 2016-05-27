;
;----------------------------------------------------------------
; 32-bit x86 assembler code for Skein block functions using XMM registers
;
; Author: Doug Whiting, Hifn
;
; This code is released to the public domain.
;----------------------------------------------------------------
;
    .386p
    .model flat
    .code
    .xmm                                    ;enable XMM instructions
;
_MASK_ALL_  equ (256+512+1024)              ;all three algorithm bits
;
;;;;;;;;;;;;;;;;;
ifndef SKEIN_USE_ASM
_USE_ASM_        = _MASK_ALL_
elseif SKEIN_USE_ASM and _MASK_ALL_
_USE_ASM_        = SKEIN_USE_ASM
else
_USE_ASM_        = _MASK_ALL_
endif
;
;;;;;;;;;;;;;;;;;
ifndef SKEIN_LOOP  
_SKEIN_LOOP       = 0                       ;default is all fully unrolled
else
_SKEIN_LOOP       = SKEIN_LOOP
endif
;--------------
; the unroll counts (0 --> fully unrolled)
SKEIN_UNROLL_256  = (_SKEIN_LOOP / 100) mod 10
SKEIN_UNROLL_512  = (_SKEIN_LOOP /  10) mod 10
SKEIN_UNROLL_1024 = (_SKEIN_LOOP      ) mod 10
;
SKEIN_ASM_UNROLL  = 0
  irp _NN_,<256,512,1024>
    if (SKEIN_UNROLL_&_NN_) eq 0
SKEIN_ASM_UNROLL  = SKEIN_ASM_UNROLL + _NN_
    endif
  endm
;
;;;;;;;;;;;;;;;;;
;
ifndef SKEIN_ROUNDS
ROUNDS_256  =   72
ROUNDS_512  =   72
ROUNDS_1024 =   80
else
ROUNDS_256  = 8*((((SKEIN_ROUNDS / 100) + 5) mod 10) + 5)
ROUNDS_512  = 8*((((SKEIN_ROUNDS /  10) + 5) mod 10) + 5)
ROUNDS_1024 = 8*((((SKEIN_ROUNDS      ) + 5) mod 10) + 5)
endif
irp _NN_,<256,512,1024>
  if _USE_ASM_ and _NN_
    irp _RR_,<%(ROUNDS_&_NN_)>
      if _NN_ eq 1024
%out  +++ SKEIN_ROUNDS_&_NN_ = _RR_
      else
%out  +++ SKEIN_ROUNDS_&_NN_  = _RR_
      endif
    endm
  endif
endm
;;;;;;;;;;;;;;;;;
;
ifdef SKEIN_CODE_SIZE
_SKEIN_CODE_SIZE equ (1)
else
ifdef  SKEIN_PERF                           ;use code size if SKEIN_PERF is defined
_SKEIN_CODE_SIZE equ (1)
endif
endif
;
;;;;;;;;;;;;;;;;;
;
ifndef SKEIN_DEBUG
_SKEIN_DEBUG      = 0
else
_SKEIN_DEBUG      = 1
endif
;;;;;;;;;;;;;;;;;
;
; define offsets of fields in hash context structure
;
HASH_BITS   =   0                           ;# bits of hash output
BCNT        =   4 + HASH_BITS               ;number of bytes in BUFFER[]
TWEAK       =   4 + BCNT                    ;tweak values[0..1]
X_VARS      =  16 + TWEAK                   ;chaining vars
;
;(Note: buffer[] in context structure is NOT needed here :-)
;
KW_PARITY_LO=   0A9FC1A22h                  ;overall parity of key schedule words (hi32/lo32)
KW_PARITY_HI=   01BD11BDAh
FIRST_MASK8 =   NOT (1 SHL 6)               ;FIRST block flag bit
;
; rotation constants for Skein
;
RC_256_0_0  = 14
RC_256_0_1  = 16

RC_256_1_0  = 52
RC_256_1_1  = 57

RC_256_2_0  = 23
RC_256_2_1  = 40

RC_256_3_0  =  5
RC_256_3_1  = 37

RC_256_4_0  = 25
RC_256_4_1  = 33

RC_256_5_0  = 46
RC_256_5_1  = 12

RC_256_6_0  = 58
RC_256_6_1  = 22

RC_256_7_0  = 32
RC_256_7_1  = 32

RC_512_0_0  = 46
RC_512_0_1  = 36
RC_512_0_2  = 19
RC_512_0_3  = 37

RC_512_1_0  = 33
RC_512_1_1  = 27
RC_512_1_2  = 14
RC_512_1_3  = 42

RC_512_2_0  = 17
RC_512_2_1  = 49
RC_512_2_2  = 36
RC_512_2_3  = 39

RC_512_3_0  = 44
RC_512_3_1  =  9
RC_512_3_2  = 54
RC_512_3_3  = 56

RC_512_4_0  = 39
RC_512_4_1  = 30
RC_512_4_2  = 34
RC_512_4_3  = 24

RC_512_5_0  = 13
RC_512_5_1  = 50
RC_512_5_2  = 10
RC_512_5_3  = 17

RC_512_6_0  = 25
RC_512_6_1  = 29
RC_512_6_2  = 39
RC_512_6_3  = 43

RC_512_7_0  =  8
RC_512_7_1  = 35
RC_512_7_2  = 56
RC_512_7_3  = 22

RC_1024_0_0 = 24
RC_1024_0_1 = 13
RC_1024_0_2 =  8
RC_1024_0_3 = 47
RC_1024_0_4 =  8
RC_1024_0_5 = 17
RC_1024_0_6 = 22
RC_1024_0_7 = 37

RC_1024_1_0 = 38
RC_1024_1_1 = 19
RC_1024_1_2 = 10
RC_1024_1_3 = 55
RC_1024_1_4 = 49
RC_1024_1_5 = 18
RC_1024_1_6 = 23
RC_1024_1_7 = 52

RC_1024_2_0 = 33
RC_1024_2_1 =  4
RC_1024_2_2 = 51
RC_1024_2_3 = 13
RC_1024_2_4 = 34
RC_1024_2_5 = 41
RC_1024_2_6 = 59
RC_1024_2_7 = 17

RC_1024_3_0 =  5
RC_1024_3_1 = 20
RC_1024_3_2 = 48
RC_1024_3_3 = 41
RC_1024_3_4 = 47
RC_1024_3_5 = 28
RC_1024_3_6 = 16
RC_1024_3_7 = 25

RC_1024_4_0 = 41
RC_1024_4_1 =  9
RC_1024_4_2 = 37
RC_1024_4_3 = 31
RC_1024_4_4 = 12
RC_1024_4_5 = 47
RC_1024_4_6 = 44
RC_1024_4_7 = 30

RC_1024_5_0 = 16
RC_1024_5_1 = 34
RC_1024_5_2 = 56
RC_1024_5_3 = 51
RC_1024_5_4 =  4
RC_1024_5_5 = 53
RC_1024_5_6 = 42
RC_1024_5_7 = 41

RC_1024_6_0 = 31
RC_1024_6_1 = 44
RC_1024_6_2 = 47
RC_1024_6_3 = 46
RC_1024_6_4 = 19
RC_1024_6_5 = 42
RC_1024_6_6 = 44
RC_1024_6_7 = 25

RC_1024_7_0 =  9
RC_1024_7_1 = 48
RC_1024_7_2 = 35
RC_1024_7_3 = 52
RC_1024_7_4 = 23
RC_1024_7_5 = 31
RC_1024_7_6 = 37
RC_1024_7_7 = 20
;
mov64 macro x0,x1
    movq    x0,x1
endm
;
;----------------------------------------------------------------
; declare allocated space on the stack
StackVar    macro localName,localSize
localName   =   _STK_OFFS_
_STK_OFFS_  =   _STK_OFFS_+(localSize)
endm ;StackVar
;
;----------------------------------------------------------------
;
; MACRO: Configure stack frame, allocate local vars
;
Setup_Stack macro WCNT,RND_CNT
_STK_OFFS_  =   0                   ;starting offset from esp, forced on 16-byte alignment
    ;----- local  variables         ;<-- esp
    StackVar    X_stk  , 8*(WCNT)   ;local context vars
    StackVar    Wcopy  , 8*(WCNT)   ;copy of input block    
    StackVar    ksTwk  ,16*3        ;key schedule: tweak words
    StackVar    ksKey  ,16*(WCNT)+16;key schedule: key   words
FRAME_OFFS  =   ksTwk+128           ;<-- ebp
  if (SKEIN_ASM_UNROLL and (WCNT*64)) eq 0
    StackVar    ksRot,16*(RND_CNT/4);leave space for ks "rotation" to happen
  endif
LOCAL_SIZE  =   _STK_OFFS_          ;size of local vars
    ;
    ;"restart" the stack defns, because we relocate esp to guarantee alignment
    ;    (i.e., these vars are NOT at fixed offsets from esp)
_STK_OFFS_  =   0
    ;----- 
    StackVar    savRegs,8*4         ;pushad data
    StackVar    retAddr,4           ;return address
    ;----- caller parameters
    StackVar    ctxPtr ,4           ;context ptr
    StackVar    blkPtr ,4           ;pointer to block data
    StackVar    blkCnt ,4           ;number of full blocks to process
    StackVar    bitAdd ,4           ;bit count to add to tweak
    ;----- caller's stack frame
;
; Notes on stack frame setup:
;   * the most used variable (except for Skein-256) is X_stk[], based at [esp+0]
;   * the next most used is the key schedule words
;       so ebp is "centered" there, allowing short offsets to the key/tweak
;       schedule in 256/512-bit Skein cases, but not posible for Skein-1024 :-(
;   * the Wcopy variables are infrequently accessed, and they have long 
;       offsets from both esp and ebp only in the 1024-bit case.
;   * all other local vars and calling parameters can be accessed 
;       with short offsets, except in the 1024-bit case
;
    pushad                          ;save all regs
    mov     ebx,esp                 ;keep ebx as pointer to caller parms
    sub     esp,LOCAL_SIZE          ;make room for the locals
    and     esp,not 15              ;force alignment
    mov     edi,[ebx+ctxPtr ]       ;edi --> Skein context
    lea     ebp,[esp+FRAME_OFFS]    ;maximize use of short offsets from ebp
    mov     ecx,ptr32 [ebx+blkCnt]  ;keep block cnt in ecx
;
endm ;Setup_Stack
;
FP_         equ <ebp-FRAME_OFFS>    ;keep as many short offsets as possible
SI_         equ <esi-FRAME_OFFS>    ;keep as many short offsets as possible
ptr64       equ <qword ptr>         ;useful abbreviations
ptr32       equ <dword ptr>
ptr08       equ <byte  ptr>
;
;----------------------------------------------------------------
;
Reset_Stack macro   procStart
    mov     esp,ebx                 ;get rid of locals (wipe??)
    popad                           ;restore all regs

    ;display code size in bytes to stdout
  irp  _BCNT_,<%($+1-procStart)>    ;account for return opcode
if     _BCNT_ ge 10000              ;(align it all pretty)
%out procStart code size = _BCNT_ bytes  
elseif _BCNT_ ge  1000
%out procStart code size =  _BCNT_ bytes  
else
%out procStart code size =   _BCNT_ bytes  
endif
  endm ;irp _BCNT_

endm ; Reset_Stack
;
;----------------------------------------------------------------
; macros to help debug internals
;
if _SKEIN_DEBUG
    extrn   _Skein_Show_Block:near   ;calls to C routines
    extrn   _Skein_Show_Round:near
;
SKEIN_RND_SPECIAL       =   1000
SKEIN_RND_KEY_INITIAL   =   SKEIN_RND_SPECIAL+0
SKEIN_RND_KEY_INJECT    =   SKEIN_RND_SPECIAL+1
SKEIN_RND_FEED_FWD      =   SKEIN_RND_SPECIAL+2
;
Skein_Debug_Block macro BLK_BITS
;
;void Skein_Show_Block(uint_t bits,const Skein_Ctxt_Hdr_t *h,const u64b_t *X,
;                     const u08b_t *blkPtr, const u64b_t *wPtr, 
;                     const u64b_t *ksPtr,const u64b_t *tsPtr);
;
    Put_XMM_&BLK_BITS
    pushad                          ;save all regs
    lea     eax,[FP_+ksTwk+1]       ;+1 = flag: "stride" size = 2 qwords
    lea     esi,[FP_+ksKey+1]
    lea     ecx,[esp+32+Wcopy]      ;adjust offset by 32 for pushad
    mov     edx,[ebx+ctxPtr]        ;ctx_hdr_ptr
    lea     edx,[edx+X_VARS]        ;edx ==> cxt->X[]
    push    eax                     ;tsPtr
    push    esi                     ;ksPtr
    push    ecx                     ;wPtr
    push    ptr32 [ebx+blkPtr]      ;blkPtr
    push    edx                     ;ctx->Xptr
    push    ptr32 [ebx+ctxPtr]      ;ctx_hdr_ptr
    mov     eax,BLK_BITS
    push    eax                     ;bits
  ifdef _MINGW_
    call    _Skein_Show_Block-4     ;strange linkage??
  else
    call    _Skein_Show_Block
  endif
    add     esp,7*4                 ;discard parameter space on stack
    popad                           ;restore regs
;
    Get_XMM_&BLK_BITS
endm ;Skein_Debug_Block

;
Skein_Debug_Round macro BLK_BITS,R,saveRegs
;
;void Skein_Show_Round(uint_t bits,const Skein_Ctxt_Hdr_t *h,int r,const u64b_t *X);
;
  ifnb <saveRegs>
      Put_XMM_&BLK_BITS
  endif
    pushad                          ;save all regs
  if R ne SKEIN_RND_FEED_FWD
    lea     eax,[esp+32+X_stk]      ;adjust offset by 32 for pushad
  else
    mov     eax,[ebx+ctxPtr]
    add     eax,X_VARS
  endif
    push    eax                     ;Xptr
  if (SKEIN_ASM_UNROLL and BLK_BITS) or (R ge SKEIN_RND_SPECIAL)
    mov     eax,R
  else
    lea     eax,[4*edx+1+(((R)-1) and 3)] ;compute round number using edx
  endif
    push    eax                     ;round number
    push    ptr32 [ebx+ctxPtr]      ;ctx_hdr_ptr
    mov     eax,BLK_BITS
    push    eax                     ;bits
  ifdef _MINGW_
    call    _Skein_Show_Round-4     ;strange linkage??
  else
    call    _Skein_Show_Round
  endif
    add     esp,4*4                 ;discard parameter space on stack
    popad                           ;restore regs

  ifnb <saveRegs>
      Get_XMM_&BLK_BITS       ;save internal vars for debug dump
  endif
endm  ;Skein_Debug_Round
endif ;ifdef SKEIN_DEBUG
;
;----------------------------------------------------------------
; useful macros
_ldX macro xn
  ifnb <xn>
    mov64 xmm&xn,ptr64 [esp+X_stk+8*xn]
  endif
endm

_stX macro xn
  ifnb <xn>
    mov64        ptr64 [esp+X_stk+8*xn],xmm&xn
  endif
endm
;
;----------------------------------------------------------------
;
if _USE_ASM_ and 256
    public      _Skein_256_Process_Block
;
; void Skein_256_Process_Block(Skein_256_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd);
;
;;;;;;;;;;;;;;;;;
;
; Skein-256 round macros
;
R_256_OneRound macro _RR_,x0,x1,x2,x3,t0,t1
  irp _qq_,<%((_RR_) and 7)>        ;figure out which rotation constants to use
    if x0 eq 0
_RC0_ =   RC_256_&_qq_&_0
_RC1_ =   RC_256_&_qq_&_1
    else
_RC0_ =   RC_256_&_qq_&_1
_RC1_ =   RC_256_&_qq_&_0
    endif
  endm
;
    paddq   xmm&x0,xmm&x1
    mov64   xmm&t0,xmm&x1
    psllq   xmm&x1,   _RC0_
    psrlq   xmm&t0,64-_RC0_
    xorpd   xmm&x1,xmm&x0
    xorpd   xmm&x1,xmm&t0
;
    paddq   xmm&x2,xmm&x3
    mov64   xmm&t1,xmm&x3
    psllq   xmm&x3,   _RC1_
    psrlq   xmm&t1,64-_RC1_
    xorpd   xmm&x3,xmm&x2
    xorpd   xmm&x3,xmm&t1
  if _SKEIN_DEBUG
    Skein_Debug_Round 256,%(_RR_+1),saveRegs
  endif
endm ;R_256_OneRound
;
R_256_FourRounds macro _RN_
    R_256_OneRound (_RN_+0),0,1,2,3,4,5
    R_256_OneRound (_RN_+1),2,1,0,3,4,5

    R_256_OneRound (_RN_+2),0,1,2,3,4,5
    R_256_OneRound (_RN_+3),2,1,0,3,4,5

    ;inject key schedule
    inc   edx                     ;bump round number
    movd  xmm4,edx
  if _UNROLL_CNT eq (ROUNDS_256/8)
    ;fully unrolled version
_RK_ = ((_RN_)/4)                 ;key injection counter
    paddq xmm0,[FP_+ksKey+16*((_RK_+1) mod 5)]
    paddq xmm1,[FP_+ksKey+16*((_RK_+2) mod 5)]
    paddq xmm2,[FP_+ksKey+16*((_RK_+3) mod 5)]
    paddq xmm3,[FP_+ksKey+16*((_RK_+4) mod 5)]
    paddq xmm1,[FP_+ksTwk+16*((_RK_+1) mod 3)]
    paddq xmm2,[FP_+ksTwk+16*((_RK_+2) mod 3)]
    paddq xmm3,xmm4
  else ;looping version
    paddq xmm0,[SI_+ksKey+16*1]
    paddq xmm1,[SI_+ksKey+16*2]
    paddq xmm2,[SI_+ksKey+16*3]
    paddq xmm3,[SI_+ksKey+16*4]
    paddq xmm1,[SI_+ksTwk+16*1]
    paddq xmm2,[SI_+ksTwk+16*2]
    paddq xmm3,xmm4
;   
    mov64 xmm4,<ptr64 [SI_+ksKey]>;first, "rotate" key schedule on the stack
    mov64 xmm5,<ptr64 [SI_+ksTwk]>;    (for next time through)
    mov64      <ptr64 [SI_+ksKey+16*(WCNT+1)]>,xmm4
    mov64      <ptr64 [SI_+ksTwk+16*3]>,xmm5
    add   esi,16                  ;bump rolling pointer
  endif
  if _SKEIN_DEBUG
      Skein_Debug_Round 256,SKEIN_RND_KEY_INJECT,saveRegs
  endif
endm ;R256_FourRounds
;
if _SKEIN_DEBUG ; macros for saving/restoring X_stk for debug routines
Put_XMM_256 equ <call _Put_XMM_256>
Get_XMM_256 equ <call _Get_XMM_256>

_Put_XMM_256:
  irp _NN_,<0,1,2,3>
    mov64           ptr64 [esp+X_stk+4+_NN_*8],xmm&_NN_
  endm
    ret
;
_Get_XMM_256:
  irp _NN_,<0,1,2,3>
    mov64  xmm&_NN_,ptr64 [esp+X_stk+4+_NN_*8]
  endm
    ret
endif
;
;;;;;;;;;;;;;;;;;
;
; code
;
_Skein_256_Process_Block proc near
    WCNT    =   4                   ;WCNT=4 for Skein-256
    Setup_Stack WCNT,ROUNDS_256
    ; main hash loop for Skein_256
Skein_256_block_loop:
    movd    xmm4,ptr32 [ebx+bitAdd]
    mov64   xmm5,ptr64 [edi+TWEAK+0]
    mov64   xmm6,ptr64 [edi+TWEAK+8]
    paddq   xmm5,xmm4               ;bump T0 by the bitAdd parameter
    mov64   ptr64 [edi+TWEAK],xmm5  ;save updated tweak value T0 (for next time)
    movapd  xmm7,xmm6
    xorpd   xmm7,xmm5               ;compute overall tweak parity
    movdqa  [FP_+ksTwk   ],xmm5     ;save the expanded tweak schedule on the stack
    movdqa  [FP_+ksTwk+16],xmm6
    movdqa  [FP_+ksTwk+32],xmm7

    mov     esi,[ebx+blkPtr]        ;esi --> input block
    mov     eax,KW_PARITY_LO        ;init key schedule parity accumulator
    mov     edx,KW_PARITY_HI
    movd    xmm4,eax
    movd    xmm0,edx
    unpcklps xmm4,xmm0              ;pack two 32-bit words into xmm4
;
  irp _NN_,<0,1,2,3>                ;copy in the chaining vars
    mov64   xmm&_NN_,ptr64 [edi+X_VARS+8*_NN_]
    xorpd   xmm4,xmm&_NN_           ;update overall parity
    movdqa  [FP_+ksKey+16*_NN_],xmm&_NN_
  endm
    movdqa  [FP_+ksKey+16*WCNT],xmm4;save overall parity at the end of the array
;
    paddq   xmm1,xmm5               ;inject the initial tweak words
    paddq   xmm2,xmm6
;
  irp _NN_,<0,1,2,3>                ;perform the initial key injection
    mov64   xmm4,ptr64 [esi+8*_NN_] ;and save a copy of the input block on stack
    mov64        ptr64 [esp+8*_NN_+Wcopy],xmm4
    paddq   xmm&_NN_,xmm4
  endm
;
if _SKEIN_DEBUG                     ;debug dump of state at this point
    Skein_Debug_Block 256
    Skein_Debug_Round 256,SKEIN_RND_KEY_INITIAL,saveRegs
endif
    add     esi, WCNT*8             ;skip to the next block
    mov         [ebx+blkPtr   ],esi ;save the updated block pointer
    ;
    ; now the key schedule is computed. Start the rounds
    ;
    xor     edx,edx                 ;edx = iteration count
if SKEIN_ASM_UNROLL and 256
_UNROLL_CNT =   ROUNDS_256/8        ;fully unrolled
else
_UNROLL_CNT =   SKEIN_UNROLL_256    ;partial unroll count
  if ((ROUNDS_256/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL_256" ;sanity check
  endif
    mov     esi,ebp                 ;use this as "rolling" pointer into ksTwk/ksKey
Skein_256_round_loop:               ;   (since there's no 16* scaled address mode)
endif
;
_Rbase_ = 0
rept _UNROLL_CNT*2                  ; here with X[0..3] in XMM0..XMM3
      R_256_FourRounds _Rbase_
_Rbase_ = _Rbase_+4
endm ;rept _UNROLL_CNT*2
;
  if _UNROLL_CNT ne (ROUNDS_256/8)
    cmp     edx,2*(ROUNDS_256/8)
    jb      Skein_256_round_loop
  endif
    ;----------------------------
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..3}
    irp _NN_,<0,1,2,3>
        mov64   xmm4,ptr64 [esp+Wcopy+8*_NN_]
        xorpd   xmm&_NN_,xmm4
        mov64        ptr64 [edi+X_VARS+8*_NN_],xmm&_NN_
    endm
    and     ptr08 [edi +TWEAK +15],FIRST_MASK8
if _SKEIN_DEBUG
    Skein_Debug_Round 256,SKEIN_RND_FEED_FWD,saveRegs
endif
    ; go back for more blocks, if needed
    dec     ecx
    jnz     Skein_256_block_loop
    
    Reset_Stack _Skein_256_Process_Block
    ret
;
_Skein_256_Process_Block endp
;
ifdef _SKEIN_CODE_SIZE
    public  _Skein_256_Process_Block_CodeSize
_Skein_256_Process_Block_CodeSize proc
    mov     eax,_Skein_256_Process_Block_CodeSize - _Skein_256_Process_Block
    ret
_Skein_256_Process_Block_CodeSize endp
;
    public  _Skein_256_Unroll_Cnt
_Skein_256_Unroll_Cnt proc
  if _UNROLL_CNT ne ROUNDS_256/8
    mov     eax,_UNROLL_CNT
  else
    xor     eax,eax
  endif
    ret
_Skein_256_Unroll_Cnt endp
endif
endif ;_USE_ASM_ and 256
;
;----------------------------------------------------------------
;
if _USE_ASM_ and 512
    public      _Skein_512_Process_Block
;
; void Skein_512_Process_Block(Skein_512_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd);
;
;;;;;;;;;;;;;;;;;
; MACRO: one round
;
R_512_Round macro _RR_, a0,a1,Ra, b0,b1,Rb, c0,c1,Rc, d0,d1,Rd
irp _nr_,<%((_RR_) and 7)>
_Ra_ = RC_512_&_nr_&_&Ra
_Rb_ = RC_512_&_nr_&_&Rb
_Rc_ = RC_512_&_nr_&_&Rc
_Rd_ = RC_512_&_nr_&_&Rd
endm
    paddq   xmm&a0,xmm&a1
                            _stX c0
    mov64   xmm&c0,xmm&a1
    psllq   xmm&a1,   _Ra_
    psrlq   xmm&c0,64-_Ra_
    xorpd   xmm&a1,xmm&c0
    xorpd   xmm&a1,xmm&a0

    paddq   xmm&b0,xmm&b1
                            _stX a0
    mov64   xmm&a0,xmm&b1
    psllq   xmm&b1,   _Rb_
    psrlq   xmm&a0,64-_Rb_
    xorpd   xmm&b1,xmm&b0
                            _ldX c0
    xorpd   xmm&b1,xmm&a0
                             
    paddq   xmm&c0,xmm&c1
    mov64   xmm&a0,xmm&c1
    psllq   xmm&c1,   _Rc_
    psrlq   xmm&a0,64-_Rc_
    xorpd   xmm&c1,xmm&c0
    xorpd   xmm&c1,xmm&a0
                             
    paddq   xmm&d0,xmm&d1
    mov64   xmm&a0,xmm&d1           
    psllq   xmm&d1,   _Rd_
    psrlq   xmm&a0,64-_Rd_
    xorpd   xmm&d1,xmm&a0
                            _ldX a0
    xorpd   xmm&d1,xmm&d0
  if _SKEIN_DEBUG
    Skein_Debug_Round 512,%(_RR_+1),saveRegs
  endif
endm
;
; MACRO: four rounds
R_512_FourRounds macro _RN_
    R_512_Round (_RN_)  , 0,1,0, 2,3,1, 4,5,2, 6,7,3
    R_512_Round (_RN_)+1, 2,1,0, 4,7,1, 6,5,2, 0,3,3
    R_512_Round (_RN_)+2, 4,1,0, 6,3,1, 0,5,2, 2,7,3
    R_512_Round (_RN_)+3, 6,1,0, 0,7,1, 2,5,2, 4,3,3

    ;inject key schedule
  irp _NN_,<0,1,2,3,4,5,6,7>
   if _UNROLL_CNT eq (ROUNDS_512/8)
    paddq xmm&_NN_,[FP_+ksKey+16*((((_RN_)/4)+(_NN_)+1) mod 9)]
    else
    paddq xmm&_NN_,[SI_+ksKey+16*((_NN_)+1)]
    endif
  endm
    _stX  0                       ;free up a register
    inc   edx                     ;bump round counter
    movd  xmm0,edx                ;inject the tweak
  if _UNROLL_CNT eq (ROUNDS_512/8)
    paddq xmm5,[FP_+ksTwk+16*(((_RN_)+1) mod 3)]
    paddq xmm6,[FP_+ksTwk+16*(((_RN_)+2) mod 3)]
    paddq xmm7,xmm0
  else ;looping version
    paddq xmm5,[SI_+ksTwk+16*1]
    paddq xmm6,[SI_+ksTwk+16*2]
    paddq xmm7,xmm0
;   
    mov64 xmm0,<ptr64 [SI_+ksKey]>;first, "rotate" key schedule on the stack
    mov64      <ptr64 [SI_+ksKey+16*(WCNT+1)]>,xmm0
    mov64 xmm0,<ptr64 [SI_+ksTwk]>;    (for next time through)
    mov64      <ptr64 [SI_+ksTwk+16*3]>,xmm0
    add   esi,16                  ;bump rolling pointer
  endif
    _ldX  0                       ;restore X0
  if _SKEIN_DEBUG
      Skein_Debug_Round 512,SKEIN_RND_KEY_INJECT,saveRegs
  endif
endm ;R_512_FourRounds
;;;;;;;;;;;;;;;;;
if _SKEIN_DEBUG ; macros for saving/restoring X_stk for debug routines
Put_XMM_512 equ <call _Put_XMM_512>
Get_XMM_512 equ <call _Get_XMM_512>

_Put_XMM_512:
  irp _NN_,<0,1,2,3,4,5,6,7>
    mov64          ptr64 [esp+X_stk+4+_NN_*8],xmm&_NN_
  endm
    ret
;
_Get_XMM_512:
  irp _NN_,<0,1,2,3,4,5,6,7>
    mov64  xmm&_NN_,ptr64 [esp+X_stk+4+_NN_*8]
  endm
    ret
endif
;
;;;;;;;;;;;;;;;;;
; code
;
_Skein_512_Process_Block proc near
    WCNT    =   8                   ;WCNT=8 for Skein-512
    Setup_Stack WCNT,ROUNDS_512
    ; main hash loop for Skein_512
Skein_512_block_loop:
    movd    xmm0,ptr32 [ebx+bitAdd]
    mov64   xmm1,ptr64 [edi+TWEAK+0]
    mov64   xmm2,ptr64 [edi+TWEAK+8]
    paddq   xmm1,xmm0               ;bump T0 by the bitAdd parameter
    mov64   ptr64 [edi+TWEAK],xmm1  ;save updated tweak value T0 (for next time)
    mov64   xmm0,xmm2
    xorpd   xmm0,xmm1               ;compute overall tweak parity
    movdqa  [FP_+ksTwk     ],xmm1   ;save the expanded tweak schedule on the stack
    movdqa  [FP_+ksTwk+16*1],xmm2
    movdqa  [FP_+ksTwk+16*2],xmm0

    mov     esi,[ebx+blkPtr]        ;esi --> input block
    mov     eax,KW_PARITY_LO        ;init key schedule parity accumulator
    mov     edx,KW_PARITY_HI
    movd    xmm0,eax
    movd    xmm7,edx
    unpcklps xmm0,xmm7              ;pack two 32-bit words into xmm0
;
  irp _NN_,<7,6,5,4,3,2,1>          ;copy in the chaining vars (skip #0 for now)
    mov64   xmm&_NN_,ptr64 [edi+X_VARS+8*_NN_]
    xorpd   xmm0,xmm&_NN_           ;update overall parity
    movdqa  [FP_+ksKey+16*_NN_],xmm&_NN_
   if _NN_ eq 5
    paddq   xmm5,xmm1               ;inject the initial tweak words
    paddq   xmm6,xmm2               ;  (before they get trashed in xmm1/2)
   endif
  endm
    mov64   xmm4,ptr64 [edi+X_VARS] ;handle #0 now
    xorpd   xmm0,xmm4               ;update overall parity
    movdqa  [FP_+ksKey+16* 0  ],xmm4;save the key value in slot #0
    movdqa  [FP_+ksKey+16*WCNT],xmm0;save overall parity at the end of the array
;
    mov64   xmm0,xmm4
  irp _NN_,<7,6,5,  4,3,2,1,0>      ;perform the initial key injection (except #4)
    mov64   xmm4,ptr64 [esi+ 8*_NN_];and save a copy of the input block on stack
    mov64        ptr64 [esp+ 8*_NN_+Wcopy],xmm4
    paddq   xmm&_NN_,xmm4
  endm
    mov64   xmm4,ptr64 [esi+ 8*4]   ;get input block word #4
    mov64        ptr64 [esp+ 8*4+Wcopy],xmm4
    paddq   xmm4,[FP_+ksKey+16*4]   ;inject the initial key
;
if _SKEIN_DEBUG                     ;debug dump of state at this point
    Skein_Debug_Block 512
    Skein_Debug_Round 512,SKEIN_RND_KEY_INITIAL,saveRegs
endif
    add     esi, WCNT*8             ;skip to the next block
    mov         [ebx+blkPtr],esi    ;save the updated block pointer
    ;
    ; now the key schedule is computed. Start the rounds
    ;
    xor     edx,edx                 ;edx = round counter
if SKEIN_ASM_UNROLL and 512
_UNROLL_CNT =   ROUNDS_512/8
else
_UNROLL_CNT =   SKEIN_UNROLL_512
  if ((ROUNDS_512/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL_512"
  endif
    mov     esi,ebp                 ;use this as "rolling" pointer into ksTwk/ksKey
Skein_512_round_loop:               ;   (since there's no 16* scaled address mode)
endif
_Rbase_ = 0
rept _UNROLL_CNT*2
      R_512_FourRounds _Rbase_
_Rbase_ = _Rbase_+4
endm ;rept _UNROLL_CNT
;
if (SKEIN_ASM_UNROLL and 512) eq 0
    cmp     edx,2*(ROUNDS_512/8)
    jb      Skein_512_round_loop
endif
    ;----------------------------
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..7}
    and     ptr08 [edi +TWEAK +15],FIRST_MASK8
irp _NN_,<0,2,4,6>                  ;do the aligned ones first
    xorpd   xmm&_NN_,[esp+Wcopy+8*_NN_]
    mov64   ptr64 [edi+X_VARS+8*_NN_],xmm&_NN_
endm
irp _NN_,<1,3,5,7>                  ;now we have some register space available
    mov64   xmm0,ptr64 [esp+Wcopy+8*_NN_]
    xorpd   xmm&_NN_,xmm0
    mov64   ptr64 [edi+X_VARS+8*_NN_],xmm&_NN_
endm
if _SKEIN_DEBUG
    Skein_Debug_Round 512,SKEIN_RND_FEED_FWD
endif
    ; go back for more blocks, if needed
    dec     ecx
    jnz     Skein_512_block_loop

    Reset_Stack _Skein_512_Process_Block
    ret
_Skein_512_Process_Block endp
;
ifdef _SKEIN_CODE_SIZE
    public  _Skein_512_Process_Block_CodeSize
_Skein_512_Process_Block_CodeSize proc
    mov     eax,_Skein_512_Process_Block_CodeSize - _Skein_512_Process_Block
    ret
_Skein_512_Process_Block_CodeSize endp
;
    public  _Skein_512_Unroll_Cnt
_Skein_512_Unroll_Cnt proc
  if _UNROLL_CNT ne ROUNDS_512/8
    mov     eax,_UNROLL_CNT
  else
    xor     eax,eax
  endif
    ret
_Skein_512_Unroll_Cnt endp
endif
;
endif ; _USE_ASM_ and 512
;
;----------------------------------------------------------------
;
if _USE_ASM_ and 1024
    public      _Skein1024_Process_Block
;
; void Skein_1024_Process_Block(Skein_1024_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd);
;
R_1024_REGS equ     (5)     ;keep this many block variables in registers
;
;;;;;;;;;;;;;;;;
if _SKEIN_DEBUG ; macros for saving/restoring X_stk for debug routines
Put_XMM_1024 equ <call _Put_XMM_1024>
Get_XMM_1024 equ <call _Get_XMM_1024>

_Put_XMM_1024:
_NN_ = 0
  rept R_1024_REGS
   irp _rr_,<%(_NN_)>
    mov64           ptr64 [esp+X_stk+4+8*_NN_],xmm&_rr_
   endm
_NN_ = _NN_+1
  endm
    ret
;
_Get_XMM_1024:
_NN_ = 0
  rept R_1024_REGS
   irp _rr_,<%(_NN_)>
    mov64  xmm&_rr_,ptr64 [esp+X_stk+4+8*_NN_]
   endm
_NN_ = _NN_+1
  endm
    ret
endif
;
;;;;;;;;;;;;;;;;;
; MACRO: one mix step
MixStep_1024 macro  x0,x1,rotIdx0,rotIdx1,_debug_
_r0_ =  x0      ;default, if already loaded
_r1_ =  x1
  ; load the regs (if necessary)
  if (x0 ge R_1024_REGS)
_r0_ =       5
  mov64   xmm5,ptr64 [esp+X_stk+8*(x0)]
  endif
  if (x1 ge R_1024_REGS)
_r1_ =       6     
    mov64 xmm6,ptr64 [esp+X_stk+8*(x1)]
  endif
  ; do the mix
  irp _rx_,<%((rotIdx0) and 7)>
_Rc_ = RC_1024_&_rx_&_&rotIdx1  ;rotation constant
  endm
  irp _x0_,<%_r0_>
  irp _x1_,<%_r1_>
    paddq   xmm&_x0_,xmm&_x1_
    mov64   xmm7    ,xmm&_x1_
    psllq   xmm&_x1_,   _Rc_
    psrlq   xmm7    ,64-_Rc_
    xorpd   xmm&_x1_,xmm&_x0_
    xorpd   xmm&_x1_,xmm7
  endm
  endm
  ; save the regs (if necessary)
  if (x0 ge R_1024_REGS)
    mov64   ptr64 [esp+X_stk+8*(x0)],xmm5
  endif
  if (x1 ge R_1024_REGS)
    mov64   ptr64 [esp+X_stk+8*(x1)],xmm6
  endif
  ; debug output
  if _SKEIN_DEBUG and (0 ne (_debug_ + 0))
    Skein_Debug_Round 1024,%((RotIdx0)+1),saveRegs
  endif
endm
;;;;;;;;;;;;;;;;;
; MACRO: four rounds
;
R_1024_FourRounds macro _RR_
    ;--------- round _RR_
    MixStep_1024     0, 1,%((_RR_)+0),0
    MixStep_1024     2, 3,%((_RR_)+0),1
    MixStep_1024     4, 5,%((_RR_)+0),2
    MixStep_1024     6, 7,%((_RR_)+0),3
    MixStep_1024     8, 9,%((_RR_)+0),4
    MixStep_1024    10,11,%((_RR_)+0),5
    MixStep_1024    12,13,%((_RR_)+0),6
    MixStep_1024    14,15,%((_RR_)+0),7,1
    ;--------- round _RR_+1
    MixStep_1024     0, 9,%((_RR_)+1),0
    MixStep_1024     2,13,%((_RR_)+1),1
    MixStep_1024     6,11,%((_RR_)+1),2
    MixStep_1024     4,15,%((_RR_)+1),3
    MixStep_1024    10, 7,%((_RR_)+1),4
    MixStep_1024    12, 3,%((_RR_)+1),5
    MixStep_1024    14, 5,%((_RR_)+1),6
    MixStep_1024     8, 1,%((_RR_)+1),7,1
    ;--------- round _RR_+2
    MixStep_1024     0, 7,%((_RR_)+2),0    
    MixStep_1024     2, 5,%((_RR_)+2),1
    MixStep_1024     4, 3,%((_RR_)+2),2    
    MixStep_1024     6, 1,%((_RR_)+2),3    
    MixStep_1024    12,15,%((_RR_)+2),4
    MixStep_1024    14,13,%((_RR_)+2),5    
    MixStep_1024     8,11,%((_RR_)+2),6    
    MixStep_1024    10, 9,%((_RR_)+2),7,1
    ;--------- round _RR_+3
    MixStep_1024     0,15,%((_RR_)+3),0
    MixStep_1024     2,11,%((_RR_)+3),1
    MixStep_1024     6,13,%((_RR_)+3),2
    MixStep_1024     4, 9,%((_RR_)+3),3
    MixStep_1024    14, 1,%((_RR_)+3),4
    MixStep_1024     8, 5,%((_RR_)+3),5
    MixStep_1024    10, 3,%((_RR_)+3),6
    MixStep_1024    12, 7,%((_RR_)+3),7,1

    inc   edx                     ;edx = round number
    movd  xmm7,edx
    ;inject the key
irp _NN_,<15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0>
  if _UNROLL_CNT ne (ROUNDS_1024/8)
    if _NN_ lt R_1024_REGS
      paddq xmm&_NN_,ptr64 [SI_+ksKey+16*_NN_+16]
    else
      mov64 xmm6    ,ptr64 [esp+X_stk+ 8*_NN_]
     if     _NN_ eq 15
      paddq xmm6,xmm7
     elseif _NN_ eq 14
      paddq xmm6,ptr64 [SI_+ksTwk+16*2]
     elseif _NN_ eq 13
      paddq xmm6,ptr64 [SI_+ksTwk+16*1]
     endif
      paddq xmm6    ,ptr64 [SI_+ksKey+16*_NN_+16]
      mov64          ptr64 [esp+X_stk+ 8*_NN_],xmm6
    endif
  else
    if _NN_ lt R_1024_REGS
      paddq xmm&_NN_,ptr64 [FP_+ksKey+16*(((_Rbase_/4)+(_NN_)+1) mod 17)]
    else
      mov64 xmm6,ptr64 [esp+X_stk+ 8*_NN_]
      paddq xmm6,ptr64 [FP_+ksKey+16*(((_Rbase_/4)+(_NN_)+1) mod 17)]
     if     _NN_ eq 15
      paddq xmm6,xmm7
     elseif _NN_ eq 14
      paddq xmm6,ptr64 [FP_+ksTwk+16*(((_Rbase_/4)+2) mod  3)]
     elseif _NN_ eq 13
      paddq xmm6,ptr64 [FP_+ksTwk+16*(((_Rbase_/4)+1) mod  3)]
     endif
      mov64      ptr64 [esp+X_stk+ 8*_NN_],xmm6
    endif
  endif
endm
if _UNROLL_CNT ne (ROUNDS_1024/8) ;rotate the key schedule on the stack
    mov64 xmm6,ptr64 [SI_+ksKey]
    mov64 xmm7,ptr64 [SI_+ksTwk]
    mov64      ptr64 [SI_+ksKey+16*(WCNT+1)],xmm6
    mov64      ptr64 [SI_+ksTwk+16* 3      ],xmm7
    add   esi,16                  ;bump rolling pointer
endif
if _SKEIN_DEBUG
      Skein_Debug_Round 1024,SKEIN_RND_KEY_INJECT ,saveRegs
endif
endm ;R_1024_FourRounds
;;;;;;;;;;;;;;;;
; code
;
_Skein1024_Process_Block proc near
;
    WCNT    =   16                  ;WCNT=16 for Skein-1024
    Setup_Stack WCNT,ROUNDS_1024
    add     edi,80h                 ;bias the edi ctxt offsets to keep them all short
ctx equ    <edi-80h>                ;offset alias
    ; main hash loop for Skein1024
Skein1024_block_loop:
    movd    xmm0,ptr32 [ebx+bitAdd]
    mov64   xmm1,ptr64 [ctx+TWEAK+0]
    mov64   xmm2,ptr64 [ctx+TWEAK+8]
    paddq   xmm1,xmm0               ;bump T0 by the bitAdd parameter
    mov64   ptr64 [ctx+TWEAK],xmm1  ;save updated tweak value T0 (for next time)
    mov64   xmm0,xmm2
    xorpd   xmm0,xmm1               ;compute overall tweak parity
    movdqa  [FP_+ksTwk   ],xmm1     ;save the expanded tweak schedule on the stack
    movdqa  [FP_+ksTwk+16],xmm2
    movdqa  [FP_+ksTwk+32],xmm0

    mov     esi,[ebx+blkPtr]        ;esi --> input block
    mov     eax,KW_PARITY_LO        ;init key schedule parity accumulator
    mov     edx,KW_PARITY_HI
    movd    xmm7,eax
    movd    xmm6,edx
    unpcklps xmm7,xmm6              ;pack two 32-bit words into xmm7
;
    lea     eax,[esp+80h]           ;use short offsets for Wcopy, X_stk writes below
SP_ equ    <eax-80h>                ;[eax+OFFS] mode is one byte shorter than [esp+OFFS]
irp _NN_,<15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0>
    mov64   xmm6,ptr64 [ctx+X_VARS+8*_NN_]
    xorpd   xmm7,xmm6               ;update overall parity
    movdqa  [FP_+ksKey+16*_NN_],xmm6;save the key schedule on the stack
  if _NN_ lt R_1024_REGS
     _rr_  =  _NN_
  else
     _rr_  =   R_1024_REGS
  endif
  irp _rn_,<%(_rr_)>
    mov64   xmm&_rn_,ptr64 [esi+         8*_NN_];save copy of the input block on stack
    mov64            ptr64 [SP_+ Wcopy + 8*_NN_],xmm&_rn_   ;(for feedforward later)
    paddq   xmm&_rn_,xmm6               ;inject the key into the block
   if _NN_ eq 13
    paddq   xmm&_rn_,xmm1               ;inject the initial tweak words
   elseif _NN_ eq 14
    paddq   xmm&_rn_,xmm2
   endif
   if _NN_ ge R_1024_REGS               ;only save X[5..15] on stack, leave X[0..4] in regs
    mov64   ptr64 [SP_+X_stk+8*_NN_],xmm&_rn_ 
   endif
  endm
endm
    movdqa  [FP_+ksKey+16*WCNT],xmm7;save overall key parity at the end of the array
;
if _SKEIN_DEBUG                     ;debug dump of state at this point
    Skein_Debug_Block 1024
    Skein_Debug_Round 1024,SKEIN_RND_KEY_INITIAL,saveRegs
endif
    add     esi, WCNT*8             ;skip to the next block
    mov         [ebx+blkPtr],esi    ;save the updated block pointer
    ;
    ; now the key schedule is computed. Start the rounds
    ;
    xor     edx,edx                 ;edx = round counter
if SKEIN_ASM_UNROLL and 1024
_UNROLL_CNT =   ROUNDS_1024/8
else
_UNROLL_CNT =   SKEIN_UNROLL_1024
  if ((ROUNDS_1024/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL_1024"
  endif
    mov     esi,ebp                 ;use this as "rolling" pointer into ksTwk/ksKey
Skein_1024_round_loop:
endif
;
_Rbase_ = 0
rept _UNROLL_CNT*2
    R_1024_FourRounds %_Rbase_
_Rbase_ = _Rbase_+4
endm ;rept _UNROLL_CNT
;
if (SKEIN_ASM_UNROLL and 1024) eq 0
    cmp     edx,2*(ROUNDS_1024/8)
    jb      Skein_1024_round_loop
endif
    and     ptr08 [ctx +TWEAK +15],FIRST_MASK8      ;clear tweak bit for next time thru
    ;----------------------------
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..15}
    lea     eax,[esp+80h]                           ;allow short offsets to X_stk and Wcopy
irp _NN_,<0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15>
  if _NN_ lt R_1024_REGS
    if _NN_ and 1                                   ;already in regs: no load needed
      mov64 xmm7  ,ptr64 [SP_+ Wcopy + 8*_NN_]      ;unaligned
      xorpd xmm&_NN_,xmm7
    else
      xorpd xmm&_NN_,    [SP_+ Wcopy + 8*_NN_]      ;aligned
    endif
      mov64        ptr64 [ctx+ X_vars+ 8*_NN_],xmm&_NN_
  else
      mov64   xmm7,ptr64 [SP_+ X_stk + 8*_NN_]      ;load X value from stack
    if _NN_ and 1
      mov64   xmm6,ptr64 [SP_+ Wcopy + 8*_NN_]      ;unaligned
      xorpd   xmm7,xmm6
    else
      xorpd   xmm7,      [SP_+ Wcopy + 8*_NN_]      ;aligned
    endif
      mov64        ptr64 [ctx+ X_vars+ 8*_NN_],xmm7
 endif
endm
if _SKEIN_DEBUG
    Skein_Debug_Round 1024,SKEIN_RND_FEED_FWD   ;no need to save regs on stack here
endif
    ; go back for more blocks, if needed
    dec     ecx
    jnz     Skein1024_block_loop

    Reset_Stack _Skein1024_Process_Block
    ret
_Skein1024_Process_Block endp
;
ifdef _SKEIN_CODE_SIZE
    public  _Skein1024_Process_Block_CodeSize
_Skein1024_Process_Block_CodeSize proc
    mov     eax,_Skein1024_Process_Block_CodeSize - _Skein1024_Process_Block
    ret
_Skein1024_Process_Block_CodeSize endp
;
    public  _Skein1024_Unroll_Cnt
_Skein1024_Unroll_Cnt proc
  if _UNROLL_CNT ne ROUNDS_1024/8
    mov     eax,_UNROLL_CNT
  else
    xor     eax,eax
  endif
    ret
_Skein1024_Unroll_Cnt endp
endif
;
endif ; _USE_ASM_ and 1024
;----------------------------------------------------------------
    end
