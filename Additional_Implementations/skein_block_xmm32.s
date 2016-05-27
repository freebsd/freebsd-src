#
#----------------------------------------------------------------
# 32-bit x86 assembler code for Skein block functions using XMM registers
#
# Author: Doug Whiting, Hifn/Exar
#
# This code is released to the public domain.
#----------------------------------------------------------------
#
    .text
    .altmacro                               #use advanced macro features
    .psize 0,128                            #list file has no page boundaries
#
_MASK_ALL_  =   (256+512+1024)              #all three algorithm bits
SAVE_REGS   =   1
#
#################
.ifndef SKEIN_USE_ASM
_USE_ASM_        = _MASK_ALL_
.elseif SKEIN_USE_ASM & _MASK_ALL_
_USE_ASM_        = SKEIN_USE_ASM
.else
_USE_ASM_        = _MASK_ALL_
.endif
#
#################
.ifndef SKEIN_LOOP  
_SKEIN_LOOP       = 002                     #default is all fully unrolled, except Skein1024
.else
_SKEIN_LOOP       = SKEIN_LOOP
.endif
#--------------
# the unroll counts (0 --> fully unrolled)
SKEIN_UNROLL_256  = (_SKEIN_LOOP / 100) % 10
SKEIN_UNROLL_512  = (_SKEIN_LOOP /  10) % 10
SKEIN_UNROLL_1024 = (_SKEIN_LOOP      ) % 10
#
SKEIN_ASM_UNROLL  = 0
  .irp _NN_,256,512,1024
    .if (SKEIN_UNROLL_\_NN_) == 0
SKEIN_ASM_UNROLL  = SKEIN_ASM_UNROLL + \_NN_
    .endif
  .endr
#
#################
#
.ifndef SKEIN_ROUNDS
ROUNDS_256  =   72
ROUNDS_512  =   72
ROUNDS_1024 =   80
.else
ROUNDS_256  = 8*((((SKEIN_ROUNDS / 100) + 5) % 10) + 5)
ROUNDS_512  = 8*((((SKEIN_ROUNDS /  10) + 5) % 10) + 5)
ROUNDS_1024 = 8*((((SKEIN_ROUNDS      ) + 5) % 10) + 5)
.irp _NN_,256,512,1024
  .if _USE_ASM_ && \_NN_
    .irp _RR_,%(ROUNDS_\_NN_)
      .if \_NN_ < 1024
.print  "+++ SKEIN_ROUNDS_\_NN_  = \_RR_"
      .else
.print  "+++ SKEIN_ROUNDS_\_NN_ = \_RR_"
      .endif
    .endr
  .endif
.endr
.endif
#################
#
.ifdef SKEIN_CODE_SIZE
_SKEIN_CODE_SIZE = (1)
.else
.ifdef  SKEIN_PERF                           #use code size if SKEIN_PERF is defined
_SKEIN_CODE_SIZE = (1)
.endif
.endif
#
#################
#
.ifndef SKEIN_DEBUG
_SKEIN_DEBUG      = 0
.else
_SKEIN_DEBUG      = 1
.endif
#################
#
# define offsets of fields in hash context structure
#
HASH_BITS   =   0                           ## bits of hash output
BCNT        =   4 + HASH_BITS               #number of bytes in BUFFER[]
TWEAK       =   4 + BCNT                    #tweak values[0..1]
X_VARS      =  16 + TWEAK                   #chaining vars
#
#(Note: buffer[] in context structure is NOT needed here :-)
#
KW_PARITY_LO=   0xA9FC1A22                  #overall parity of key schedule words (hi32/lo32)
KW_PARITY_HI=   0x1BD11BDA
FIRST_MASK8 =   ~ (1 << 6)                  #FIRST block flag bit
#
# rotation constants for Skein
#
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
#
#----------------------------------------------------------------
# declare allocated space on the stack
.macro StackVar  localName,localSize
\localName  =   _STK_OFFS_
_STK_OFFS_  =   _STK_OFFS_+(\localSize)
.endm #StackVar
#
#----------------------------------------------------------------
#
# MACRO: Configure stack frame, allocate local vars
#
.macro Setup_Stack WCNT,RND_CNT
_STK_OFFS_  =   0                   #starting offset from esp, forced on 16-byte alignment
    #----- local  variables         #<-- esp
    StackVar    X_stk  , 8*(WCNT)   #local context vars
    StackVar    Wcopy  , 8*(WCNT)   #copy of input block    
    StackVar    ksTwk  ,16*3        #key schedule: tweak words
    StackVar    ksKey  ,16*(WCNT)+16#key schedule: key   words
FRAME_OFFS  =   ksTwk+128           #<-- ebp
F_O         =   FRAME_OFFS          #syntactic shorthand
  .if (SKEIN_ASM_UNROLL && (WCNT*64)) == 0
    StackVar    ksRot,16*(RND_CNT/4)#leave space for ks "rotation" to happen
  .endif
LOCAL_SIZE  =   _STK_OFFS_          #size of local vars
    #
    #"restart" the stack defns, because we relocate esp to guarantee alignment
    #    (i.e., these vars are NOT at fixed offsets from esp)
_STK_OFFS_  =   0
    #----- 
    StackVar    savRegs,8*4         #pushad data
    StackVar    retAddr,4           #return address
    #----- caller parameters
    StackVar    ctxPtr ,4           #context ptr
    StackVar    blkPtr ,4           #pointer to block data
    StackVar    blkCnt ,4           #number of full blocks to process
    StackVar    bitAdd ,4           #bit count to add to tweak
    #----- caller's stack frame
