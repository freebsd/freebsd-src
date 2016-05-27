;
;----------------------------------------------------------------
; 64-bit x86 assembler code (Microsoft ML64) for Skein block functions
;
; Author: Doug Whiting, Hifn
;
; This code is released to the public domain.
;----------------------------------------------------------------
;
    .code
;
_MASK_ALL_  equ (256+512+1024)      ;all three algorithm bits
_MAX_FRAME_ equ 240
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
ifndef SKEIN_LOOP                           ;configure loop unrolling
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
;
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
ifndef SKEIN_CODE_SIZE
ifdef  SKEIN_PERF
SKEIN_CODE_SIZE equ (1)
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
HASH_BITS   =   0                   ;# bits of hash output
BCNT        =   8 + HASH_BITS       ;number of bytes in BUFFER[]
TWEAK       =   8 + BCNT            ;tweak values[0..1]
X_VARS      =  16 + TWEAK           ;chaining vars
;
;(Note: buffer[] in context structure is NOT needed here :-)
;
r08     equ     <r8>
r09     equ     <r9>
;
KW_PARITY   =   01BD11BDAA9FC1A22h  ;overall parity of key schedule words
FIRST_MASK  =   NOT (1 SHL 62)
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
;  Input:  reg
; Output: <reg> <<< RC_BlkSize_roundNum_mixNum, BlkSize=256/512/1024
;
RotL64 macro reg,BLK_SIZE,ROUND_NUM,MIX_NUM
_RCNT_ = ( RC_&BLK_SIZE&_&ROUND_NUM&_&MIX_NUM AND 63 )
  if _RCNT_  ;is there anything to do?
    rol     reg,_RCNT_
  endif
endm
;
;----------------------------------------------------------------
;
; MACROS: define local vars and configure stack
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
Setup_Stack macro BLK_BITS,KS_CNT,NO_FRAME,debugCnt
    WCNT    =    (BLK_BITS)/64
;
_PushCnt_   =   0                   ;save nonvolatile regs on stack
  irp _reg_,<rbp,rsi,rdi,rbx,r12,r13,r14,r15>
       push     _reg_
      .pushreg  _reg_               ;pseudo-op push for exception handling
_PushCnt_ = _PushCnt_ + 1           ;track count to keep alignment
  endm
;
_STK_OFFS_  =   0                   ;starting offset from rsp
    ;---- local  variables         ;<-- rsp
    StackVar    X_stk  ,8*(WCNT)    ;local context vars
    StackVar    ksTwk  ,8*3         ;key schedule: tweak words
    StackVar    ksKey  ,8*(WCNT)+8  ;key schedule: key   words
  if (SKEIN_ASM_UNROLL and (BLK_BITS)) eq 0
    StackVar    ksRot ,16*(KS_CNT+0);leave space for "rotation" to happen
  endif
    StackVar    Wcopy  ,8*(WCNT)    ;copy of input block    
  if _SKEIN_DEBUG
  ifnb  <debugCnt>                  ;temp location for debug X[] info
    StackVar    xDebug_&BLK_BITS ,8*(debugCnt)
  endif
  endif
  if ((8*_PushCnt_ + _STK_OFFS_) and 8) eq 0
    StackVar    align16,8           ;keep 16-byte aligned (adjust for retAddr?)
tmpStk_&BLK_BITS = align16          ;use this
  endif
LOCAL_SIZE  =   _STK_OFFS_          ;size of local vars
    ;---- 
    StackVar    savRegs,8*_PushCnt_ ;saved registers
    StackVar    retAddr,8           ;return address
    ;---- caller parameters
    StackVar    ctxPtr ,8           ;context ptr
    StackVar    blkPtr ,8           ;pointer to block data
    StackVar    blkCnt ,8           ;number of full blocks to process
    StackVar    bitAdd ,8           ;bit count to add to tweak
    ;---- caller's stack frame
;
; set up the stack frame pointer (rbp)
;
FRAME_OFFS  =   ksTwk + 128         ;allow short (negative) offset to ksTwk, kwKey
  if FRAME_OFFS gt _STK_OFFS_       ;keep rbp in the "locals" range
FRAME_OFFS  =      _STK_OFFS_
  endif
  if FRAME_OFFS gt _MAX_FRAME_      ;keep Microsoft .setframe happy
FRAME_OFFS  =      _MAX_FRAME_
  endif
;
ifdef SKEIN_ASM_INFO
  if     FRAME_OFFS+128 lt savRegs
%out +++ SKEIN_&BLK_BITS: Unable to reach all of Wcopy with short offset from rbp.
  elseif FRAME_OFFS+128 lt Wcopy
%out +++ SKEIN_&BLK_BITS: Unable to reach end of Wcopy with short offset from rbp.
  elseif FRAME_OFFS+128 lt _STK_OFFS_
%out +++ SKEIN_&BLK_BITS: Unable to reach caller parms with short offset from rbp
  endif
endif
  ;put some useful defines in the .lst file (for grep)
__STK_LCL_SIZE_&BLK_BITS = LOCAL_SIZE
__STK_TOT_SIZE_&BLK_BITS = _STK_OFFS_
__STK_FRM_OFFS_&BLK_BITS = FRAME_OFFS
;
; Notes on stack frame setup:
;   * the most frequently used variable is X_stk[], based at [rsp+0]
;   * the next most used is the key schedule arrays, ksKey and ksTwk
;       so rbp is "centered" there, allowing short offsets to the key 
;       schedule even in 1024-bit Skein case
;   * the Wcopy variables are infrequently accessed, but they have long 
;       offsets from both rsp and rbp only in the 1024-bit case.
;   * all other local vars and calling parameters can be accessed 
;       with short offsets, except in the 1024-bit case
;
    sub     rsp,LOCAL_SIZE          ;make room for the locals
    .allocstack LOCAL_SIZE          ;pseudo op for exception handling
    lea     rbp,[rsp+FRAME_OFFS]    ;maximize use of short offsets
  ifb <NO_FRAME>
    .setframe rbp,   FRAME_OFFS     ;pseudo op for exception handling
  endif
    mov         [FP_+ctxPtr],rcx    ;save caller's parameters on the stack
    mov         [FP_+blkPtr],rdx
    mov         [FP_+blkCnt],r08
    mov         [FP_+bitAdd],r09
    .endprolog                      ;pseudo op to support exception handling

    mov     rdi,[FP_+ctxPtr ]       ;rdi --> context
