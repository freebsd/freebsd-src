/*
 * Macros for asm code.  Arm version.
 *
 * Copyright (c) 2019-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef _ASMDEFS_H
#define _ASMDEFS_H

/* Check whether leaf function PAC signing has been requested in the
   -mbranch-protect compile-time option.  */
#define LEAF_PROTECT_BIT 2

#ifdef __ARM_FEATURE_PAC_DEFAULT
# define HAVE_PAC_LEAF \
	((__ARM_FEATURE_PAC_DEFAULT & (1 << LEAF_PROTECT_BIT)) && 1)
#else
# define HAVE_PAC_LEAF 0
#endif

/* Provide default parameters for PAC-code handling in leaf-functions.  */
#if HAVE_PAC_LEAF
# ifndef PAC_LEAF_PUSH_IP
#  define PAC_LEAF_PUSH_IP 1
# endif
#else /* !HAVE_PAC_LEAF */
# undef PAC_LEAF_PUSH_IP
# define PAC_LEAF_PUSH_IP 0
#endif /* HAVE_PAC_LEAF */

#define STACK_ALIGN_ENFORCE 0

/******************************************************************************
* Implementation of the prologue and epilogue assembler macros and their
* associated helper functions.
*
* These functions add support for the following:
*
* - M-profile branch target identification (BTI) landing-pads when compiled
*   with `-mbranch-protection=bti'.
* - PAC-signing and verification instructions, depending on hardware support
*   and whether the PAC-signing of leaf functions has been requested via the
*   `-mbranch-protection=pac-ret+leaf' compiler argument.
* - 8-byte stack alignment preservation at function entry, defaulting to the
*   value of STACK_ALIGN_ENFORCE.
*
* Notes:
* - Prologue stack alignment is implemented by detecting a push with an odd
*   number of registers and prepending a dummy register to the list.
* - If alignment is attempted on a list containing r0, compilation will result
*   in an error.
* - If alignment is attempted in a list containing r1, r0 will be prepended to
*   the register list and r0 will be restored prior to function return.  for
*   functions with non-void return types, this will result in the corruption of
*   the result register.
* - Stack alignment is enforced via the following helper macro call-chain:
*
*	{prologue|epilogue} ->_align8 -> _preprocess_reglist ->
*		_preprocess_reglist1 -> {_prologue|_epilogue}
*
* - Debug CFI directives are automatically added to prologues and epilogues,
*   assisted by `cfisavelist' and `cfirestorelist', respectively.
*
* Arguments:
* prologue
* --------
* - first	- If `last' specified, this serves as start of general-purpose
*		  register (GPR) range to push onto stack, otherwise represents
*		  single GPR to push onto stack.  If omitted, no GPRs pushed
*		  onto stack at prologue.
* - last	- If given, specifies inclusive upper-bound of GPR range.
* - push_ip	- Determines whether IP register is to be pushed to stack at
*		  prologue.  When pac-signing is requested, this holds the
*		  the pac-key.  Either 1 or 0 to push or not push, respectively.
*		  Default behavior: Set to value of PAC_LEAF_PUSH_IP macro.
* - push_lr	- Determines whether to push lr to the stack on function entry.
*		  Either 1 or 0  to push or not push, respectively.
* - align8	- Whether to enforce alignment. Either 1 or 0, with 1 requesting
*		  alignment.
*
* epilogue
* --------
*   The epilogue should be called passing the same arguments as those passed to
*   the prologue to ensure the stack is not corrupted on function return.
*
* Usage examples:
*
*   prologue push_ip=1 -> push {ip}
*   epilogue push_ip=1, align8=1 -> pop {r2, ip}
*   prologue push_ip=1, push_lr=1 -> push {ip, lr}
*   epilogue 1 -> pop {r1}
*   prologue 1, align8=1 -> push {r0, r1}
*   epilogue 1, push_ip=1 -> pop {r1, ip}
*   prologue 1, 4 -> push {r1-r4}
*   epilogue 1, 4 push_ip=1 -> pop {r1-r4, ip}
*
******************************************************************************/

/* Emit .cfi_restore directives for a consecutive sequence of registers.  */
	.macro cfirestorelist first, last
	.cfi_restore \last
	.if \last-\first
	 cfirestorelist \first, \last-1
	.endif
	.endm

