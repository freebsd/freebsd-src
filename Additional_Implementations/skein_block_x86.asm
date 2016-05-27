;
;----------------------------------------------------------------
; 32-bit x86 assembler code for Skein block functions
;
; Author: Doug Whiting, Hifn
;
; This code is released to the public domain.
;----------------------------------------------------------------
;
    .386p
    .model flat
    .code
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
;;;;;;;;;;;;;;;;;
ifndef SKEIN_LOOP  
_SKEIN_LOOP       = 0                       ;default is all fully unrolled
else
_SKEIN_LOOP       = SKEIN_LOOP
endif
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
KW_PARITY_HI=   01BD11BDAh                  ;overall parity of key schedule words (hi32/lo32)
FIRST_MASK  =   NOT (1 SHL 30)              ;FIRST block flag bit
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
;  Input:  rHi,rLo
; Output: <rHi,rLo> <<< _RCNT_
Rol64 macro rHi,rLo,tmp,_RCNT_
  if _RCNT_  ;is there anything to do?
    if _RCNT_ lt 32
      mov   tmp,rLo
      shld  rLo,rHi,_RCNT_
      shld  rHi,tmp,_RCNT_
    elseif _RCNT_ gt 32
      mov   tmp,rLo
      shrd  rLo,rHi,((64-_RCNT_) AND 63)
      shrd  rHi,tmp,((64-_RCNT_) AND 63)
    else    
      xchg  rHi,rLo ;special case for _RCNT_ == 32
    endif
  endif
endm
;
;  Input:  rHi,rLo
; Output: <rHi,rLo> <<< rName&&rNum, and tmp trashed;
RotL64 macro rHi,rLo,tmp,BLK_SIZE,ROUND_NUM,MIX_NUM
_RCNT_ = ( RC_&BLK_SIZE&_&ROUND_NUM&_&MIX_NUM AND 63 )
    Rol64 rHi,rLo,tmp,_RCNT_
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
Setup_Stack macro WCNT,KS_CNT
_STK_OFFS_  =   0                   ;starting offset from esp 
    ;----- local  variables         ;<-- esp
    StackVar    X_stk  ,8*(WCNT)    ;local context vars
    StackVar    Wcopy  ,8*(WCNT)    ;copy of input block    
    StackVar    ksTwk  ,8*3         ;key schedule: tweak words
    StackVar    ksKey  ,8*(WCNT)+8  ;key schedule: key   words
  if WCNT le 8
FRAME_OFFS  =   _STK_OFFS_          ;<-- ebp
  else
FRAME_OFFS  =   _STK_OFFS_-8*4      ;<-- ebp
  endif
  if (SKEIN_ASM_UNROLL and (WCNT*64)) eq 0
    StackVar    ksRot ,16*(KS_CNT+0);leave space for "rotation" to happen
  endif
LOCAL_SIZE  =   _STK_OFFS_          ;size of local vars
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
;   * the most frequently used variable is X_stk[], based at [esp+0]
;   * the next most used is the key schedule words
;       so ebp is "centered" there, allowing short offsets to the key/tweak
;       schedule even in 1024-bit Skein case
;   * the Wcopy variables are infrequently accessed, but they have long 
;       offsets from both esp and ebp only in the 1024-bit case.
;   * all other local vars and calling parameters can be accessed 
;       with short offsets, except in the 1024-bit case
;
    pushad                          ;save all regs
    sub     esp,LOCAL_SIZE          ;make room for the locals
    lea     ebp,[esp+FRAME_OFFS]    ;maximize use of short offsets
    mov     edi,[FP_+ctxPtr ]       ;edi --> context
;
endm ;Setup_Stack
;
FP_         equ <ebp-FRAME_OFFS>    ;keep as many short offsets as possible
;
;----------------------------------------------------------------
;
Reset_Stack macro   procStart
    add     esp,LOCAL_SIZE          ;get rid of locals (wipe??)
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
    pushad                          ;save all regs
    lea     eax,[FP_+ksTwk]
    lea     ebx,[FP_+ksKey]
    lea     ecx,[esp+32+Wcopy]
    mov     edx,[FP_+ctxPtr]        ;ctx_hdr_ptr
    lea     edx,[edx+X_VARS]        ;edx ==> cxt->X[]
    push    eax                     ;tsPtr
    push    ebx                     ;ksPtr
    push    ecx                     ;wPtr
    push    dword ptr [FP_+blkPtr]  ;blkPtr
    push    edx                     ;ctx->Xptr
    push    dword ptr [FP_+ctxPtr]  ;ctx_hdr_ptr
    mov     eax,BLK_BITS
    push    eax                     ;bits
  ifdef _MINGW_
    call    _Skein_Show_Block-4     ;strange linkage??
  else
    call    _Skein_Show_Block
  endif
    add     esp,7*4                 ;discard parameter space on stack
    popad                           ;restore regs