#
# Notes on stack frame setup:
#   * the most used variable (except for Skein-256) is X_stk[], based at [esp+0]
#   * the next most used is the key schedule words
#       so ebp is "centered" there, allowing short offsets to the key/tweak
#       schedule in 256/512-bit Skein cases, but not posible for Skein-1024 :-(
#   * the Wcopy variables are infrequently accessed, and they have long 
#       offsets from both esp and ebp only in the 1024-bit case.
#   * all other local vars and calling parameters can be accessed 
#       with short offsets, except in the 1024-bit case
#
    pushal                          #save all regs
    movl    %esp,%ebx               #keep ebx as pointer to caller parms
    subl    $LOCAL_SIZE,%esp        #make room for the locals
    andl    $~15,%esp               #force alignment
    movl    ctxPtr(%ebx),%edi       #edi --> Skein context
    leal    FRAME_OFFS(%esp),%ebp   #maximize use of short offsets from ebp
    movl    blkCnt(%ebx),%ecx       #keep block cnt in ecx
.endm #Setup_Stack
#
#----------------------------------------------------------------
#
.macro Reset_Stack,procStart
    movl     %ebx,%esp              #get rid of locals (wipe??)
    popal                           #restore all regs
.endm # Reset_Stack
#
#----------------------------------------------------------------
# macros to help debug internals
#
.if _SKEIN_DEBUG
    .extern   _Skein_Show_Block   #calls to C routines
    .extern   _Skein_Show_Round
#
SKEIN_RND_SPECIAL       =   1000
SKEIN_RND_KEY_INITIAL   =   SKEIN_RND_SPECIAL+0
SKEIN_RND_KEY_INJECT    =   SKEIN_RND_SPECIAL+1
SKEIN_RND_FEED_FWD      =   SKEIN_RND_SPECIAL+2
#
.macro Skein_Debug_Block BLK_BITS
#
#void Skein_Show_Block(uint_t bits,const Skein_Ctxt_Hdr_t *h,const u64b_t *X,
#                     const u08b_t *blkPtr, const u64b_t *wPtr, 
#                     const u64b_t *ksPtr,const u64b_t *tsPtr)#
#
    call    _Put_XMM_\BLK_BITS
    pushal                          #save all regs
    leal    ksTwk+1-F_O(%ebp),%eax  #+1 = flag: "stride" size = 2 qwords
    leal    ksKey+1-F_O(%ebp),%esi
    leal    Wcopy+32(%esp),%ecx     #adjust offset by 32 for pushad
    movl    ctxPtr(%ebx)  ,%edx     #ctx_hdr_ptr
    leal    X_VARS(%edx)  ,%edx     #edx ==> cxt->X[]
    pushl   %eax                    #tsPtr
    pushl   %esi                    #ksPtr
    pushl   %ecx                    #wPtr
    pushl   blkPtr(%ebx)            #blkPtr
    pushl   %edx                    #ctx->Xptr
    pushl   ctxPtr(%ebx)            #ctx_hdr_ptr
    movl    $\BLK_BITS,%eax
    pushl   %eax                    #bits
    call    _Skein_Show_Block
    addl    $7*4,%esp               #discard parameter space on stack
    popal                           #restore regs
#
    call    _Get_XMM_\BLK_BITS
.endm #Skein_Debug_Block

#
.macro Skein_Debug_Round BLK_BITS,R,saveRegs=0
#
#void Skein_Show_Round(uint_t bits,const Skein_Ctxt_Hdr_t *h,int r,const u64b_t *X)#
#
  .if \saveRegs
    call    _Put_XMM_\BLK_BITS
  .endif
    pushal                          #save all regs
  .if R <> SKEIN_RND_FEED_FWD
    leal    32+X_stk(%esp),%eax     #adjust offset by 32 for pushal
  .else
    movl    ctxPtr(%ebx),%eax
    addl    $X_VARS,%eax
  .endif
    pushl   %eax                    #Xptr
  .if (SKEIN_ASM_UNROLL && \BLK_BITS) || (\R >= SKEIN_RND_SPECIAL)
    movl    $\R,%eax
  .else     #compute round number from edx, R
    leal    1+(((\R)-1) && 3)(,%edx,4),%eax
  .endif
    pushl   %eax                    #round number
    pushl   ctxPtr(%ebx)            #ctx_hdr_ptr
    movl    $\BLK_BITS,%eax
    pushl   %eax                    #bits
    call    _Skein_Show_Round
    addl    $4*4,%esp               #discard parameter space on stack
    popal                           #restore regs
  .if \saveRegs
    call  _Get_XMM_\BLK_BITS        #save internal vars for debug dump
  .endif
.endm  #Skein_Debug_Round
.endif #ifdef SKEIN_DEBUG
#
#----------------------------------------------------------------
# useful macros
.macro _ldX xn
    movq          X_stk+8*(\xn)(%esp),%xmm\xn
.endm

.macro _stX xn
    movq  %xmm\xn,X_stk+8*(\xn)(%esp)
.endm
#
#----------------------------------------------------------------
#
.macro C_label lName
 \lName:        #use both "genders" to work across linkage conventions
_\lName:
    .global  \lName
    .global _\lName
.endm
#

.if _USE_ASM_ & 256
#
# void Skein_256_Process_Block(Skein_256_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd)#
#
#################
#
# Skein-256 round macros
#
.macro R_256_OneRound _RR_,x0,x1,x2,x3,t0,t1
  .irp _qq_,%((\_RR_) && 7)        #figure out which rotation constants to use
    .if \x0 == 0