/* Emit .cfi_offset directives for a consecutive sequence of registers.  */
	.macro cfisavelist first, last, index=1
	.cfi_offset \last, -4*(\index)
	.if \last-\first
	 cfisavelist \first, \last-1, \index+1
	.endif
	.endm

.macro _prologue first=-1, last=-1, push_ip=PAC_LEAF_PUSH_IP, push_lr=0
	.if \push_ip & 1 != \push_ip
	 .error "push_ip may be either 0 or 1"
	.endif
	.if \push_lr & 1 != \push_lr
	 .error "push_lr may be either 0 or 1"
	.endif
	.if \first != -1
	 .if \last == -1
	  /* Upper-bound not provided: Set upper = lower.  */
	  _prologue \first, \first, \push_ip, \push_lr
	  .exitm
	 .endif
	.endif
#if HAVE_PAC_LEAF
# if __ARM_FEATURE_BTI_DEFAULT
	pacbti	ip, lr, sp
# else
	pac	ip, lr, sp
# endif /* __ARM_FEATURE_BTI_DEFAULT */
	.cfi_register 143, 12
#else
# if __ARM_FEATURE_BTI_DEFAULT
	bti
# endif /* __ARM_FEATURE_BTI_DEFAULT */
#endif /* HAVE_PAC_LEAF */
	.if \first != -1
	 .if \last != \first
	  .if \last >= 13
	.error "SP cannot be in the save list"
	  .endif
	  .if \push_ip
	   .if \push_lr
	/* Case 1: push register range, ip and lr registers.  */
	push {r\first-r\last, ip, lr}
	.cfi_adjust_cfa_offset ((\last-\first)+3)*4
	.cfi_offset 14, -4
	.cfi_offset 143, -8
	cfisavelist \first, \last, 3
	   .else // !\push_lr
	/* Case 2: push register range and ip register.  */
	push {r\first-r\last, ip}
	.cfi_adjust_cfa_offset ((\last-\first)+2)*4
	.cfi_offset 143, -4
	cfisavelist \first, \last, 2
	   .endif
	  .else // !\push_ip
	   .if \push_lr
	/* Case 3: push register range and lr register.  */
	push {r\first-r\last, lr}
	.cfi_adjust_cfa_offset ((\last-\first)+2)*4
	.cfi_offset 14, -4
	cfisavelist \first, \last, 2
	   .else // !\push_lr
	/* Case 4: push register range.  */
	push {r\first-r\last}
	.cfi_adjust_cfa_offset ((\last-\first)+1)*4
	cfisavelist \first, \last, 1
	   .endif
	  .endif
	 .else // \last == \first
	  .if \push_ip
	   .if \push_lr
	/* Case 5: push single GP register plus ip and lr registers.  */
	push {r\first, ip, lr}
	.cfi_adjust_cfa_offset 12
	.cfi_offset 14, -4
	.cfi_offset 143, -8
        cfisavelist \first, \first, 3
	   .else // !\push_lr
	/* Case 6: push single GP register plus ip register.  */
	push {r\first, ip}
	.cfi_adjust_cfa_offset 8
	.cfi_offset 143, -4
        cfisavelist \first, \first, 2
	   .endif
	  .else // !\push_ip
	   .if \push_lr
	/* Case 7: push single GP register plus lr register.  */
	push {r\first, lr}
	.cfi_adjust_cfa_offset 8
	.cfi_offset 14, -4
	cfisavelist \first, \first, 2
	   .else // !\push_lr
	/* Case 8: push single GP register.  */
	push {r\first}
	.cfi_adjust_cfa_offset 4
	cfisavelist \first, \first, 1
	   .endif
	  .endif
	 .endif
	.else // \first == -1
	 .if \push_ip
	  .if \push_lr
	/* Case 9: push ip and lr registers.  */
	push {ip, lr}
	.cfi_adjust_cfa_offset 8
	.cfi_offset 14, -4
	.cfi_offset 143, -8
	  .else // !\push_lr
	/* Case 10: push ip register.  */
	push {ip}
	.cfi_adjust_cfa_offset 4
	.cfi_offset 143, -4
	  .endif
	 .else // !\push_ip
          .if \push_lr
	/* Case 11: push lr register.  */
	push {lr}
	.cfi_adjust_cfa_offset 4
	.cfi_offset 14, -4
          .endif
	 .endif
	.endif