endm ;Skein_Debug_Block

;
Skein_Debug_Round macro BLK_SIZE,R,saveRegs
;
;void Skein_Show_Round(uint_t bits,const Skein_Ctxt_Hdr_t *h,int r,const u64b_t *X);
;
  ifnb <saveRegs>
    mov         [esp+X_stk+ 0],eax  ;save internal vars for debug dump
    mov         [esp+X_stk+ 4],ebx
    mov         [esp+X_stk+ 8],ecx
    mov         [esp+X_stk+12],edx
  endif
    pushad                          ;save all regs
  if R ne SKEIN_RND_FEED_FWD
    lea     eax,[esp+32+X_stk]
  else
    mov     eax,[FP_+ctxPtr]
    add     eax,X_VARS
  endif
    push    eax                     ;Xptr
  if (SKEIN_ASM_UNROLL and BLK_SIZE) or (R ge SKEIN_RND_SPECIAL)
    mov     eax,R
  else
    lea     eax,[4*edi+1+(((R)-1) and 3)] ;compute round number using edi
  endif
    push    eax                     ;round number
    push    dword ptr [FP_+ctxPtr]  ;ctx_hdr_ptr
    mov     eax,BLK_SIZE
    push    eax                     ;bits
  ifdef _MINGW_
    call    _Skein_Show_Round-4     ;strange linkage??
  else
    call    _Skein_Show_Round
  endif
    add     esp,4*4                 ;discard parameter space on stack
    popad                           ;restore regs
endm  ;Skein_Debug_Round
endif ;ifdef SKEIN_DEBUG
;
;----------------------------------------------------------------
;
; MACRO: a mix step
;
MixStep     macro   BLK_SIZE,ld_A,ld_C,st_A,st_C,RotNum0,RotNum1,_debug_
  ifnb <ld_A>
    mov     eax,[esp+X_stk+8*(ld_A)+0]
    mov     ebx,[esp+X_stk+8*(ld_A)+4]
  endif
  ifnb <ld_C>
    mov     ecx,[esp+X_stk+8*(ld_C)+0]
    mov     edx,[esp+X_stk+8*(ld_C)+4]
  endif
    add     eax, ecx                ;X[A] += X[C]
    adc     ebx, edx
  ifnb <st_A>
    mov         [esp+X_stk+8*(st_A)+0],eax
    mov         [esp+X_stk+8*(st_A)+4],ebx
  endif
__rNum0 = (RotNum0) AND 7
    RotL64  ecx, edx, esi,%(BLK_SIZE),%(__rNum0),%(RotNum1) ;X[C] <<<= RC_<BLK_BITS,RotNum0,RotNum1>
    xor     ecx, eax                ;X[C] ^= X[A]
    xor     edx, ebx
  if _SKEIN_DEBUG or  (0 eq (_debug_ + 0))
   ifb <st_C>
    mov         [esp+X_stk+8*(ld_C)+0],ecx
    mov         [esp+X_stk+8*(ld_C)+4],edx
   else
    mov         [esp+X_stk+8*(st_C)+0],ecx
    mov         [esp+X_stk+8*(st_C)+4],edx
   endif
  endif
  if _SKEIN_DEBUG and (0 ne (_debug_ + 0))
    Skein_Debug_Round BLK_SIZE,%(RotNum0+1)
  endif
endm ;MixStep
;
;;;;;;;;;;;;;;;;;
;
; MACRO: key schedule injection
;
ks_Inject macro BLK_SIZE,X_load,X_stor,rLo,rHi,rndBase,keyIdx,twkIdx,ROUND_ADD
    ;are rLo,rHi values already loaded? if not, load them now
  ifnb <X_load> 
    mov     rLo,[esp+X_stk +8*(X_load)  ]
    mov     rHi,[esp+X_stk +8*(X_load)+4]
  endif

  ;inject the 64-bit key schedule value (and maybe the tweak as well)
if SKEIN_ASM_UNROLL and BLK_SIZE
_kOffs_ = ((rndBase)+(keyIdx)) mod ((BLK_SIZE/64)+1)
    add     rLo,[FP_+ksKey+8*_kOffs_+ 0]
    adc     rHi,[FP_+ksKey+8*_kOffs_+ 4]
  ifnb <twkIdx>
_tOffs_ = ((rndBase)+(twkIdx)) mod 3
    add     rLo,[FP_+ksTwk+8*_tOffs_+ 0]
    adc     rHi,[FP_+ksTwk+8*_tOffs_+ 4]
  endif
  ifnb <ROUND_ADD>
    add     rLo,(ROUND_ADD)
    adc     rHi,0
  endif