;
endm ;Setup_Stack
;
FP_         equ <rbp-FRAME_OFFS>    ;keep as many short offsets as possible
;
;----------------------------------------------------------------
;
Reset_Stack macro   procStart
    add     rsp,LOCAL_SIZE          ;get rid of locals (wipe??)
  irp _reg_,<r15,r14,r13,r12,rbx,rdi,rsi,rbp>
    pop     _reg_
_PushCnt_ = _PushCnt_ - 1
  endm
  if _PushCnt_
    .err    "Mismatched push/pops?"
  endif

    ;display code size in bytes to stdout
  irp  _BCNT_,<%($+1-procStart)>    ;account for return opcode
_ProcBytes_ = _BCNT_
if     _BCNT_ ge 10000
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
    extrn   Skein_Show_Block:proc   ;calls to C routines
    extrn   Skein_Show_Round:proc
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
  irp _reg_,<rax,rcx,rdx,r08,r09,r10,r11>
    push    _reg_                   ;save all volatile regs on tack before the call
  endm
    ; get and push call parameters
    lea     rax,[FP_+ksTwk]         ;tweak pointer
    push    rax
    lea     rax,[FP_+ksKey]         ;key pointer
    push    rax
    lea     rax,[FP_+Wcopy]         ;wPtr
    push    rax
    mov     r09,[FP_+blkPtr]        ;blkPtr
    push    r09                     ;(push register parameters anyway to make room on stack)
    mov     rdx,[FP_+ctxPtr]        
    lea     r08,[rdx+X_VARS]        ;X (pointer)
    push    r08
    push    rdx                     ;h (pointer)
    mov     rcx, BLK_BITS           ;bits
    push    rdx
    call    Skein_Show_Block        ;call external debug handler
    add     rsp,7*8                 ;discard parameters on stack
  irp _reg_,<r11,r10,r09,r08,rdx,rcx,rax>
    pop     _reg_                   ;restore regs
  endm
endm ; Skein_Debug_Block
;
;
; the macro to "call" to debug a round
;
Skein_Debug_Round macro BLK_BITS,R,RDI_OFFS,afterOp
    ; call the appropriate (local) debug function
    push    r08
  if (SKEIN_ASM_UNROLL and BLK_BITS) or (R ge SKEIN_RND_SPECIAL)
    mov     r08, R
  else                              ;compute round number using edi
_rOffs_ = RDI_OFFS + 0
   if BLK_BITS eq 1024
    mov     r08,[rsp+8+rIdx_offs]   ;get rIdx off the stack (adjust for push r08)
    lea     r08,[4*r08+1+(((R)-1) and 3)+_rOffs_]
   else
    lea     r08,[4*rdi+1+(((R)-1) and 3)+_rOffs_]
   endif
  endif
    call    Skein_Debug_Round_&BLK_BITS
    pop     r08
;
  afterOp
endm  ;  Skein_Debug_Round
else  ;------- _SKEIN_DEBUG (dummy macros if debug not enabled)
Skein_Debug_Block macro BLK_BITS,afterOp
endm
;
Skein_Debug_Round macro BLK_BITS,R,RDI_OFFS,afterOp
endm
;
endif ; _SKEIN_DEBUG
;
;----------------------------------------------------------------
;
addReg  macro   dstReg,srcReg_A,srcReg_B,useAddOp,immOffs
  ifnb <immOffs>
       lea     dstReg,[srcReg_A&&srcReg_B + dstReg + immOffs]
  elseif ((useAddOp + 0) eq 0)
    ifndef ASM_NO_LEA
      ;lea seems to be faster on Core 2 Duo CPUs!
       lea     dstReg,[srcReg_A&&srcReg_B + dstReg]   
    else
       add     dstReg, srcReg_A&&srcReg_B
    endif
  else
       add     dstReg, srcReg_A&&srcReg_B
  endif
endm
;
;=================================== Skein_256 =============================================
;
if _USE_ASM_ and 256
    public  Skein_256_Process_Block
;
; void Skein_256_Process_Block(Skein_256_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd);
;
;;;;;;;;;;;;;;;;;
;
; code
;
Skein_256_Process_Block proc frame
    Setup_Stack 256,((ROUNDS_256/8)+1)
    mov     r14,[rdi+TWEAK+8]
    jmp   short Skein_256_block_loop
    align   16
    ; main hash loop for Skein_256
Skein_256_block_loop:
    ;
    ; general register usage:
    ;   RAX..RDX        = X0..X3    
    ;   R08..R12        = ks[0..4]
    ;   R13..R15        = ts[0..2]
    ;   RSP, RBP        = stack/frame pointers
    ;   RDI             = round counter or context pointer
    ;   RSI             = temp
    ;
    mov     r13,[rdi+TWEAK+0]
    add     r13,[FP_+bitAdd]        ;computed updated tweak value T0
    mov     r15,r14
    xor     r15,r13                 ;now r13.r15 is set as the tweak 

    mov     r12,KW_PARITY
    mov     r08,[rdi+X_VARS+ 0]
    mov     r09,[rdi+X_VARS+ 8]
    mov     r10,[rdi+X_VARS+16]
    mov     r11,[rdi+X_VARS+24]
    mov         [rdi+TWEAK+0],r13   ;save updated tweak value ctx->h.T[0]
    xor     r12,r08                 ;start accumulating overall parity

    mov     rsi,[FP_+blkPtr ]       ;esi --> input block
    xor     r12,r09
    mov     rax,[rsi+ 0]            ;get X[0..3]
    xor     r12,r10
    mov     rbx,[rsi+ 8]
    xor     r12,r11
    mov     rcx,[rsi+16]
    mov     rdx,[rsi+24]

    mov         [FP_+Wcopy+ 0],rax  ;save copy of input block
    mov         [FP_+Wcopy+ 8],rbx
    mov         [FP_+Wcopy+16],rcx
    mov         [FP_+Wcopy+24],rdx

    add     rax, r08                ;initial key injection
    add     rbx, r09
    add     rcx, r10
    add     rdx, r11
    add     rbx, r13
    add     rcx, r14

if _SKEIN_DEBUG
    mov         [rdi+TWEAK+ 8],r14  ;save updated tweak T[1] (start bit cleared?)
    mov         [FP_+ksKey+ 0],r08  ;save key schedule on stack for Skein_Debug_Block
    mov         [FP_+ksKey+ 8],r09
    mov         [FP_+ksKey+16],r10
    mov         [FP_+ksKey+24],r11
    mov         [FP_+ksKey+32],r12

    mov         [FP_+ksTwk+ 0],r13
    mov         [FP_+ksTwk+ 8],r14
    mov         [FP_+ksTwk+16],r15

    mov         [rsp+X_stk + 0],rax ;save X[] on stack for Skein_Debug_Block
    mov         [rsp+X_stk + 8],rbx
    mov         [rsp+X_stk +16],rcx
    mov         [rsp+X_stk +24],rdx

    Skein_Debug_Block 256           ;debug dump
    Skein_Debug_Round 256,SKEIN_RND_KEY_INITIAL