.endm

.macro _epilogue first=-1, last=-1, push_ip=PAC_LEAF_PUSH_IP, push_lr=0
	.if \push_ip & 1 != \push_ip
	 .error "push_ip may be either 0 or 1"
	.endif
	.if \push_lr & 1 != \push_lr
	 .error "push_lr may be either 0 or 1"
	.endif
	.if \first != -1
	 .if \last == -1
	  /* Upper-bound not provided: Set upper = lower.  */
	  _epilogue \first, \first, \push_ip, \push_lr
	  .exitm
	 .endif
	 .if \last != \first
	  .if \last >= 13
	.error "SP cannot be in the save list"
	  .endif
	  .if \push_ip
	   .if \push_lr
	/* Case 1: pop register range, ip and lr registers.  */
	pop {r\first-r\last, ip, lr}
	.cfi_restore 14
	.cfi_register 143, 12
	cfirestorelist \first, \last
	   .else // !\push_lr
	/* Case 2: pop register range and ip register.  */
	pop {r\first-r\last, ip}
	.cfi_register 143, 12
	cfirestorelist \first, \last
	   .endif
	  .else // !\push_ip
	   .if \push_lr
	/* Case 3: pop register range and lr register.  */
	pop {r\first-r\last, lr}
	.cfi_restore 14
	cfirestorelist \first, \last
	   .else // !\push_lr
	/* Case 4: pop register range.  */
	pop {r\first-r\last}
	cfirestorelist \first, \last
	   .endif
	  .endif
	 .else // \last == \first
	  .if \push_ip
	   .if \push_lr
	/* Case 5: pop single GP register plus ip and lr registers.  */
	pop {r\first, ip, lr}
	.cfi_restore 14
	.cfi_register 143, 12
	cfirestorelist \first, \first
	   .else // !\push_lr
	/* Case 6: pop single GP register plus ip register.  */
	pop {r\first, ip}
	.cfi_register 143, 12
	cfirestorelist \first, \first
	   .endif
	  .else // !\push_ip
	   .if \push_lr
	/* Case 7: pop single GP register plus lr register.  */
	pop {r\first, lr}
	.cfi_restore 14
	cfirestorelist \first, \first
	   .else // !\push_lr
	/* Case 8: pop single GP register.  */
	pop {r\first}
	cfirestorelist \first, \first
	   .endif
	  .endif
	 .endif
	.else // \first == -1
	 .if \push_ip
	  .if \push_lr
	/* Case 9: pop ip and lr registers.  */
	pop {ip, lr}
	.cfi_restore 14
	.cfi_register 143, 12
	  .else // !\push_lr
	/* Case 10: pop ip register.  */
	pop {ip}
	.cfi_register 143, 12
	  .endif
	 .else // !\push_ip
          .if \push_lr
	/* Case 11: pop lr register.  */
	pop {lr}
	.cfi_restore 14
          .endif
	 .endif
	.endif
#if HAVE_PAC_LEAF
	aut	ip, lr, sp
#endif /* HAVE_PAC_LEAF */
	bx	lr
.endm

/* Clean up expressions in 'last'.  */
.macro _preprocess_reglist1 first:req, last:req, push_ip:req, push_lr:req, reglist_op:req
	.if \last == 0
	 \reglist_op \first, 0, \push_ip, \push_lr
	.elseif \last == 1
	 \reglist_op \first, 1, \push_ip, \push_lr
	.elseif \last == 2
	 \reglist_op \first, 2, \push_ip, \push_lr
	.elseif \last == 3
	 \reglist_op \first, 3, \push_ip, \push_lr
	.elseif \last == 4
	 \reglist_op \first, 4, \push_ip, \push_lr
	.elseif \last == 5
	 \reglist_op \first, 5, \push_ip, \push_lr
	.elseif \last == 6
	 \reglist_op \first, 6, \push_ip, \push_lr
	.elseif \last == 7
	 \reglist_op \first, 7, \push_ip, \push_lr
	.elseif \last == 8
	 \reglist_op \first, 8, \push_ip, \push_lr
	.elseif \last == 9
	 \reglist_op \first, 9, \push_ip, \push_lr
	.elseif \last == 10
	 \reglist_op \first, 10, \push_ip, \push_lr
	.elseif \last == 11
	 \reglist_op \first, 11, \push_ip, \push_lr
	.else
	 .error "last (\last) out of range"
	.endif