else
    add     rLo,[FP_+ksKey+8*(keyIdx)+8*edi  ]
    adc     rHi,[FP_+ksKey+8*(keyIdx)+8*edi+4]
  ifnb <twkIdx>
    add     rLo,[FP_+ksTwk+8*(twkIdx)+8*edi  ]
    adc     rHi,[FP_+ksTwk+8*(twkIdx)+8*edi+4]
  endif
  ifnb <ROUND_ADD>
    add     rLo,edi                     ;edi is the round number 
    adc     rHi,0
  endif
endif

  ;do we need to store updated rLo,rHi values? if so, do it now
  ifnb <X_stor>
    mov         [esp+X_stk +8*(X_stor)  ],rLo
    mov         [esp+X_stk +8*(X_stor)+4],rHi
  endif
endm ;ks_Inject
;
;----------------------------------------------------------------
; MACRO: key schedule rotation
;
ks_Rotate macro rLo,rHi,WCNT
    mov   rLo,[FP_+ksKey+8*edi+ 0]       ;"rotate" the key schedule in memory
    mov   rHi,[FP_+ksKey+8*edi+ 4]
    mov       [FP_+ksKey+8*edi+8*(WCNT+1)+ 0],rLo
    mov       [FP_+ksKey+8*edi+8*(WCNT+1)+ 4],rHi
    mov   rLo,[FP_+ksTwk+8*edi+ 0]
    mov   rHi,[FP_+ksTwk+8*edi+ 4]
    mov       [FP_+ksTwk+8*edi+8*3+ 0],rLo
    mov       [FP_+ksTwk+8*edi+8*3+ 4],rHi
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
; MACRO: two rounds
;
R_256_TwoRounds macro _RR_,ld_0
    ; here with edx:ecx = X[1]
    ;--------- round _RR_
    MixStep 256,ld_0, ,0,1,((_RR_)+0),0
    MixStep 256,   2,3,2,3,((_RR_)+0),1,1

    ; here with edx:ecx = X[3]
    ;--------- round _RR_ + 1
    MixStep 256,   0, ,0,3,((_RR_)+1),0
    MixStep 256,   2,1,2,1,((_RR_)+1),1,1

    ; here with edx:ecx = X[1]
endm ;R_256_TwoRounds
;
;;;;;;;;;;;;;;;;;
;
; code
;
_Skein_256_Process_Block proc near
    WCNT    =   4                   ;WCNT=4 for Skein-256
    Setup_Stack WCNT,(ROUNDS_256/8)

    ; main hash loop for Skein_256
Skein_256_block_loop:
    mov     eax,[edi+TWEAK+ 0]      ;ebx:eax = tweak word T0
    mov     ebx,[edi+TWEAK+ 4]
    mov     ecx,[edi+TWEAK+ 8]      ;edx:ecx = tweak word T1
    mov     edx,[edi+TWEAK+12]

    add     eax,[FP_+bitAdd  ]      ;bump T0 by the bitAdd parameter
    adc     ebx, 0
    mov         [edi+TWEAK   ],eax  ;save updated tweak value T0
    mov         [edi+TWEAK+ 4],ebx

    mov         [FP_+ksTwk   ],eax  ;build the tweak schedule on the stack
    mov         [FP_+ksTwk+ 4],ebx
    xor     eax,ecx                 ;ebx:eax = T0 ^ T1
    xor     ebx,edx
    mov         [FP_+ksTwk+ 8],ecx
    mov         [FP_+ksTwk+12],edx
    mov         [FP_+ksTwk+16],eax
    mov         [FP_+ksTwk+20],ebx

    mov     eax,KW_PARITY_LO        ;init parity accumulator
    mov     ebx,KW_PARITY_HI
;
_NN_ = 0
  rept WCNT                         ;copy in the chaining vars
    mov     ecx,[edi+X_VARS+_NN_   ]
    mov     edx,[edi+X_VARS+_NN_+ 4]
    xor     eax,ecx                 ;compute overall parity along the way
    xor     ebx,edx
    mov         [FP_+ksKey +_NN_   ],ecx
    mov         [FP_+ksKey +_NN_+ 4],edx
_NN_ = _NN_+8
  endm
;
    mov         [FP_+ksKey +_NN_   ],eax ;save overall parity at the end of the array
    mov         [FP_+ksKey +_NN_+ 4],ebx

    mov     esi,[FP_+blkPtr ]       ;esi --> input block