_RC0_ =   RC_256_\_qq_&&_0
_RC1_ =   RC_256_\_qq_&&_1
    .else
_RC0_ =   RC_256_\_qq_&&_1
_RC1_ =   RC_256_\_qq_&&_0
    .endif
  .endr
#
    paddq    %xmm\x1,%xmm\x0
    movq     %xmm\x1,%xmm\t0
    psllq  $   _RC0_,%xmm\x1
    psrlq  $64-_RC0_,%xmm\t0
    xorpd    %xmm\x0,%xmm\x1
    xorpd    %xmm\t0,%xmm\x1
#                         
    paddq    %xmm\x3,%xmm\x2
    movq     %xmm\x3,%xmm\t1
    psllq  $   _RC1_,%xmm\x3
    psrlq  $64-_RC1_,%xmm\t1
    xorpd    %xmm\x2,%xmm\x3
    xorpd    %xmm\t1,%xmm\x3
  .if _SKEIN_DEBUG
    Skein_Debug_Round 256,%(\_RR_+1),SAVE_REGS
  .endif
.endm #R_256_OneRound
#
.macro R_256_FourRounds _RN_
    R_256_OneRound %(_RN_+0),0,1,2,3,4,5
    R_256_OneRound (_RN_+1),2,1,0,3,4,5

    R_256_OneRound (_RN_+2),0,1,2,3,4,5
    R_256_OneRound (_RN_+3),2,1,0,3,4,5

    #inject key schedule
    incl  %edx                     #bump round number
    movd  %edx,%xmm4
  .if _UNROLL_CNT == (ROUNDS_256/8)
    #fully unrolled version
_RK_ = ((_RN_)/4)                 #key injection counter
    paddq ksKey+16*((_RK_+1) % 5)-F_O(%ebp),%xmm0
    paddq ksKey+16*((_RK_+2) % 5)-F_O(%ebp),%xmm1
    paddq ksKey+16*((_RK_+3) % 5)-F_O(%ebp),%xmm2
    paddq ksKey+16*((_RK_+4) % 5)-F_O(%ebp),%xmm3
    paddq ksTwk+16*((_RK_+1) % 3)-F_O(%ebp),%xmm1
    paddq ksTwk+16*((_RK_+2) % 3)-F_O(%ebp),%xmm2
    paddq %xmm4,%xmm3
  .else #looping version
    paddq ksKey+16*1-F_O(%esi),%xmm0
    paddq ksKey+16*2-F_O(%esi),%xmm1
    paddq ksKey+16*3-F_O(%esi),%xmm2
    paddq ksKey+16*4-F_O(%esi),%xmm3
    paddq ksTwk+16*1-F_O(%esi),%xmm1
    paddq ksTwk+16*2-F_O(%esi),%xmm2
    paddq %xmm4,%xmm3
#   
    movq        ksKey-F_O(%esi),%xmm4   #first, "rotate" key schedule on the stack
    movq        ksTwk-F_O(%esi),%xmm5   #    (for next time through)
    movq  %xmm4,ksKey+16*(WCNT+1)-F_O(%esi)
    movq  %xmm5,ksTwk+16*3-F_O(%esi)
    addl  $16,%esi                     #bump rolling pointer
  .endif
  .if _SKEIN_DEBUG
      Skein_Debug_Round 256,SKEIN_RND_KEY_INJECT,SAVE_REGS
  .endif
.endm #R256_FourRounds
#
.if _SKEIN_DEBUG # macros for saving/restoring X_stk for debug routines
_Put_XMM_256:
  .irp _NN_,0,1,2,3
    movq  %xmm\_NN_,X_stk+4+\_NN_*8(%esp)
  .endr
    ret
#
_Get_XMM_256:
  .irp _NN_,0,1,2,3
    movq            X_stk+4+_NN_*8(%esp),%xmm\_NN_
  .endr
    ret
.endif
#
#################
#
# code
#
C_label Skein_256_Process_Block
    WCNT    =   4                   #WCNT=4 for Skein-256
    Setup_Stack WCNT,ROUNDS_256
    # main hash loop for Skein_256
Skein_256_block_loop:
    movd    bitAdd (%ebx),%xmm4
    movq    TWEAK+0(%edi),%xmm5
    movq    TWEAK+8(%edi),%xmm6
    paddq   %xmm4        ,%xmm5     #bump T0 by the bitAdd parameter
    movq    %xmm5,TWEAK(%edi)       #save updated tweak value T0 (for next time)
    movapd  %xmm6,%xmm7
    xorpd   %xmm5,%xmm7             #compute overall tweak parity
    movdqa  %xmm5,ksTwk   -F_O(%ebp)#save the expanded tweak schedule on the stack
    movdqa  %xmm6,ksTwk+16-F_O(%ebp)        
    movdqa  %xmm7,ksTwk+32-F_O(%ebp)        

    movl    blkPtr(%ebx),%esi       #esi --> input block
    movl    $KW_PARITY_LO,%eax      #init key schedule parity accumulator
    movl    $KW_PARITY_HI,%edx 
    movd    %eax ,%xmm4
    movd    %edx ,%xmm0
    unpcklps %xmm0,%xmm4            #replicate parity dword to 64 bits
#
  .irp _NN_,0,1,2,3                 #copy in the chaining vars
    movq    X_VARS+8*\_NN_(%edi),%xmm\_NN_
    xorpd   %xmm\_NN_,%xmm4         #update overall parity
    movdqa  %xmm\_NN_,ksKey+16*_NN_-F_O(%ebp)
  .endr
    movdqa  %xmm4,ksKey+16*WCNT-F_O(%ebp)#save overall parity at the end of the array