.endm

/* Clean up expressions in 'first'.  */
.macro _preprocess_reglist first:req, last, push_ip=0, push_lr=0, reglist_op:req
	.ifb \last
	 _preprocess_reglist \first \first \push_ip \push_lr
	.else
	 .if \first > \last
	  .error "last (\last) must be at least as great as first (\first)"
	 .endif
	 .if \first == 0
	  _preprocess_reglist1 0, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 1
	  _preprocess_reglist1 1, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 2
	  _preprocess_reglist1 2, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 3
	  _preprocess_reglist1 3, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 4
	  _preprocess_reglist1 4, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 5
	  _preprocess_reglist1 5, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 6
	  _preprocess_reglist1 6, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 7
	  _preprocess_reglist1 7, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 8
	  _preprocess_reglist1 8, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 9
	  _preprocess_reglist1 9, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 10
	  _preprocess_reglist1 10, \last, \push_ip, \push_lr, \reglist_op
	 .elseif \first == 11
	  _preprocess_reglist1 11, \last, \push_ip, \push_lr, \reglist_op
	 .else
	  .error "first (\first) out of range"
	 .endif
	.endif
.endm

.macro _align8 first, last, push_ip=0, push_lr=0, reglist_op=_prologue
	.ifb \first
	 .ifnb \last
	  .error "can't have last (\last) without specifying first"
	 .else // \last not blank
	  .if ((\push_ip + \push_lr) % 2) == 0
	   \reglist_op first=-1, last=-1, push_ip=\push_ip, push_lr=\push_lr
	   .exitm
	  .else // ((\push_ip + \push_lr) % 2) odd
	   _align8 2, 2, \push_ip, \push_lr, \reglist_op
	   .exitm
	  .endif // ((\push_ip + \push_lr) % 2) == 0
	 .endif // .ifnb \last
	.endif // .ifb \first

	.ifb \last
	 _align8 \first, \first, \push_ip, \push_lr, \reglist_op
	.else
	 .if \push_ip & 1 <> \push_ip
	  .error "push_ip may be 0 or 1"
	 .endif
	 .if \push_lr & 1 <> \push_lr
	  .error "push_lr may be 0 or 1"
	 .endif
	 .ifeq (\last - \first + \push_ip + \push_lr) % 2
	  .if \first == 0
	   .error "Alignment required and first register is r0"
	   .exitm
	  .endif
	  _preprocess_reglist \first-1, \last, \push_ip, \push_lr, \reglist_op
	 .else
	  _preprocess_reglist \first \last, \push_ip, \push_lr, \reglist_op
	 .endif
	.endif
.endm

.macro prologue first, last, push_ip=PAC_LEAF_PUSH_IP, push_lr=0, align8=STACK_ALIGN_ENFORCE
	.if \align8
	 _align8 \first, \last, \push_ip, \push_lr, _prologue
	.else
	 _prologue first=\first, last=\last, push_ip=\push_ip, push_lr=\push_lr
	.endif
.endm

.macro epilogue first, last, push_ip=PAC_LEAF_PUSH_IP, push_lr=0, align8=STACK_ALIGN_ENFORCE
	.if \align8
	 _align8 \first, \last, \push_ip, \push_lr, reglist_op=_epilogue
	.else
	 _epilogue first=\first, last=\last, push_ip=\push_ip, push_lr=\push_lr
	.endif
.endm

#define ENTRY_ALIGN(name, alignment)	\
  .global name;		\
  .type name,%function;	\
  .align alignment;		\
  name:			\
  .fnstart;		\
  .cfi_startproc;

#define ENTRY(name)	ENTRY_ALIGN(name, 6)

#define ENTRY_ALIAS(name)	\
  .global name;		\
  .type name,%function;	\
  name:

#if defined (IS_LEAF)
# define END_UNWIND .cantunwind;
#else
# define END_UNWIND
#endif

#define END(name)	\
  .cfi_endproc;		\
  END_UNWIND		\
  .fnend;		\
  .size name, .-name;

#define L(l) .L ## l

#endif