endif
;
if ((SKEIN_ASM_UNROLL and 256) eq 0)
    mov         [FP_+ksKey+40],r08 ;save key schedule on stack for looping code
    mov         [FP_+ksKey+ 8],r09
    mov         [FP_+ksKey+16],r10
    mov         [FP_+ksKey+24],r11
    mov         [FP_+ksKey+32],r12

    mov         [FP_+ksTwk+24],r13
    mov         [FP_+ksTwk+ 8],r14
    mov         [FP_+ksTwk+16],r15
endif
    add     rsi, WCNT*8             ;skip the block
    mov         [FP_+blkPtr   ],rsi ;update block pointer
;
opLoop macro op1,op2
  if (SKEIN_ASM_UNROLL and 256) eq 0
    op1
  else
    op2
  endif
endm
;
    ;
    ; now the key schedule is computed. Start the rounds
    ;
if SKEIN_ASM_UNROLL and 256
_UNROLL_CNT =   ROUNDS_256/8
else
_UNROLL_CNT =   SKEIN_UNROLL_256
  if ((ROUNDS_256/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL_256"
  endif
    xor     rdi,rdi                   ;rdi = iteration count
Skein_256_round_loop:
endif
_Rbase_ = 0
rept _UNROLL_CNT*2
    ; all X and ks vars in regs     ; (ops to "rotate" ks vars, via mem, if not unrolled)
    ; round 4*_RBase_ + 0
    addReg  rax, rbx
    RotL64  rbx, 256,%((4*_RBase_+0) and 7),0
    addReg  rcx, rdx
                    opLoop  <mov r08,[FP_+ksKey+8*rdi+8*1]>
    xor     rbx, rax
    RotL64  rdx, 256,%((4*_RBase_+0) and 7),1
    xor     rdx, rcx
 if SKEIN_ASM_UNROLL and 256
    irp _r0_,<%(08+(_Rbase_+3) mod 5)>
    irp _r1_,<%(13+(_Rbase_+2) mod 3)>
      lea   rdi,[r&_r0_+r&_r1_]     ;precompute key injection value for rcx
    endm
    endm
 endif
                    opLoop  <mov r13,[FP_+ksTwk+8*rdi+8*1]>
    Skein_Debug_Round 256,%(4*_RBase_+1)

    ; round 4*_RBase_ + 1
    addReg  rax, rdx
    RotL64  rdx, 256,%((4*_RBase_+1) and 7),0
    xor     rdx, rax
                    opLoop  <mov r09,[FP_+ksKey+8*rdi+8*2]>
    addReg  rcx, rbx
    RotL64  rbx, 256,%((4*_RBase_+1) and 7),1
    xor     rbx, rcx
                    opLoop  <mov r11,[FP_+ksKey+8*rdi+8*4]>
    Skein_Debug_Round 256,%(4*_RBase_+2)
 if SKEIN_ASM_UNROLL and 256
    irp _r0_,<%(08+(_Rbase_+2) mod 5)>
    irp _r1_,<%(13+(_Rbase_+1) mod 3)>
      lea   rsi,[r&_r0_+r&_r1_]     ;precompute key injection value for rbx
    endm
    endm
 endif
    ; round 4*_RBase_ + 2
    addReg  rax, rbx
    RotL64  rbx, 256,%((4*_RBase_+2) and 7),0
    addReg  rcx, rdx
                    opLoop  <mov r10,[FP_+ksKey+8*rdi+8*3]>
    xor     rbx, rax
    RotL64  rdx, 256,%((4*_RBase_+2) and 7),1
    xor     rdx, rcx
                    opLoop  <mov     [FP_+ksKey+8*rdi+8*6],r08> ;"rotate" the key
                    opLoop  <lea r11,[r11+rdi+1]>   ;precompute key + tweak
    Skein_Debug_Round 256,%(4*_RBase_+3)
    ; round 4*_RBase_ + 3
    addReg  rax, rdx
    RotL64  rdx, 256,%((4*_RBase_+3) and 7),0
    addReg  rcx, rbx
                    opLoop  <add r10,[FP_+ksTwk+8*rdi+8*2]>    ;precompute key + tweak
                    opLoop  <mov     [FP_+ksTwk+8*rdi+8*4],r13> ;"rotate" the tweak
    xor     rdx, rax
    RotL64  rbx, 256,%((4*_RBase_+3) and 7),1
    xor     rbx, rcx
    Skein_Debug_Round 256,%(4*_RBase_+4)
                    opLoop  <addReg r09,r13>    ;precompute key+tweak
      ;inject key schedule words
_Rbase_ = _Rbase_+1
  if SKEIN_ASM_UNROLL and 256
      addReg    rax,r,%(08+((_Rbase_+0) mod 5))
      addReg    rbx,rsi
      addReg    rcx,rdi
      addReg    rdx,r,%(08+((_Rbase_+3) mod 5)),,_Rbase_
  else
      inc       rdi
      addReg    rax,r08
      addReg    rcx,r10
      addReg    rbx,r09
      addReg    rdx,r11
  endif
      Skein_Debug_Round 256,SKEIN_RND_KEY_INJECT
endm ;rept _UNROLL_CNT

;
if (SKEIN_ASM_UNROLL and 256) eq 0
    cmp     rdi,2*(ROUNDS_256/8)
    jb      Skein_256_round_loop
endif ; (SKEIN_ASM_UNROLL and 256) eq 0
    mov     rdi,[FP_+ctxPtr ]           ;restore edi --> context

    ;----------------------------
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..3}
    xor     rax,[FP_+Wcopy + 0]
    mov     r14,FIRST_MASK
    xor     rbx,[FP_+Wcopy + 8]
    xor     rcx,[FP_+Wcopy +16]
    xor     rdx,[FP_+Wcopy +24]
    mov         [rdi+X_VARS+ 0],rax     ;store final result
    and     r14,[rdi+TWEAK + 8]
    dec     qword ptr [FP_+blkCnt]      ;set zero flag
    mov         [rdi+X_VARS+ 8],rbx
    mov         [rdi+X_VARS+16],rcx
    mov         [rdi+X_VARS+24],rdx

    Skein_Debug_Round 256,SKEIN_RND_FEED_FWD,,<cmp qword ptr [FP_+blkCnt],0>

    ; go back for more blocks, if needed
    jnz     Skein_256_block_loop
    mov         [rdi+TWEAK + 8],r14
    Reset_Stack Skein_256_Process_Block
    ret

  if _SKEIN_DEBUG
Skein_Debug_Round_256:
    mov         [FP_+X_stk+ 0],rax  ;first, save X[] state on stack so debug routines can access it
    mov         [FP_+X_stk+ 8],rbx  ;(use FP_ since rsp has changed!)
    mov         [FP_+X_stk+16],rcx
    mov         [FP_+X_stk+24],rdx
    push    rdx                     ;save two regs for BLK_BITS-specific parms
    push    rcx
    mov     rdx,[FP_+ctxPtr]        ;ctx_hdr_ptr
    mov     rcx, 256
    jmp     Skein_Debug_Round_Common
  endif

Skein_256_Process_Block endp
;
ifdef SKEIN_CODE_SIZE
    public  Skein_256_Process_Block_CodeSize
Skein_256_Process_Block_CodeSize proc
    mov     rax,_ProcBytes_
    ret
Skein_256_Process_Block_CodeSize endp
;
    public  Skein_256_Unroll_Cnt
Skein_256_Unroll_Cnt proc
  if _UNROLL_CNT ne ROUNDS_256/8
    mov     rax,_UNROLL_CNT
  else
    xor     rax,rax
  endif
    ret
Skein_256_Unroll_Cnt endp
endif
;
endif ;_USE_ASM_ and 256
;
;=================================== Skein_512 =============================================
;
if _USE_ASM_ and 512
    public  Skein_512_Process_Block
;
; void Skein_512_Process_Block(Skein_512_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd);
;
rX_512_0    equ r08         ;register assignments for X[] values during rounds
rX_512_1    equ r09
rX_512_2    equ r10
rX_512_3    equ r11
rX_512_4    equ r12
rX_512_5    equ r13
rX_512_6    equ r14
rX_512_7    equ r15
;
;;;;;;;;;;;;;;;;;
; MACRO: one round for 512-bit blocks
;
R_512_OneRound  macro r0,r1,r2,r3,r4,r5,r6,r7,_Rn_,op1,op2,op3,op4
;
    addReg      rX_512_&r0, rX_512_&r1
    RotL64      rX_512_&r1, 512,%((_Rn_) and 7),0
    xor         rX_512_&r1, rX_512_&r0
            op1
    addReg      rX_512_&r2, rX_512_&r3
    RotL64      rX_512_&r3, 512,%((_Rn_) and 7),1
    xor         rX_512_&r3, rX_512_&r2
            op2
    addReg      rX_512_&r4, rX_512_&r5
    RotL64      rX_512_&r5, 512,%((_Rn_) and 7),2
    xor         rX_512_&r5, rX_512_&r4
            op3
    addReg      rX_512_&r6, rX_512_&r7
    RotL64      rX_512_&r7, 512,%((_Rn_) and 7),3
    xor         rX_512_&r7, rX_512_&r6
            op4
    Skein_Debug_Round 512,%(_Rn_+1),-4
;
endm ;R_512_OneRound
;
;;;;;;;;;;;;;;;;;
; MACRO: eight rounds for 512-bit blocks
;
R_512_FourRounds macro _RR_    ;RR = base round number (0 mod 8)
  if SKEIN_ASM_UNROLL and 512
    ; here for fully unrolled case.
    _II_ = ((_RR_)/4) + 1       ;key injection counter
    R_512_OneRound 0,1,2,3,4,5,6,7,%((_RR_)+0),<mov rax,[FP_+ksKey+8*(((_II_)+3) mod 9)]>,,<mov rbx,[FP_+ksKey+8*(((_II_)+4) mod 9)]>
    R_512_OneRound 2,1,4,7,6,5,0,3,%((_RR_)+1),<mov rcx,[FP_+ksKey+8*(((_II_)+5) mod 9)]>,,<mov rdx,[FP_+ksKey+8*(((_II_)+6) mod 9)]>
    R_512_OneRound 4,1,6,3,0,5,2,7,%((_RR_)+2),<mov rsi,[FP_+ksKey+8*(((_II_)+7) mod 9)]>,,<add rcx,[FP_+ksTwk+8*(((_II_)+0) mod 3)]>
    R_512_OneRound 6,1,0,7,2,5,4,3,%((_RR_)+3),<add rdx,[FP_+ksTwk+8*(((_II_)+1) mod 3)]>,
    ; inject the key schedule
    add     r08,[FP_+ksKey+8*(((_II_)+0) mod 9)]
    addReg  r11,rax
    add     r09,[FP_+ksKey+8*(((_II_)+1) mod 9)]
    addReg  r12,rbx
    add     r10,[FP_+ksKey+8*(((_II_)+2) mod 9)]
    addReg  r13,rcx
    addReg  r14,rdx
    addReg  r15,rsi,,,(_II_)
  else
    ; here for looping case                                                    ;"rotate" key/tweak schedule (move up on stack)
    inc     rdi                 ;bump key injection counter
    R_512_OneRound 0,1,2,3,4,5,6,7,%((_RR_)+0),<mov rdx,[FP_+ksKey+8*rdi+8*6]>,<mov rax,[FP_+ksTwk+8*rdi-8*1]>    ,<mov rsi,[FP_+ksKey+8*rdi-8*1]>
    R_512_OneRound 2,1,4,7,6,5,0,3,%((_RR_)+1),<mov rcx,[FP_+ksKey+8*rdi+8*5]>,<mov     [FP_+ksTwk+8*rdi+8*2],rax>,<mov     [FP_+ksKey+8*rdi+8*8],rsi>
    R_512_OneRound 4,1,6,3,0,5,2,7,%((_RR_)+2),<mov rbx,[FP_+ksKey+8*rdi+8*4]>,<add rdx,[FP_+ksTwk+8*rdi+8*1]>    ,<mov rsi,[FP_+ksKey+8*rdi+8*7]>    
    R_512_OneRound 6,1,0,7,2,5,4,3,%((_RR_)+3),<mov rax,[FP_+ksKey+8*rdi+8*3]>,<add rcx,[FP_+ksTwk+8*rdi+8*0]>
    ; inject the key schedule
    add     r08,[FP_+ksKey+8*rdi+8*0]
    addReg  r11,rax
    addReg  r12,rbx
    add     r09,[FP_+ksKey+8*rdi+8*1]
    addReg  r13,rcx
    addReg  r14,rdx
    add     r10,[FP_+ksKey+8*rdi+8*2]
    addReg  r15,rsi
    addReg  r15,rdi              ;inject the round number
  endif
    ;show the result of the key injection
    Skein_Debug_Round 512,SKEIN_RND_KEY_INJECT
endm ;R_512_EightRounds
;
;;;;;;;;;;;;;;;;;
; instantiated code
;
Skein_512_Process_Block proc frame
    Setup_Stack 512,ROUNDS_512/8
    mov     rbx,[rdi+TWEAK+ 8]
    jmp   short Skein_512_block_loop
    align  16
    ; main hash loop for Skein_512
Skein_512_block_loop:
    ; general register usage:
    ;   RAX..RDX        = temps for key schedule pre-loads
    ;   R08..R15        = X0..X7
    ;   RSP, RBP        = stack/frame pointers
    ;   RDI             = round counter or context pointer
    ;   RSI             = temp
    ;
    mov     rax,[rdi+TWEAK+ 0]
    add     rax,[FP_+bitAdd]        ;computed updated tweak value T0
    mov     rcx,rbx
    xor     rcx,rax                 ;rax/rbx/rcx = tweak schedule
    mov         [rdi+TWEAK+ 0],rax  ;save updated tweak value ctx->h.T[0]
    mov         [FP_+ksTwk+ 0],rax
    mov     rdx,KW_PARITY
    mov     rsi,[FP_+blkPtr ]       ;rsi --> input block
    mov         [FP_+ksTwk+ 8],rbx
    mov         [FP_+ksTwk+16],rcx

    irp _Rn_,<0,1,2,3,4,5,6,7>
      mov   rX_512_&_Rn_,[rdi+X_VARS+8*(_Rn_)]
      xor   rdx,rX_512_&_Rn_        ;compute overall parity
      mov   [FP_+ksKey+8*(_Rn_)],rX_512_&_Rn_
    endm                            ;load state into r08..r15, compute parity
      mov   [FP_+ksKey+8*(8)],rdx   ;save key schedule parity

    addReg  rX_512_5,rax            ;precompute key injection for tweak
    addReg  rX_512_6,rbx
if _SKEIN_DEBUG
    mov         [rdi+TWEAK+ 8],rbx  ;save updated tweak value ctx->h.T[1] for Skein_Debug_Block below
endif
    mov     rax,[rsi+ 0]            ;load input block
    mov     rbx,[rsi+ 8]
    mov     rcx,[rsi+16]
    mov     rdx,[rsi+24]
    addReg  r08,rax                 ;do initial key injection
    addReg  r09,rbx
    mov         [FP_+Wcopy+ 0],rax  ;keep local copy for feedforward
    mov         [FP_+Wcopy+ 8],rbx
    addReg  r10,rcx
    addReg  r11,rdx
    mov         [FP_+Wcopy+16],rcx
    mov         [FP_+Wcopy+24],rdx

    mov     rax,[rsi+32]
    mov     rbx,[rsi+40]
    mov     rcx,[rsi+48]
    mov     rdx,[rsi+56]
    addReg  r12,rax
    addReg  r13,rbx
    addReg  r14,rcx
    addReg  r15,rdx
    mov         [FP_+Wcopy+32],rax
    mov         [FP_+Wcopy+40],rbx
    mov         [FP_+Wcopy+48],rcx
    mov         [FP_+Wcopy+56],rdx

if _SKEIN_DEBUG
    irp _Rn_,<0,1,2,3,4,5,6,7>      ;save values on stack for debug output
      mov       [rsp+X_stk+8*(_Rn_)],rX_512_&_Rn_
    endm

    Skein_Debug_Block 512           ;debug dump
    Skein_Debug_Round 512,SKEIN_RND_KEY_INITIAL
endif
    add     rsi, 8*WCNT             ;skip the block
    mov         [FP_+blkPtr   ],rsi ;update block pointer
    ;
    ;;;;;;;;;;;;;;;;;
    ; now the key schedule is computed. Start the rounds
    ;
if SKEIN_ASM_UNROLL and 512
_UNROLL_CNT =   ROUNDS_512/8
else
_UNROLL_CNT =   SKEIN_UNROLL_512
  if ((ROUNDS_512/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL_512"
  endif
    xor     rdi,rdi                 ;rdi = round counter
Skein_512_round_loop:
endif
;
_Rbase_ = 0
rept _UNROLL_CNT*2
      R_512_FourRounds %(4*_Rbase_+00)
_Rbase_ = _Rbase_+1
endm ;rept _UNROLL_CNT
;
if (SKEIN_ASM_UNROLL and 512) eq 0
    cmp     rdi,2*(ROUNDS_512/8)
    jb      Skein_512_round_loop
    mov     rdi,[FP_+ctxPtr ]           ;restore rdi --> context
endif
    ; end of rounds
    ;;;;;;;;;;;;;;;;;
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..7}
    irp _Rn_,<0,1,2,3,4,5,6,7>
  if (_Rn_ eq 0)
    mov     rbx,FIRST_MASK
  endif
      xor   rX_512_&_Rn_,[FP_+Wcopy+8*(_Rn_)]       ;feedforward XOR
      mov       [rdi+X_VARS+8*(_Rn_)],rX_512_&_Rn_  ;and store result
  if (_Rn_ eq 6)
    and     rbx,[rdi+TWEAK+ 8]
  endif
    endm
    Skein_Debug_Round 512,SKEIN_RND_FEED_FWD

    ; go back for more blocks, if needed
    dec     qword ptr [FP_+blkCnt]
    jnz     Skein_512_block_loop
    mov         [rdi+TWEAK + 8],rbx

    Reset_Stack Skein_512_Process_Block
    ret
;
  if _SKEIN_DEBUG
; call here with r08 = "round number"
Skein_Debug_Round_512:
    push    rdx                     ;save two regs for BLK_BITS-specific parms
    push    rcx
    mov     rcx,[rsp+24]            ;get back original r08 (pushed on stack in macro call)
    mov         [FP_+X_stk],rcx     ;and save it in X_stk
  irp _Rn_,<1,2,3,4,5,6,7>          ;save rest of X[] state on stack so debug routines can access it
    mov         [FP_+X_stk+8*(_Rn_)],rX_512_&_Rn_ 
  endm
    mov     rdx,[FP_+ctxPtr]        ;ctx_hdr_ptr
    mov     rcx, 512                ;block size
    jmp     Skein_Debug_Round_Common
  endif
;
Skein_512_Process_Block endp
;
ifdef SKEIN_CODE_SIZE
    public  Skein_512_Process_Block_CodeSize
Skein_512_Process_Block_CodeSize proc
    mov     rax,_ProcBytes_
    ret
Skein_512_Process_Block_CodeSize endp
;
    public  Skein_512_Unroll_Cnt
Skein_512_Unroll_Cnt proc
  if _UNROLL_CNT ne ROUNDS_512/8
    mov     rax,_UNROLL_CNT
  else
    xor     rax,rax
  endif
    ret
Skein_512_Unroll_Cnt endp
endif
;
endif ; _USE_ASM_ and 512
;
;=================================== Skein1024 =============================================
if _USE_ASM_ and 1024
    public  Skein1024_Process_Block
;
; void Skein1024_Process_Block(Skein_1024_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd);
;
;;;;;;;;;;;;;;;;;
; use details of permutation to make register assignments
;
r1K_x0    equ rdi 
r1K_x1    equ rsi
r1K_x2    equ rbp
r1K_x3    equ rax
r1K_x4    equ rcx           ;"shared" with X6, since X4/X6 alternate
r1K_x5    equ rbx
r1K_x6    equ rcx
r1K_x7    equ rdx
r1K_x8    equ r08
r1K_x9    equ r09
r1K_xA    equ r10
r1K_xB    equ r11
r1K_xC    equ r12
r1K_xD    equ r13
r1K_xE    equ r14
r1K_xF    equ r15
;
rIdx      equ r1K_x0        ;index register for looping versions
rIdx_offs equ tmpStk_1024
;
R1024_Mix  macro w0,w1,_RN0_,_Rn1_,op1
_w0  = 0&w0&h               ;handle the hex conversion
_w1  = 0&w1&h
_II_ = ((_RN0_)/4)+1        ;injection count
     ;
    addReg      r1K_x&w0 , r1K_x&w1                     ;perform the MIX
    RotL64      r1K_x&w1 , 1024,%((_RN0_) and 7),_Rn1_
    xor         r1K_x&w1 , r1K_x&w0
 if ((_RN0_) and 3) eq 3                                ;time to do key injection?
  if _SKEIN_DEBUG
    mov         [rsp+xDebug_1024+8*_w0],r1K_x&w0        ;save intermediate values for Debug_Round
    mov         [rsp+xDebug_1024+8*_w1],r1K_x&w1        ; (before inline key injection)
  endif
  if SKEIN_ASM_UNROLL and 1024  ;here to do fully unrolled key injection
    add         r1K_x&w0, [rsp+ksKey+      8*((_II_+_w0) mod 17)]
    add         r1K_x&w1, [rsp+ksKey+      8*((_II_+_w1) mod 17)]
   if     _w1 eq 13                                     ;tweak injection
    add         r1K_x&w1, [rsp+ksTwk+      8*((_II_+0  ) mod  3)]
   elseif _w0 eq 14
    add         r1K_x&w0, [rsp+ksTwk+      8*((_II_+1  ) mod  3)]
   elseif _w1 eq 15
    add         r1K_x&w1, _II_                          ;(injection counter)
   endif
  else                          ;here to do looping  key injection
   if  (_w0 eq 0)
    mov                   [rsp+X_stk+8*_w0],r1K_x0      ;if so, store N0 so we can use reg as index
    mov         rIdx,     [rsp+rIdx_offs]               ;get the injection counter index into rIdx (N0)
   else
    add         r1K_x&w0, [rsp+ksKey+8+8*rIdx+8*_w0]    ;even key injection
   endif
   if     _w1 eq 13                                     ;tweak injection
    add         r1K_x&w1, [rsp+ksTwk+8+8*rIdx+8*0  ]
   elseif _w0 eq 14
    add         r1K_x&w0, [rsp+ksTwk+8+8*rIdx+8*1  ]
   elseif _w1 eq 15
    addReg      r1K_x&w1, rIdx,,,1                      ;(injection counter)
   endif
    add         r1K_x&w1, [rsp+ksKey+8+8*rIdx+8*_w1]    ;odd  key injection
  endif
 endif
    ; insert the op provided, if any
    op1
endm
;;;;;;;;;;;;;;;;;
; MACRO: one round for 1024-bit blocks
;
R1024_OneRound  macro x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,xA,xB,xC,xD,xE,xF,_Rn_
  if (x0 ne 0) or ((x4 ne 4) and (x4 ne 6)) or (x4 ne (x6 xor 2))
    .err "faulty register assignment!"
  endif
    R1024_Mix   x0,x1,_Rn_,0
    R1024_Mix   x2,x3,_Rn_,1 
    R1024_Mix   x4,x5,_Rn_,2, <mov        [rsp+X_stk+8*0&x4&h],r1K_x4>  ;save x4  on  stack (x4/x6 alternate)
    R1024_Mix   x8,x9,_Rn_,4, <mov r1K_x6,[rsp+X_stk+8*0&x6&h]>         ;load x6 from stack 
    R1024_Mix   xA,xB,_Rn_,5
    R1024_Mix   xC,xD,_Rn_,6
    R1024_Mix   x6,x7,_Rn_,3
    R1024_Mix   xE,xF,_Rn_,7
  if _SKEIN_DEBUG
    Skein_Debug_Round 1024,%(_Rn_+1)
  endif
endm ;R1024_OneRound
;;;;;;;;;;;;;;;;;
; MACRO: four rounds for 1024-bit blocks
;
R1024_FourRounds macro _RR_    ;RR = base round number (0 mod 4)
    ; should be here with r1K_x4 set properly, x6 stored on stack
    R1024_OneRound 0,1,2,3,4,5,6,7,8,9,A,B,C,D,E,F,%((_RR_)+0)
    R1024_OneRound 0,9,2,D,6,B,4,F,A,7,C,3,E,5,8,1,%((_RR_)+1)
    R1024_Oneround 0,7,2,5,4,3,6,1,C,F,E,D,8,B,A,9,%((_RR_)+2)
    R1024_Oneround 0,F,2,B,6,D,4,9,E,1,8,5,A,3,C,7,%((_RR_)+3)
  if (SKEIN_ASM_UNROLL and 1024) eq 0       ;here with r1K_x0 == rIdx, X0 on stack
    ;rotate the key schedule on the stack
    mov            [rsp+X_stk+       8* 8],r1K_x8;free up a reg
    mov     r1K_x8,[rsp+ksKey+8*rIdx+8* 0]          ;get key
    mov            [rsp+ksKey+8*rIdx+8*17],r1K_x8   ;rotate it (must do key first or tweak clobbers it!)
    mov     r1K_x8,[rsp+ksTwk+8*rIdx+8* 0]          ;get tweak
    mov            [rsp+ksTwk+8*rIdx+8* 3],r1K_x8   ;rotate it
    mov     r1K_x8,[rsp+X_stk+       8* 8]      ;get the reg back
    inc     rIdx                                ;bump the index
    mov            [rsp+rIdx_offs],rIdx         ;save it
    mov     r1K_x0,[rsp+ksKey+8*rIdx]           ;get the key schedule word for X0
    add     r1K_x0,[rsp+X_stk+8*0]              ;perform the X0 key injection
  endif
    ;show the result of the key injection
    Skein_Debug_Round 1024,SKEIN_RND_KEY_INJECT
endm ;R1024_FourRounds
;
;;;;;;;;;;;;;;;;
; code
;
Skein1024_Process_Block proc frame
;
    Setup_Stack 1024,ROUNDS_1024/8,NO_FRAME,<WCNT>
    mov     r09,[rdi+TWEAK+ 8]
    jmp   short Skein1024_block_loop
    align  16
    ; main hash loop for Skein1024
Skein1024_block_loop:
    ; general register usage:
    ;   RSP             = stack pointer
    ;   RAX..RDX,RSI,RDI= X1, X3..X7 (state words)
    ;   R08..R15        = X8..X15    (state words)
    ;   RBP             = temp (used for X0 and X2)
    ;
  if (SKEIN_ASM_UNROLL and 1024) eq 0
    xor     rax,rax                 ;init loop index on the stack
    mov     [rsp+rIdx_offs],rax
  endif
    mov     r08,[rdi+TWEAK+ 0]
    add     r08,[FP_+bitAdd]        ;computed updated tweak value T0
    mov     r10,r09
    xor     r10,r08                 ;rax/rbx/rcx = tweak schedule
    mov         [rdi+TWEAK+ 0],r08  ;save updated tweak value ctx->h.T[0]
    mov         [FP_+ksTwk+ 0],r08
    mov         [FP_+ksTwk+ 8],r09  ;keep values in r08,r09 for initial tweak injection below
    mov         [FP_+ksTwk+16],r10
  if _SKEIN_DEBUG
    mov         [rdi+TWEAK+ 8],r09  ;save updated tweak value ctx->h.T[1] for Skein_Debug_Block
  endif
    mov     rsi ,[FP_+blkPtr ]      ;r1K_x2 --> input block
    mov     rax , KW_PARITY         ;overall key schedule parity

    ; logic here assumes the set {rdi,rsi,rbp,rax} = r1K_x{0,1,2,3}

    irp _rN_,<0,1,2,3,4,6>            ;process the "initial" words, using r14,r15 as temps
      mov       r14,[rdi+X_VARS+8*_rN_]                 ;get state word
      mov       r15,[rsi+       8*_rN_]                 ;get msg   word
      xor       rax,r14                                 ;update key schedule parity
      mov           [FP_+ksKey +8*_rN_],r14             ;save key schedule word on stack
      mov           [FP_+Wcopy +8*_rN_],r15             ;save local msg Wcopy 
      add       r14,r15                                 ;do the initial key injection
      mov           [rsp+X_stk +8*_rN_],r14             ;save initial state var on stack
    endm
    ; now process the rest, using the "real" registers 
    ;     (MUST do it in reverse order to inject tweaks r08/r09 first)
    irp _rN_,<F,E,D,C,B,A,9,8,7,5>
_rr_ = 0&_rN_&h
      mov   r1K_x&_rN_,[rdi+X_VARS+8*_rr_]              ;get key schedule word from context
      mov   r1K_x4    ,[rsi+       8*_rr_]              ;get next input msg word
      mov              [rsp+ksKey +8*_rr_],r1K_x&_rN_   ;save key schedule on stack
      xor   rax       , r1K_x&_rN_                      ;accumulate key schedule parity
      mov              [FP_+Wcopy +8*_rr_],r1K_x4       ;save copy of msg word for feedforward
      add   r1K_x&_rN_, r1K_x4                          ;do the initial  key  injection
      if     _rr_ eq 13                                 ;do the initial tweak injection
        addReg r1K_x&_rN_,r08                           ;          (only in words 13/14)
      elseif _rr_ eq 14
        addReg r1K_x&_rN_,r09
      endif
    endm
    mov                [FP_+ksKey+8*WCNT],rax           ;save key schedule parity
if _SKEIN_DEBUG
    Skein_Debug_Block 1024           ;debug dump
endif
    addReg  rsi,8*WCNT                                  ;bump the msg ptr
    mov                [FP_+blkPtr],rsi                 ;save bumped msg ptr
    ; re-load words 0..4 [rbp,rsi,rdi,rax,rbx] from stack, enter the main loop
    irp _rN_,<0,1,2,3,4>                                ;(no need to re-load x6)
      mov   r1K_x&_rN_,[rsp+X_stk+8*_rN_]               ;re-load state and get ready to go!
    endm
if _SKEIN_DEBUG
    Skein_Debug_Round 1024,SKEIN_RND_KEY_INITIAL        ;show state after initial key injection
endif
    ;
    ;;;;;;;;;;;;;;;;;
    ; now the key schedule is computed. Start the rounds
    ;
if SKEIN_ASM_UNROLL and 1024
_UNROLL_CNT =   ROUNDS_1024/8
else
_UNROLL_CNT =   SKEIN_UNROLL_1024
  if ((ROUNDS_1024/8) mod _UNROLL_CNT)
    .err "Invalid SKEIN_UNROLL1024"
  endif
Skein1024_round_loop:
endif
;
_Rbase_ = 0
rept _UNROLL_CNT*2                   ;implement the rounds, 4 at a time
      R1024_FourRounds %(4*_Rbase_+00)
_Rbase_ = _Rbase_+1
endm ;rept _UNROLL_CNT
;
if (SKEIN_ASM_UNROLL and 1024) eq 0
    cmp     qword ptr [rsp+tmpStk_1024],2*(ROUNDS_1024/8) ;see if we are done
    jb      Skein1024_round_loop    
endif
    ; end of rounds
    ;;;;;;;;;;;;;;;;;
    ;
    ; feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..15}
    mov     [rsp+X_stk+8*7],r1K_x7  ;we need a register. x6 already on stack
    mov     r1K_x7,[rsp+ctxPtr]
    
    irp _rN_,<0,1,2,3,4,5,8,9,A,B,C,D,E,F>              ;do all but x6,x7
      xor   r1K_x&_rN_,[rsp   +Wcopy +8*(0&_rN_&h)]     ;feedforward XOR
      mov              [r1K_x7+X_VARS+8*(0&_rN_&h)],r1K_x&_rN_ ;save result into context
  if (0&_rN_&h eq 9)
    mov     r09,FIRST_MASK
  endif
  if (0&_rN_&h eq 0eh)
    and     r09,[r1K_x7+TWEAK+ 8]
  endif
    endm
    ; 
    mov     rax,[rsp+X_stk    +8*6] ;now process x6,x7
    mov     rbx,[rsp+X_stk    +8*7]
    xor     rax,[rsp+Wcopy    +8*6]
    xor     rbx,[rsp+Wcopy    +8*7]
    mov         [r1K_x7+X_VARS+8*6],rax
    dec     qword ptr [rsp+blkCnt]  ;set zero flag iff done
    mov         [r1K_x7+X_VARS+8*7],rbx

    Skein_Debug_Round 1024,SKEIN_RND_FEED_FWD,,<cmp qword ptr [rsp+blkCnt],0>
    ; go back for more blocks, if needed
    mov     rdi,[rsp+ctxPtr]        ;don't muck with the flags here!
    lea     rbp,[rsp+FRAME_OFFS]
    jnz     Skein1024_block_loop
    mov         [r1K_x7+TWEAK+ 8],r09
    Reset_Stack Skein1024_Process_Block
    ret
;
if _SKEIN_DEBUG
; call here with r08 = "round number"
Skein_Debug_Round_1024:
_SP_OFFS_ = 8*2                     ;stack "offset" here: r08, return addr
 SP_ equ <rsp + _SP_OFFS_>          ;useful shorthand below
;
  irp _wN_,<1,2,3,5,7,9,A,B,C,D,E,F> ;save rest of X[] state on stack so debug routines can access it
    mov         [SP_+X_stk+8*(0&_wN_&h)],r1K_x&_wN_
  endm
    ;figure out what to do with x0. On rounds R where R==0 mod 4, it's already on the stack
    cmp     r08,SKEIN_RND_SPECIAL   ;special rounds always save
    jae     save_x0
    test    r08,3
    jz      save_x0_not
save_x0:
    mov     [SP_+X_stk+8*0],r1K_x0
save_x0_not:
    ;figure out the x4/x6 swapping state and save the correct one!
    cmp     r08,SKEIN_RND_SPECIAL   ;special rounds always do x4
    jae     save_x4
    test    r08,1                   ;and even ones have r4 as well
    jz      save_x4
    mov     [SP_+X_stk+8*6],r1K_x6
    jmp     short debug_1024_go
save_x4:
    mov     [SP_+X_stk+8*4],r1K_x4
debug_1024_go:
    ;now all is saved in Xstk[] except for X8
    push    rdx                     ;save two regs for BLK_BITS-specific parms
    push    rcx
_SP_OFFS_ = _SP_OFFS_ + 16          ;adjust stack offset accordingly
    ; now stack offset is 32 to X_stk
    mov     rcx,[SP_ - 8]           ;get back original r08 (pushed on stack in macro call)
    mov         [SP_+X_stk+8*8],rcx ;and save it in its rightful place in X_stk[8]
    mov     rdx,[SP_+ctxPtr]        ;ctx_hdr_ptr
    mov     rcx, 1024               ;block size
    jmp     Skein_Debug_Round_Common
endif
;
Skein1024_Process_Block endp
;
ifdef SKEIN_CODE_SIZE
    public  Skein1024_Process_Block_CodeSize
Skein1024_Process_Block_CodeSize proc
    mov     rax,_ProcBytes_
    ret
Skein1024_Process_Block_CodeSize endp
;
    public  Skein1024_Unroll_Cnt
Skein1024_Unroll_Cnt proc
  if _UNROLL_CNT ne ROUNDS_1024/8
    mov     rax,_UNROLL_CNT
  else
    xor     rax,rax
  endif
    ret
Skein1024_Unroll_Cnt endp
endif
;
endif ; _USE_ASM_ and 1024
;
if _SKEIN_DEBUG
;----------------------------------------------------------------
;local debug routine to set up for calls to:
;  void Skein_Show_Round(uint_t bits,const Skein_Ctxt_Hdr_t *h,int r,const u64b_t *X);
;
; here with r08 = round number
;           rdx = ctx_hdr_ptr
;           rcx = block size (256/512/1024)
;
Skein_Debug_Round_Common:
_SP_OFFS_ = 32                      ;current stack "offset": r08, retAddr, rcx, rdx
    irp _rr_,<rax,rbx,rsi,rdi,rbp,r09,r10,r11,r12,r13,r14,r15>  ;save the rest of the regs
      push  _rr_
_SP_OFFS_ = _SP_OFFS_+8
    endm
 if (_SP_OFFS_ and 0Fh)             ; make sure stack is still 16-byte aligned here
    .err    "Debug_Round_Common: stack alignment"
 endif
    ; compute r09 = ptr to the X[] array on the stack
    lea     r09,[SP_+X_stk]         ;adjust for reg pushes, return address
    cmp     r08,SKEIN_RND_FEED_FWD  ;special handling for feedforward "round"?
    jnz     _got_r09a
    lea     r09,[rdx+X_VARS]
_got_r09a:
  if _USE_ASM_ and 1024
    ; special handling for 1024-bit case
    ;    (for rounds right before with key injection: 
    ;        use xDebug_1024[] instead of X_stk[])
    cmp     r08,SKEIN_RND_SPECIAL
    jae     _got_r09b               ;must be a normal round
    or      r08,r08
    jz      _got_r09b               ;just before key injection
    test    r08,3
    jne     _got_r09b
    cmp     rcx,1024                ;only 1024-bit(s) for now
    jne     _got_r09b
    lea     r09,[SP_+xDebug_1024]
_got_r09b:
  endif
    sub     rsp, 8*4                ;make room for parms on stack
    call    Skein_Show_Round        ;call external debug handler
    add     rsp, 8*4                ;discard parm space on the stack

    irp _rr_,<r15,r14,r13,r12,r11,r10,r09,rbp,rdi,rsi,rbx,rax>  ;restore regs
      pop   _rr_
_SP_OFFS_ = _SP_OFFS_-8
    endm
 if _SP_OFFS_ - 32
    .err    "Debug_Round_Common: push/pop misalignment!"
 endif    
    pop     rcx
    pop     rdx
    ret
endif
;----------------------------------------------------------------
    end