;
_NN_ = WCNT*8-16                    ;work down from the end
  rept WCNT/2                       ;perform initial key injection
    mov     eax,[esi+_NN_       + 0]
    mov     ebx,[esi+_NN_       + 4]
    mov     ecx,[esi+_NN_       + 8]
    mov     edx,[esi+_NN_       +12]
    mov         [esp+_NN_+Wcopy + 0],eax
    mov         [esp+_NN_+Wcopy + 4],ebx
    mov         [esp+_NN_+Wcopy + 8],ecx
    mov         [esp+_NN_+Wcopy +12],edx
    add     eax,[FP_+_NN_+ksKey + 0]
    adc     ebx,[FP_+_NN_+ksKey + 4]
    add     ecx,[FP_+_NN_+ksKey + 8]
    adc     edx,[FP_+_NN_+ksKey +12]
   if     _NN_ eq (WCNT*8-16)       ;inject the tweak words
    add     eax,[FP_+     ksTwk + 8];   (at the appropriate points)
    adc     ebx,[FP_+     ksTwk +12]
   elseif _NN_ eq (WCNT*8-32)
    add     ecx,[FP_+     ksTwk + 0]
    adc     edx,[FP_+     ksTwk + 4]
   endif
   if _NN_ or _SKEIN_DEBUG
    mov         [esp+_NN_+X_stk + 0],eax
    mov         [esp+_NN_+X_stk + 4],ebx
    mov         [esp+_NN_+X_stk + 8],ecx
    mov         [esp+_NN_+X_stk +12],edx
   endif
_NN_ = _NN_ - 16                    ;end at X[0], so regs are already loaded for first MIX!
  endm
;
if _SKEIN_DEBUG                     ;debug dump of state at this point
    Skein_Debug_Block WCNT*64 
    Skein_Debug_Round WCNT*64,SKEIN_RND_KEY_INITIAL
endif
    add     esi, WCNT*8             ;skip the block
    mov         [FP_+blkPtr   ],esi ;update block pointer
    ;
    ; now the key schedule is computed. Start the rounds
    ;