#
    paddq   %xmm5,%xmm1             #inject the initial tweak words
    paddq   %xmm6,%xmm2
#
  .irp _NN_,0,1,2,3                 #perform the initial key injection
    movq          8*\_NN_(%esi),%xmm4#and save a copy of the input block on stack
    movq    %xmm4,8*\_NN_+Wcopy(%esp)
    paddq   %xmm4,%xmm\_NN_         #inject the key word
  .endr
#
.if _SKEIN_DEBUG                    #debug dump of state at this point
    Skein_Debug_Block 256
    Skein_Debug_Round 256,SKEIN_RND_KEY_INITIAL,SAVE_REGS
.endif
    addl    $WCNT*8,%esi            #skip to the next block
    movl    %esi,blkPtr(%ebx)       #save the updated block pointer
    #
    # now the key schedule is computed. Start the rounds
    #
    xorl    %edx,%edx               #edx = iteration count
.if SKEIN_ASM_UNROLL & 256
_UNROLL_CNT =   ROUNDS_256/8        #fully unrolled
.else
_UNROLL_CNT =   SKEIN_UNROLL_256    #partial unroll count
  .if ((ROUNDS_256/8) % _UNROLL_CNT)
    .error "Invalid SKEIN_UNROLL_256" #sanity check
  .endif
    movl    %ebp,%esi               #use this as "rolling" pointer into ksTwk/ksKey
Skein_256_round_loop:               #   (since there's no 16* scaled address mode)
.endif
#
_Rbase_ = 0
.rept _UNROLL_CNT*2                  # here with X[0..3] in XMM0..XMM3
      R_256_FourRounds _Rbase_
_Rbase_ = _Rbase_+4
.endr #rept _UNROLL_CNT*2
#
  .if _UNROLL_CNT <> (ROUNDS_256/8)
    cmpl    $2*(ROUNDS_256/8),%edx
    jb      Skein_256_round_loop
  .endif
    #----------------------------
    # feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..3}
  .irp _NN_,0,1,2,3
    movq    Wcopy+8*\_NN_(%esp),%xmm4
    xorpd   %xmm4,%xmm\_NN_
    movq    %xmm\_NN_,X_VARS+8*\_NN_(%edi)
  .endr
    andb    $FIRST_MASK8,TWEAK +15(%edi)
.if _SKEIN_DEBUG
    Skein_Debug_Round 256,SKEIN_RND_FEED_FWD,SAVE_REGS
.endif
    # go back for more blocks, if needed
    decl    %ecx
    jnz     Skein_256_block_loop
    Reset_Stack _Skein_256_Process_Block
    ret
#
.ifdef _SKEIN_CODE_SIZE
C_label  Skein_256_Process_Block_CodeSize
    movl    $_Skein_256_Process_Block_CodeSize - _Skein_256_Process_Block,%eax
    ret
#
C_label  Skein_256_Unroll_Cnt
  .if _UNROLL_CNT <> ROUNDS_256/8
    movl    $_UNROLL_CNT,%eax
  .else
    xorl    %eax,%eax
  .endif
    ret
.endif
.endif #_USE_ASM_ & 256
#
#----------------------------------------------------------------
#
.if _USE_ASM_ & 512
#
# void Skein_512_Process_Block(Skein_512_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd)#
#
#################
# MACRO: one round
#
.macro R_512_Round _RR_, a0,a1,Ra, b0,b1,Rb, c0,c1,Rc, d0,d1,Rd
  .irp _qq_,%((\_RR_) && 7)
_Ra_ = RC_512_\_qq_&&_\Ra
_Rb_ = RC_512_\_qq_&&_\Rb
_Rc_ = RC_512_\_qq_&&_\Rc
_Rd_ = RC_512_\_qq_&&_\Rd
  .endr
    paddq   %xmm\a1 , %xmm\a0 
                              _stX c0
    movq    %xmm\a1 , %xmm\c0 
    psllq  $   _Ra_ , %xmm\a1 
    psrlq  $64-_Ra_ , %xmm\c0 
    xorpd   %xmm\c0 , %xmm\a1 
    xorpd   %xmm\a0 , %xmm\a1 
                                    
    paddq   %xmm\b1 , %xmm\b0 
                              _stX a0
    movq    %xmm\b1 , %xmm\a0 
    psllq  $   _Rb_ , %xmm\b1 
    psrlq  $64-_Rb_ , %xmm\a0 
    xorpd   %xmm\b0 , %xmm\b1 
                              _ldX c0
    xorpd   %xmm\a0 , %xmm\b1 
                               
    paddq   %xmm\c1 , %xmm\c0 
    movq    %xmm\c1 , %xmm\a0 
    psllq  $   _Rc_ , %xmm\c1 
    psrlq  $64-_Rc_ , %xmm\a0 
    xorpd   %xmm\c0 , %xmm\c1 
    xorpd   %xmm\a0 , %xmm\c1 
                               
    paddq   %xmm\d1 , %xmm\d0 
    movq    %xmm\d1 , %xmm\a0           
    psllq  $   _Rd_ , %xmm\d1 
    psrlq  $64-_Rd_ , %xmm\a0 
    xorpd   %xmm\a0 , %xmm\d1 
                              _ldX a0
    xorpd   %xmm\d0 , %xmm\d1 
  .if _SKEIN_DEBUG
    Skein_Debug_Round 512,%(_RR_+1),SAVE_REGS
  .endif
.endm
#
# MACRO: four rounds
.macro R_512_FourRounds _RN_
    R_512_Round %((_RN_)  ), 0,1,0, 2,3,1, 4,5,2, 6,7,3
    R_512_Round %((_RN_)+1), 2,1,0, 4,7,1, 6,5,2, 0,3,3
    R_512_Round %((_RN_)+2), 4,1,0, 6,3,1, 0,5,2, 2,7,3
    R_512_Round %((_RN_)+3), 6,1,0, 0,7,1, 2,5,2, 4,3,3

    #inject key schedule
.irp _NN_,0,1,2,3,4,5,6,7
  .if _UNROLL_CNT == (ROUNDS_512/8)
    paddq ksKey+16*((((\_RN_)/4)+(\_NN_)+1)%9)-F_O(%ebp),%xmm\_NN_
  .else
    paddq ksKey+16*((\_NN_)+1)-F_O(%esi),%xmm\_NN_
  .endif
.endr
    _stX  0                       #free up a register
    incl  %edx                    #bump round counter
    movd  %edx,%xmm0              #inject the tweak
  .if _UNROLL_CNT == (ROUNDS_512/8)
    paddq ksTwk+16*(((_RN_)+1) % 3)-F_O(%ebp),%xmm5
    paddq ksTwk+16*(((_RN_)+2) % 3)-F_O(%ebp),%xmm6
    paddq %xmm0                              ,%xmm7
  .else #looping version
    paddq ksTwk+16*1-F_O(%esi),%xmm5
    paddq ksTwk+16*2-F_O(%esi),%xmm6
    paddq %xmm0               ,%xmm7
    # "rotate" key schedule on the stack (for next time through)
    movq        ksKey            -F_O(%esi),%xmm0
    movq  %xmm0,ksKey+16*(WCNT+1)-F_O(%esi)
    movq        ksTwk            -F_O(%esi),%xmm0
    movq  %xmm0,ksTwk+16*3       -F_O(%esi)
    addl  $16,%esi                #bump rolling pointer
  .endif
    _ldX  0                       #restore X0
  .if _SKEIN_DEBUG
    Skein_Debug_Round 512,SKEIN_RND_KEY_INJECT,SAVE_REGS
  .endif
.endm #R_512_FourRounds
#################
.if _SKEIN_DEBUG # macros for saving/restoring X_stk for debug routines
_Put_XMM_512:
  .irp _NN_,0,1,2,3,4,5,6,7
    movq  %xmm\_NN_,X_stk+4+\_NN_*8(%esp)
  .endr
    ret
#
_Get_XMM_512:
  .irp _NN_,0,1,2,3,4,5,6,7
    movq            X_stk+4+\_NN_*8(%esp),%xmm\_NN_
  .endr
    ret
.endif
#
#################
#
C_label Skein_512_Process_Block
    WCNT    =   8                   #WCNT=8 for Skein-512
    Setup_Stack WCNT,ROUNDS_512
    # main hash loop for Skein_512
Skein_512_block_loop:
    movd    bitAdd(%ebx) ,%xmm0
    movq    TWEAK+0(%edi),%xmm1
    movq    TWEAK+8(%edi),%xmm2
    paddq   %xmm0,%xmm1               #bump T0 by the bitAdd parameter
    movq    %xmm1,TWEAK(%edi)         #save updated tweak value T0 (for next time)
    movq    %xmm2,%xmm0
    xorpd   %xmm1,%xmm0               #compute overall tweak parity
    movdqa  %xmm1,ksTwk     -F_O(%ebp)#save the expanded tweak schedule on the stack
    movdqa  %xmm2,ksTwk+16*1-F_O(%ebp)    
    movdqa  %xmm0,ksTwk+16*2-F_O(%ebp)    

    movl    blkPtr(%ebx),%esi         #esi --> input block
    movl    $KW_PARITY_LO,%eax        #init key schedule parity accumulator
    movl    $KW_PARITY_HI,%edx 
    movd    %eax ,%xmm0
    movd    %edx ,%xmm7
    unpcklps %xmm7,%xmm0              #replicate parity dword to 64 bits
#
  .irp _NN_,7,6,5,4,3,2,1             #copy in the chaining vars (skip #0 for now)
    movq    X_VARS+8*\_NN_(%edi),%xmm\_NN_
    xorpd   %xmm\_NN_,%xmm0           #update overall parity
    movdqa  %xmm\_NN_,ksKey+16*\_NN_-F_O(%ebp)
   .if \_NN_ == 5
    paddq   %xmm1,%xmm5               #inject the initial tweak words
    paddq   %xmm2,%xmm6               #  (before they get trashed in %xmm1/2)
   .endif
  .endr
    movq    X_VARS(%edi),%xmm4        #handle #0 now
    xorpd   %xmm4,%xmm0               #update overall parity
    movdqa  %xmm4,ksKey+16* 0  -F_O(%ebp) #save the key value in slot #0
    movdqa  %xmm0,ksKey+16*WCNT-F_O(%ebp) #save overall parity at the end of the array
#
    movq    %xmm4,%xmm0
  .irp _NN_,7,6,5,  4,3,2,1,0         #perform the initial key injection (except #4)
    movq    8*\_NN_(%esi),%xmm4       #and save a copy of the input block on stack
    movq    %xmm4,8*\_NN_+Wcopy(%esp)
    paddq   %xmm4,%xmm\_NN_
  .endr
    movq    8*4(%esi),%xmm4           #get input block word #4
    movq    %xmm4,8*4+Wcopy(%esp)
    paddq   ksKey+16*4-F_O(%ebp),%xmm4#inject the initial key