if SKEIN_ASM_UNROLL and 256
_UNROLL_CNT =   ROUNDS_256/8
else
_UNROLL_CNT =   SKEIN_UNROLL_256    ;unroll count
  if ((ROUNDS_256/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL_256"
  endif
    xor     edi,edi                 ;edi = iteration count
Skein_256_round_loop:
endif
_Rbase_ = 0
rept _UNROLL_CNT*2
      ; here with X[0], X[1] already loaded into eax..edx
      R_256_TwoRounds %(4*_Rbase_+00),
      R_256_TwoRounds %(4*_Rbase_+02),0

      ;inject key schedule
  if _UNROLL_CNT ne (ROUNDS_256/8)
      ks_Rotate eax,ebx,WCNT
      inc   edi                     ;edi = round number
  endif
_Rbase_ = _Rbase_+1
      ks_Inject 256,3,3,eax,ebx,_Rbase_,3, ,_Rbase_
      ks_Inject 256,2,2,eax,ebx,_Rbase_,2,1
      ks_Inject 256, , ,ecx,edx,_Rbase_,1,0
      ks_Inject 256,0, ,eax,ebx,_Rbase_,0
  if _SKEIN_DEBUG
      Skein_Debug_Round 256,SKEIN_RND_KEY_INJECT,saveRegs
  endif
endm ;rept _UNROLL_CNT
;
  if _UNROLL_CNT ne (ROUNDS_256/8)
    cmp     edi,2*(ROUNDS_256/8)
    jb      Skein_256_round_loop
    mov     edi,[FP_+ctxPtr ]           ;restore edi --> context
  endif
    ;----------------------------
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..3}
_NN_ = 0
 rept WCNT/2
   if _NN_  ;eax..edx already loaded the first time
    mov     eax,[esp+X_stk + _NN_ + 0]
    mov     ebx,[esp+X_stk + _NN_ + 4]
    mov     ecx,[esp+X_stk + _NN_ + 8]
    mov     edx,[esp+X_stk + _NN_ +12]
   endif
   if _NN_ eq 0
    and     dword ptr [edi +TWEAK +12],FIRST_MASK
   endif
    xor     eax,[esp+Wcopy + _NN_ + 0]
    xor     ebx,[esp+Wcopy + _NN_ + 4]
    xor     ecx,[esp+Wcopy + _NN_ + 8]
    xor     edx,[esp+Wcopy + _NN_ +12]
    mov         [edi+X_VARS+ _NN_ + 0],eax
    mov         [edi+X_VARS+ _NN_ + 4],ebx
    mov         [edi+X_VARS+ _NN_ + 8],ecx
    mov         [edi+X_VARS+ _NN_ +12],edx
_NN_ = _NN_+16
  endm
if _SKEIN_DEBUG
    Skein_Debug_Round 256,SKEIN_RND_FEED_FWD
endif
    ; go back for more blocks, if needed
    dec     dword ptr [FP_+blkCnt]
    jnz     Skein_256_block_loop
    
    Reset_Stack _Skein_256_Process_Block
    ret
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
; MACRO: four rounds
;
R_512_FourRounds macro _RR_,ld_0
    ; here with edx:ecx = X[1]
    ;--------- round _RR_
    ; R512(0,1,2,3,4,5,6,7,R_0, 1);
    MixStep 512, ld_0, ,0,1,((_RR_)+0),0
    MixStep 512,    2,3,2,3,((_RR_)+0),1
    MixStep 512,    4,5,4,5,((_RR_)+0),2
    MixStep 512,    6,7,6, ,((_RR_)+0),3,1

    ; here with edx:ecx = X[7]
    ; R512(2,1,4,7,6,5,0,3,R_1, 2);
    MixStep 512,    4, ,4,7,((_RR_)+1),1
    MixStep 512,    6,5,6,5,((_RR_)+1),2
    MixStep 512,    0,3,0,3,((_RR_)+1),3
    MixStep 512,    2,1,2, ,((_RR_)+1),0,1

    ; here with edx:ecx = X[1]
    ; R512(4,1,6,3,0,5,2,7,R_2, 3);
    MixStep 512,    4, ,4,1,((_RR_)+2),0
    MixStep 512,    6,3,6,3,((_RR_)+2),1
    MixStep 512,    0,5,0,5,((_RR_)+2),2
    MixStep 512,    2,7,2, ,((_RR_)+2),3,1

    ; here with edx:ecx = X[7]
    ; R512(6,1,0,7,2,5,4,3,R_3, 4);
    MixStep 512,    0, ,0,7,((_RR_)+3),1
    MixStep 512,    2,5,2,5,((_RR_)+3),2
    MixStep 512,    4,3,4,3,((_RR_)+3),3
    MixStep 512,    6,1,6, ,((_RR_)+3),0,1

endm ;R_512_FourRounds
;
;;;;;;;;;;;;;;;;;
; code
;
_Skein_512_Process_Block proc near
    WCNT    =   8                   ;WCNT=8 for Skein-512
    Setup_Stack WCNT,(ROUNDS_512/8)

    ; main hash loop for Skein_512
Skein_512_block_loop:
    mov     eax,[edi+TWEAK+ 0]      ;ebx:eax = tweak word T0
    mov     ebx,[edi+TWEAK+ 4]
    mov     ecx,[edi+TWEAK+ 8]      ;edx:ecx = tweak word T1
    mov     edx,[edi+TWEAK+12]

    add     eax,[FP_+bitAdd  ]      ;bump T0 by the bitAdd parameter
    adc     ebx, 0
    mov         [edi+TWEAK   ],eax  ;save updated tweak value T0
    mov         [edi+TWEAK+ 4],ebx

    mov         [FP_+ksTwk   ],eax  ;build the tweak schedule on the stack
    mov         [FP_+ksTwk+ 4],ebx
    xor     eax,ecx                 ;ebx:eax = T0 ^ T1
    xor     ebx,edx
    mov         [FP_+ksTwk+ 8],ecx
    mov         [FP_+ksTwk+12],edx
    mov         [FP_+ksTwk+16],eax
    mov         [FP_+ksTwk+20],ebx

    mov     eax,KW_PARITY_LO        ;init parity accumulator
    mov     ebx,KW_PARITY_HI
;
_NN_ = 0
  rept WCNT                         ;copy in the chaining vars
    mov     ecx,[edi+X_VARS+_NN_   ]
    mov     edx,[edi+X_VARS+_NN_+ 4]
    xor     eax,ecx                 ;compute overall parity along the way
    xor     ebx,edx
    mov         [FP_+ksKey +_NN_   ],ecx
    mov         [FP_+ksKey +_NN_+ 4],edx
_NN_ = _NN_+8
  endm
;
    mov         [FP_+ksKey +_NN_   ],eax ;save overall parity at the end of the array
    mov         [FP_+ksKey +_NN_+ 4],ebx

    mov     esi,[FP_+blkPtr ]       ;esi --> input block
;
_NN_ = WCNT*8-16                    ;work down from the end
  rept WCNT/2                       ;perform initial key injection
    mov     eax,[esi+_NN_       + 0]
    mov     ebx,[esi+_NN_       + 4]
    mov     ecx,[esi+_NN_       + 8]
    mov     edx,[esi+_NN_       +12]
    mov         [esp+_NN_+Wcopy + 0],eax
    mov         [esp+_NN_+Wcopy + 4],ebx
    mov         [esp+_NN_+Wcopy + 8],ecx
    mov         [esp+_NN_+Wcopy +12],edx
    add     eax,[FP_+_NN_+ksKey + 0]
    adc     ebx,[FP_+_NN_+ksKey + 4]
    add     ecx,[FP_+_NN_+ksKey + 8]
    adc     edx,[FP_+_NN_+ksKey +12]
   if     _NN_ eq (WCNT*8-16)       ;inject the tweak words
    add     eax,[FP_+     ksTwk + 8];   (at the appropriate points)
    adc     ebx,[FP_+     ksTwk +12]
   elseif _NN_ eq (WCNT*8-32)
    add     ecx,[FP_+     ksTwk + 0]
    adc     edx,[FP_+     ksTwk + 4]
   endif
   if _NN_ or _SKEIN_DEBUG
    mov         [esp+_NN_+X_stk + 0],eax
    mov         [esp+_NN_+X_stk + 4],ebx
    mov         [esp+_NN_+X_stk + 8],ecx
    mov         [esp+_NN_+X_stk +12],edx
   endif
_NN_ = _NN_ - 16                    ;end at X[0], so regs are already loaded for first MIX!
  endm
;
if _SKEIN_DEBUG                     ;debug dump of state at this point
    Skein_Debug_Block WCNT*64 
    Skein_Debug_Round WCNT*64,SKEIN_RND_KEY_INITIAL
endif
    add     esi, WCNT*8             ;skip the block
    mov         [FP_+blkPtr   ],esi ;update block pointer
    ;
    ; now the key schedule is computed. Start the rounds
    ;
if SKEIN_ASM_UNROLL and 512
_UNROLL_CNT =   ROUNDS_512/8
else
_UNROLL_CNT =   SKEIN_UNROLL_512
  if ((ROUNDS_512/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL_512"
  endif
    xor     edi,edi                 ;edi = round counter
Skein_512_round_loop:
endif
_Rbase_ = 0
rept _UNROLL_CNT*2
      ; here with X[0], X[1] already loaded into eax..edx
      R_512_FourRounds %(4*_Rbase_+00),

      ;inject odd  key schedule words
  if _UNROLL_CNT ne (ROUNDS_512/8)
      ks_Rotate eax,ebx,WCNT
      inc   edi                     ;edi = round number
  endif
_Rbase_ = _Rbase_+1
      ks_Inject 512,7,7,eax,ebx,_Rbase_,7, ,_Rbase_
      ks_Inject 512,6,6,eax,ebx,_Rbase_,6,1
      ks_Inject 512,5,5,eax,ebx,_Rbase_,5,0
      ks_Inject 512,4,4,eax,ebx,_Rbase_,4
      ks_Inject 512,3,3,eax,ebx,_Rbase_,3
      ks_Inject 512,2,2,eax,ebx,_Rbase_,2
      ks_Inject 512, , ,ecx,edx,_Rbase_,1
      ks_Inject 512,0, ,eax,ebx,_Rbase_,0
  if _SKEIN_DEBUG
      Skein_Debug_Round 512,SKEIN_RND_KEY_INJECT ,saveRegs
  endif
endm ;rept _UNROLL_CNT
;
if (SKEIN_ASM_UNROLL and 512) eq 0
    cmp     edi,2*(ROUNDS_512/8)
    jb      Skein_512_round_loop
    mov     edi,[FP_+ctxPtr ]           ;restore edi --> context
endif
    ;----------------------------
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..7}
_NN_ = 0
 rept WCNT/2
   if _NN_  ;eax..edx already loaded the first time
    mov     eax,[esp+X_stk + _NN_ + 0]
    mov     ebx,[esp+X_stk + _NN_ + 4]
    mov     ecx,[esp+X_stk + _NN_ + 8]
    mov     edx,[esp+X_stk + _NN_ +12]
   endif
   if _NN_ eq 0
    and     dword ptr [edi + TWEAK+12],FIRST_MASK
   endif
    xor     eax,[esp+Wcopy + _NN_ + 0]
    xor     ebx,[esp+Wcopy + _NN_ + 4]
    xor     ecx,[esp+Wcopy + _NN_ + 8]
    xor     edx,[esp+Wcopy + _NN_ +12]
    mov         [edi+X_VARS+ _NN_ + 0],eax
    mov         [edi+X_VARS+ _NN_ + 4],ebx
    mov         [edi+X_VARS+ _NN_ + 8],ecx
    mov         [edi+X_VARS+ _NN_ +12],edx
_NN_ = _NN_+16
  endm
if _SKEIN_DEBUG
    Skein_Debug_Round 512,SKEIN_RND_FEED_FWD
endif
    ; go back for more blocks, if needed
    dec     dword ptr [FP_+blkCnt]
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
;;;;;;;;;;;;;;;;;
; MACRO: four rounds
;
R_1024_FourRounds macro _RR_,ld_0
    ; here with edx:ecx = X[1]

    ;--------- round _RR_
    MixStep 1024, ld_0,  , 0, 1,((_RR_)+0),0
    MixStep 1024,    2, 3, 2, 3,((_RR_)+0),1
    MixStep 1024,    4, 5, 4, 5,((_RR_)+0),2
    MixStep 1024,    6, 7, 6, 7,((_RR_)+0),3
    MixStep 1024,    8, 9, 8, 9,((_RR_)+0),4
    MixStep 1024,   10,11,10,11,((_RR_)+0),5
    MixStep 1024,   12,13,12,13,((_RR_)+0),6
    MixStep 1024,   14,15,14,  ,((_RR_)+0),7,1
    ; here with edx:ecx = X[15]

    ;--------- round _RR_+1
    MixStep 1024,    4,  , 4,15,((_RR_)+1),3
    MixStep 1024,    0, 9, 0, 9,((_RR_)+1),0
    MixStep 1024,    2,13, 2,13,((_RR_)+1),1
    MixStep 1024,    6,11, 6,11,((_RR_)+1),2
    MixStep 1024,   10, 7,10, 7,((_RR_)+1),4
    MixStep 1024,   12, 3,12, 3,((_RR_)+1),5
    MixStep 1024,   14, 5,14, 5,((_RR_)+1),6
    MixStep 1024,    8, 1, 8,  ,((_RR_)+1),7,1
    ; here with edx:ecx = X[1]

    ;--------- round _RR_+2
    MixStep 1024,    6,  , 6, 1,((_RR_)+2),3    
    MixStep 1024,    0, 7, 0, 7,((_RR_)+2),0    
    MixStep 1024,    2, 5, 2, 5,((_RR_)+2),1
    MixStep 1024,    4, 3, 4, 3,((_RR_)+2),2    
    MixStep 1024,   12,15,12,15,((_RR_)+2),4
    MixStep 1024,   14,13,14,13,((_RR_)+2),5    
    MixStep 1024,    8,11, 8,11,((_RR_)+2),6    
    MixStep 1024,   10, 9,10,  ,((_RR_)+2),7,1
    ; here with edx:ecx = X[9]

    ;--------- round _RR_+3
    MixStep 1024,    4,  , 4, 9,((_RR_)+3),3
    MixStep 1024,    0,15, 0,15,((_RR_)+3),0
    MixStep 1024,    2,11, 2,11,((_RR_)+3),1
    MixStep 1024,    6,13, 6,13,((_RR_)+3),2
    MixStep 1024,    8, 5, 8, 5,((_RR_)+3),5
    MixStep 1024,   10, 3,10, 3,((_RR_)+3),6
    MixStep 1024,   12, 7,12, 7,((_RR_)+3),7
    MixStep 1024,   14, 1,14,  ,((_RR_)+3),4,1

    ; here with edx:ecx = X[1]
endm ;R_1024_FourRounds
;
;;;;;;;;;;;;;;;;;
; code
;
_Skein1024_Process_Block proc near
;
    WCNT    =   16                   ;WCNT=16 for Skein-1024
    Setup_Stack WCNT,(ROUNDS_1024/8)

    ; main hash loop for Skein1024
Skein1024_block_loop:
    mov     eax,[edi+TWEAK+ 0]      ;ebx:eax = tweak word T0
    mov     ebx,[edi+TWEAK+ 4]
    mov     ecx,[edi+TWEAK+ 8]      ;edx:ecx = tweak word T1
    mov     edx,[edi+TWEAK+12]

    add     eax,[FP_+bitAdd  ]      ;bump T0 by the bitAdd parameter
    adc     ebx, 0
    mov         [edi+TWEAK   ],eax  ;save updated tweak value T0
    mov         [edi+TWEAK+ 4],ebx

    mov         [FP_+ksTwk   ],eax  ;build the tweak schedule on the stack
    mov         [FP_+ksTwk+ 4],ebx
    xor     eax,ecx                 ;ebx:eax = T0 ^ T1
    xor     ebx,edx
    mov         [FP_+ksTwk+ 8],ecx
    mov         [FP_+ksTwk+12],edx
    mov         [FP_+ksTwk+16],eax
    mov         [FP_+ksTwk+20],ebx

    mov     eax,KW_PARITY_LO        ;init parity accumulator
    mov     ebx,KW_PARITY_HI
EDI_BIAS    equ 70h                 ;bias the edi offsets to make them short!
    add     edi, EDI_BIAS
CT_ equ     <edi-EDI_BIAS>
;
_NN_ = 0
  rept WCNT                         ;copy in the chaining vars
    mov     ecx,[CT_+X_VARS+_NN_   ]
    mov     edx,[CT_+X_VARS+_NN_+ 4]
    xor     eax,ecx                 ;compute overall parity along the way
    xor     ebx,edx
    mov         [FP_+ksKey +_NN_   ],ecx
    mov         [FP_+ksKey +_NN_+ 4],edx
_NN_ = _NN_+8
  endm
;
    mov         [FP_+ksKey +_NN_   ],eax ;save overall parity at the end of the array
    mov         [FP_+ksKey +_NN_+ 4],ebx

    mov     esi,[FP_+blkPtr ]       ;esi --> input block
    lea     edi,[esp+Wcopy]
;
_NN_ = WCNT*8-16                    ;work down from the end
  rept WCNT/2                       ;perform initial key injection
    mov     eax,[esi+_NN_       + 0]
    mov     ebx,[esi+_NN_       + 4]
    mov     ecx,[esi+_NN_       + 8]
    mov     edx,[esi+_NN_       +12]
    mov         [edi+_NN_+      + 0],eax
    mov         [edi+_NN_+      + 4],ebx
    mov         [edi+_NN_+      + 8],ecx
    mov         [edi+_NN_+      +12],edx
    add     eax,[FP_+_NN_+ksKey + 0]
    adc     ebx,[FP_+_NN_+ksKey + 4]
    add     ecx,[FP_+_NN_+ksKey + 8]
    adc     edx,[FP_+_NN_+ksKey +12]
   if     _NN_ eq (WCNT*8-16)       ;inject the tweak words
    add     eax,[FP_+     ksTwk + 8];   (at the appropriate points)
    adc     ebx,[FP_+     ksTwk +12]
   elseif _NN_ eq (WCNT*8-32)
    add     ecx,[FP_+     ksTwk + 0]
    adc     edx,[FP_+     ksTwk + 4]
   endif
   if _NN_ or _SKEIN_DEBUG
    mov         [esp+_NN_+X_stk + 0],eax
    mov         [esp+_NN_+X_stk + 4],ebx
    mov         [esp+_NN_+X_stk + 8],ecx
    mov         [esp+_NN_+X_stk +12],edx
   endif
_NN_ = _NN_ - 16                    ;end at X[0], so regs are already loaded for first MIX!
  endm
;
if _SKEIN_DEBUG                     ;debug dump of state at this point
    Skein_Debug_Block WCNT*64 
    Skein_Debug_Round WCNT*64,SKEIN_RND_KEY_INITIAL
endif
    sub     esi,-WCNT*8             ;skip the block (short immediate)
    mov         [FP_+blkPtr   ],esi ;update block pointer
    ;
    ; now the key schedule is computed. Start the rounds
    ;
if SKEIN_ASM_UNROLL and 1024
_UNROLL_CNT =   ROUNDS_1024/8
else
_UNROLL_CNT =   SKEIN_UNROLL_1024
  if ((ROUNDS_1024/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL_1024"
  endif
    xor     edi,edi                 ;edi = round counter
Skein_1024_round_loop:
endif

_Rbase_ = 0
rept _UNROLL_CNT*2
      ; here with X[0], X[1] already loaded into eax..edx
      R_1024_FourRounds %(4*_Rbase_+00),

      ;inject odd  key schedule words
      ;inject odd  key schedule words
  if _UNROLL_CNT ne (ROUNDS_1024/8)
      ks_Rotate eax,ebx,WCNT
      inc   edi                     ;edi = round number
  endif
_Rbase_ = _Rbase_+1
      ks_Inject 1024,15,15,eax,ebx,_Rbase_,15, ,_Rbase_
      ks_Inject 1024,14,14,eax,ebx,_Rbase_,14,1
      ks_Inject 1024,13,13,eax,ebx,_Rbase_,13,0
  irp _w,<12,11,10,9,8,7,6,5,4,3,2>
      ks_Inject 1024,_w,_w,eax,ebx,_Rbase_,_w
  endm
      ks_Inject 1024,  ,  ,ecx,edx,_Rbase_,1
      ks_Inject 1024, 0,  ,eax,ebx,_Rbase_,0

  if _SKEIN_DEBUG
      Skein_Debug_Round 1024,SKEIN_RND_KEY_INJECT ,saveRegs
  endif
endm ;rept _UNROLL_CNT
;
if (SKEIN_ASM_UNROLL and 1024) eq 0
    cmp     edi,2*(ROUNDS_1024/8)
    jb      Skein_1024_round_loop
endif
    mov     edi,[FP_+ctxPtr ]           ;restore edi --> context
    add     edi,EDI_BIAS                ;and bias it for short offsets below
    ;----------------------------
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..15}
    lea     esi,[esp+Wcopy]             ;use short offsets below
_NN_ = 0
 rept WCNT/2
   if _NN_  ;eax..edx already loaded the first time
    mov     eax,[esp+X_stk + _NN_ + 0]
    mov     ebx,[esp+X_stk + _NN_ + 4]
    mov     ecx,[esp+X_stk + _NN_ + 8]
    mov     edx,[esp+X_stk + _NN_ +12]
   endif
   if _NN_ eq 0
    and     dword ptr [CT_ + TWEAK+12],FIRST_MASK
   endif
    xor     eax,[esi       + _NN_ + 0]
    xor     ebx,[esi       + _NN_ + 4]
    xor     ecx,[esi       + _NN_ + 8]
    xor     edx,[esi       + _NN_ +12]
    mov         [CT_+X_VARS+ _NN_ + 0],eax
    mov         [CT_+X_VARS+ _NN_ + 4],ebx
    mov         [CT_+X_VARS+ _NN_ + 8],ecx
    mov         [CT_+X_VARS+ _NN_ +12],edx
_NN_ = _NN_+16
  endm
    sub     edi,EDI_BIAS                ;undo the bias for return

if _SKEIN_DEBUG
    Skein_Debug_Round 1024,SKEIN_RND_FEED_FWD
endif
    ; go back for more blocks, if needed
    dec     dword ptr [FP_+blkCnt]
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