#
.if _SKEIN_DEBUG                      #debug dump of state at this point
    Skein_Debug_Block 512
    Skein_Debug_Round 512,SKEIN_RND_KEY_INITIAL,SAVE_REGS
.endif
    addl    $WCNT*8,%esi              #skip to the next block
    movl    %esi,blkPtr(%ebx)         #save the updated block pointer
    #
    # now the key schedule is computed. Start the rounds
    #
    xorl    %edx,%edx                 #edx = round counter
.if SKEIN_ASM_UNROLL & 512
_UNROLL_CNT =   ROUNDS_512/8
.else
_UNROLL_CNT =   SKEIN_UNROLL_512
  .if ((ROUNDS_512/8) % _UNROLL_CNT)
    .error "Invalid SKEIN_UNROLL_512"
  .endif
    movl    %ebp,%esi                 #use this as "rolling" pointer into ksTwk/ksKey
Skein_512_round_loop:                 #   (since there's no 16* scaled address mode)
.endif
_Rbase_ = 0
.rept _UNROLL_CNT*2
      R_512_FourRounds %_Rbase_
_Rbase_ = _Rbase_+4
.endr #rept _UNROLL_CNT
#
.if (SKEIN_ASM_UNROLL & 512) == 0
    cmpl    $2*(ROUNDS_512/8),%edx
    jb      Skein_512_round_loop
.endif
    #----------------------------
    # feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..7}
    andb    $FIRST_MASK8,TWEAK +15(%edi)
.irp _NN_,0,2,4,6                   #do the aligned ones first
    xorpd   Wcopy+8*\_NN_(%esp),%xmm\_NN_
    movq    %xmm\_NN_,X_VARS+8*_NN_(%edi)
.endr
.irp _NN_,1,3,5,7                   #now we have some register space available
    movq    Wcopy+8*\_NN_(%esp),%xmm0
    xorpd   %xmm0,%xmm&\_NN_
    movq    %xmm&\_NN_,X_VARS+8*\_NN_(%edi)
.endr
.if _SKEIN_DEBUG
    Skein_Debug_Round 512,SKEIN_RND_FEED_FWD
.endif
    # go back for more blocks, if needed
    decl    %ecx
    jnz     Skein_512_block_loop

    Reset_Stack _Skein_512_Process_Block
    ret
#
.ifdef _SKEIN_CODE_SIZE
C_label Skein_512_Process_Block_CodeSize
    movl    $(_Skein_512_Process_Block_CodeSize - _Skein_512_Process_Block),%eax
    ret
#
C_label Skein_512_Unroll_Cnt
  .if _UNROLL_CNT <> ROUNDS_512/8
    movl    $_UNROLL_CNT,%eax
  .else
    xorl    %eax,%eax
  .endif
    ret
.endif
#
.endif # _USE_ASM_ & 512
#
#----------------------------------------------------------------
#
.if _USE_ASM_ & 1024
    .global      _Skein1024_Process_Block
#
# void Skein_1024_Process_Block(Skein_1024_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t bitcntAdd)#
#
R_1024_REGS =     (5)     #keep this many block variables in registers
#
################
.if _SKEIN_DEBUG # macros for saving/restoring X_stk for debug routines
_Put_XMM_1024:
_NN_ = 0
  .rept R_1024_REGS
   .irp _rr_,%(_NN_)
    movq   %xmm\_rr_,X_stk+4+8*_NN_(%esp)
   .endr
_NN_ = _NN_+1
  .endr
    ret
#
_Get_XMM_1024:
_NN_ = 0
  .rept R_1024_REGS
   .irp _rr_,%(_NN_)
    movq             X_stk+4+8*_NN_(%esp),%xmm\_rr_
   .endr
_NN_ = _NN_+1
  .endr
    ret
.endif
#
#################
# MACRO: one mix step
.macro MixStep_1024  x0,x1,rotIdx0,rotIdx1,_debug_=0
_r0_ =  \x0      #default, if already loaded
_r1_ =  \x1
  # load the regs (if necessary)
  .if (\x0 >= R_1024_REGS)
_r0_ =       5
    movq    X_stk+8*(\x0)(%esp),%xmm5
  .endif
  .if (\x1 >= R_1024_REGS)
_r1_ =       6     
    movq  X_stk+8*(\x1)(%esp),%xmm6
  .endif
  # do the mix
  .irp _rx_,%((rotIdx0) && 7)
_Rc_ = RC_1024_\_rx_&&_\rotIdx1  #rotation constant
  .endr
  .irp _x0_,%_r0_
  .irp _x1_,%_r1_
    paddq   %xmm\_x1_,%xmm\_x0_
    movq    %xmm\_x1_,%xmm7    
    psllq  $   _Rc_  ,%xmm\_x1_
    psrlq  $64-_Rc_  ,%xmm7    
    xorpd   %xmm\_x0_,%xmm\_x1_
    xorpd   %xmm7    ,%xmm\_x1_
  .endr
  .endr
  # save the regs (if necessary)
  .if (\x0 >= R_1024_REGS)
    movq    %xmm5,X_stk+8*(\x0)(%esp)
  .endif
  .if (\x1 >= R_1024_REGS)
    movq    %xmm6,X_stk+8*(\x1)(%esp)
  .endif
  # debug output
  .if _SKEIN_DEBUG && (\_debug_)
    Skein_Debug_Round 1024,%((\RotIdx0)+1),SAVE_REGS
  .endif
.endm
#################
# MACRO: four rounds
#
.macro R_1024_FourRounds _RR_
    #--------- round _RR_
    MixStep_1024     0, 1,%((\_RR_)+0),0
    MixStep_1024     2, 3,%((\_RR_)+0),1
    MixStep_1024     4, 5,%((\_RR_)+0),2
    MixStep_1024     6, 7,%((\_RR_)+0),3
    MixStep_1024     8, 9,%((\_RR_)+0),4
    MixStep_1024    10,11,%((\_RR_)+0),5
    MixStep_1024    12,13,%((\_RR_)+0),6
    MixStep_1024    14,15,%((\_RR_)+0),7,1
    #--------- round _RR_+1
    MixStep_1024     0, 9,%((\_RR_)+1),0
    MixStep_1024     2,13,%((\_RR_)+1),1
    MixStep_1024     6,11,%((\_RR_)+1),2
    MixStep_1024     4,15,%((\_RR_)+1),3
    MixStep_1024    10, 7,%((\_RR_)+1),4
    MixStep_1024    12, 3,%((\_RR_)+1),5
    MixStep_1024    14, 5,%((\_RR_)+1),6
    MixStep_1024     8, 1,%((\_RR_)+1),7,1
    #--------- round _RR_+2
    MixStep_1024     0, 7,%((\_RR_)+2),0    
    MixStep_1024     2, 5,%((\_RR_)+2),1
    MixStep_1024     4, 3,%((\_RR_)+2),2    
    MixStep_1024     6, 1,%((\_RR_)+2),3    
    MixStep_1024    12,15,%((\_RR_)+2),4
    MixStep_1024    14,13,%((\_RR_)+2),5    
    MixStep_1024     8,11,%((\_RR_)+2),6    
    MixStep_1024    10, 9,%((\_RR_)+2),7,1
    #--------- round _RR_+3
    MixStep_1024     0,15,%((\_RR_)+3),0
    MixStep_1024     2,11,%((\_RR_)+3),1
    MixStep_1024     6,13,%((\_RR_)+3),2
    MixStep_1024     4, 9,%((\_RR_)+3),3
    MixStep_1024    14, 1,%((\_RR_)+3),4
    MixStep_1024     8, 5,%((\_RR_)+3),5
    MixStep_1024    10, 3,%((\_RR_)+3),6
    MixStep_1024    12, 7,%((\_RR_)+3),7,1

    incl  %edx                     #edx = round number
    movd  %edx,%xmm7

    #inject the key
.irp _NN_,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
  .if _UNROLL_CNT <> (ROUNDS_1024/8)
    .if \_NN_ < R_1024_REGS
      paddq ksKey+16*\_NN_+16-F_O(%esi),%xmm&\_NN_
    .else
      movq  X_stk+ 8*\_NN_(%esp),%xmm6
     .if     \_NN_ == 15
      paddq %xmm7,%xmm6
     .elseif \_NN_ == 14
      paddq ksTwk+16*2-F_O(%esi),%xmm6
     .elseif \_NN_ == 13
      paddq ksTwk+16*1-F_O(%esi),%xmm6
     .endif
      paddq       ksKey+16*\_NN_+16-F_O(%esi),%xmm6
      movq  %xmm6,X_stk+ 8*\_NN_(%esp)
    .endif
  .else
    .if \_NN_ < R_1024_REGS
      paddq ksKey+16*(((_Rbase_/4)+(\_NN_)+1) % 17)-F_O(%ebp),%xmm&\_NN_
    .else
      movq  X_stk+ 8*\_NN_(%esp), %xmm6
      paddq ksKey+16*(((_Rbase_/4)+(\_NN_)+1) % 17)-F_O(%ebp),%xmm6
     .if     \_NN_ == 15
      paddq %xmm7,%xmm6
     .elseif \_NN_ == 14
      paddq ksTwk+16*(((_Rbase_/4)+2) %  3)-F_O(%ebp),%xmm6
     .elseif \_NN_ == 13
      paddq ksTwk+16*(((_Rbase_/4)+1) %  3)-F_O(%ebp),%xmm6
     .endif
      movq %xmm6,X_stk+ 8*\_NN_(%esp)
    .endif
  .endif
.endr
  .if _UNROLL_CNT <> (ROUNDS_1024/8) #rotate the key schedule on the stack
    movq ksKey-F_O(%esi), %xmm6
    movq ksTwk-F_O(%esi), %xmm7
    movq %xmm6,ksKey+16*(WCNT+1)-F_O(%esi)
    movq %xmm7,ksTwk+16* 3      -F_O(%esi)
    addl $16,%esi                   #bump rolling pointer
  .endif
  .if _SKEIN_DEBUG
      Skein_Debug_Round 1024,SKEIN_RND_KEY_INJECT ,SAVE_REGS
  .endif
.endm #R_1024_FourRounds
#
################
#
C_label Skein1024_Process_Block
#
    WCNT    =   16                  #WCNT=16 for Skein-1024
    Setup_Stack WCNT,ROUNDS_1024
    addl    $0x80,%edi              #bias the edi ctxt offsets to keep them all short
    # main hash loop for Skein1024
Skein1024_block_loop:
    movd    bitAdd(%ebx)      ,%xmm0
    movq    TWEAK+0-0x80(%edi),%xmm1
    movq    TWEAK+8-0x80(%edi),%xmm2
    paddq   %xmm0,%xmm1             #bump T0 by the bitAdd parameter
    movq    %xmm1,TWEAK-0x80(%edi)  #save updated tweak value T0 (for next time)
    movq    %xmm2,%xmm0
    xorpd   %xmm1,%xmm0             #compute overall tweak parity
    movdqa  %xmm1,ksTwk   -F_O(%ebp)#save the expanded tweak schedule on the stack
    movdqa  %xmm2,ksTwk+16-F_O(%ebp)
    movdqa  %xmm0,ksTwk+32-F_O(%ebp)

    movl    blkPtr(%ebx),%esi       #esi --> input block
    movl    $KW_PARITY_LO,%eax      #init key schedule parity accumulator
    movl    $KW_PARITY_HI,%edx 
    movd    %eax ,%xmm7
    movd    %edx ,%xmm6
    unpcklps %xmm6,%xmm7            #replicate parity dword to 64 bits
#
    leal    0x80(%esp),%eax         #use short offsets for Wcopy, X_stk writes below
.irp _NN_,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
    movq    X_VARS+8*\_NN_-0x80(%edi),%xmm6
    xorpd   %xmm6,%xmm7             #update overall parity
    movdqa  %xmm6,ksKey+16*\_NN_-F_O(%ebp) #save the key schedule on the stack
  .if \_NN_ < R_1024_REGS
    _rr_  =  \_NN_
  .else
    _rr_  =   R_1024_REGS
  .endif
  .irp _rn_,%(_rr_)
    movq    8*\_NN_(%esi),%xmm\_rn_ #save copy of the input block on stack
    movq    %xmm\_rn_,Wcopy+8*\_NN_-0x80(%eax)  #(for feedforward later)
    paddq   %xmm6,%xmm\_rn_         #inject the key into the block
   .if \_NN_ == 13
    paddq   %xmm1,%xmm\_rn_         #inject the initial tweak words
   .elseif \_NN_ == 14
    paddq   %xmm2,%xmm\_rn_
   .endif
   .if \_NN_ >= R_1024_REGS         #only save X[5..15] on stack, leave X[0..4] in regs
    movq    %xmm\_rn_,X_stk+8*\_NN_-0x80(%eax)
   .endif
  .endr
.endr
    movdqa  %xmm7,ksKey+16*WCNT-F_O(%ebp) #save overall key parity at the end of the array
#
.if _SKEIN_DEBUG                    #debug dump of state at this point
    Skein_Debug_Block 1024
    Skein_Debug_Round 1024,SKEIN_RND_KEY_INITIAL,SAVE_REGS
.endif
    addl    $WCNT*8,%esi            #skip to the next block
    movl    %esi,blkPtr(%ebx)       #save the updated block pointer
    #
    # now the key schedule is computed. Start the rounds
    #
    xorl    %edx,%edx               #edx = round counter
.if SKEIN_ASM_UNROLL & 1024
_UNROLL_CNT =   ROUNDS_1024/8
.else
_UNROLL_CNT =   SKEIN_UNROLL_1024
  .if ((ROUNDS_1024/8) % _UNROLL_CNT)
    .error "Invalid SKEIN_UNROLL_1024"
  .endif
    movl    %ebp,%esi               #use this as "rolling" pointer into ksTwk/ksKey
Skein_1024_round_loop:
.endif
#
_Rbase_ = 0
.rept _UNROLL_CNT*2
    R_1024_FourRounds %_Rbase_
_Rbase_ = _Rbase_+4
.endr #rept _UNROLL_CNT
#
.if (SKEIN_ASM_UNROLL & 1024) == 0
    cmp     $2*(ROUNDS_1024/8),%edx
    jb      Skein_1024_round_loop
.endif
    andb    $FIRST_MASK8,TWEAK +15-0x80(%edi)      #clear tweak bit for next time thru
    #----------------------------
    # feedforward:   ctx->X[i] = X[i] ^ w[i], {i=0..15}
    leal    0x80(%esp),%eax                        #allow short offsets to X_stk and Wcopy
.irp _NN_,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
  .if \_NN_ < R_1024_REGS
    .if \_NN_ && 1                                 #already in regs: no load needed
      movq  Wcopy+ 8*\_NN_-0x80(%eax),%xmm7        #unaligned
      xorpd %xmm7,%xmm\_NN_
    .else
      xorpd Wcopy+ 8*\_NN_-0x80(%eax),%xmm\_NN_    #aligned
    .endif
      movq  %xmm\_NN_,X_VARS+8*\_NN_-0x80(%edi)
  .else
      movq    X_stk+8*\_NN_-0x80(%eax),%xmm7       #load X value from stack
    .if \_NN_ && 1
      movq    Wcopy+8*\_NN_-0x80(%eax),%xmm6       #unaligned
      xorpd   %xmm6,%xmm7
    .else
      xorpd   Wcopy+8*\_NN_-0x80(%eax),%xmm7       #aligned
    .endif
      movq    %xmm7,X_VARS+8*\_NN_-0x80(%edi)
 .endif
.endr
.if _SKEIN_DEBUG
    Skein_Debug_Round 1024,SKEIN_RND_FEED_FWD   #no need to save regs on stack here
.endif
    # go back for more blocks, if needed
    decl    %ecx
    jnz     Skein1024_block_loop

    Reset_Stack _Skein1024_Process_Block
    ret
#
.ifdef _SKEIN_CODE_SIZE
C_label Skein1024_Process_Block_CodeSize
    movl    $(_Skein1024_Process_Block_CodeSize - _Skein1024_Process_Block),%eax
    ret
#
C_label Skein1024_Unroll_Cnt
  .if _UNROLL_CNT <> ROUNDS_1024/8
    movl    $_UNROLL_CNT,%eax
  .else
    xorl    %eax,%eax
  .endif
    ret
.endif
#
.endif # _USE_ASM_ & 1024
#----------------------------------------------------------------
    .end
