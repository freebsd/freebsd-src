; This used to be part of the binop test; differences broke when the
; broken-dot-word handling was broke and were moved here.
 .text
 .syntax no_register_prefix
 .byte 56,43,42 ; Make sure we don't start at zero.

; Some differences we want to see computed right, giving the right
; size of the operands.
;
 .globl back_ref_text_zero
back_ref_text_zero:
 .space 42,0
 .globl back_ref_text_fortytwo
back_ref_text_fortytwo:
 .space 32767-42,0
 .globl back_ref_text_three2767
back_ref_text_three2767:
 .space 327767-32767,0
 .globl back_ref_text_three27767
back_ref_text_three27767:

 .data
 .globl back_ref_data_zero
back_ref_data_zero:
 .space 42,0
 .globl back_ref_data_fortytwo
back_ref_data_fortytwo:
 .space 32767-42,0
 .globl back_ref_data_three2767
back_ref_data_three2767:
 .space 327767-32767,0
 .globl back_ref_data_three27767
back_ref_data_three27767:

 .text

 add.b back_ref_data_fortytwo-back_ref_data_zero,r5
 add.b forw_ref_data_fortytwo-forw_ref_data_zero,r5
 add.b back_ref_text_fortytwo-back_ref_text_zero,r5
 add.b forw_ref_text_fortytwo-forw_ref_text_zero,r5

 add.w back_ref_data_fortytwo-back_ref_data_zero,r5
 add.w forw_ref_data_fortytwo-forw_ref_data_zero,r5
 add.w back_ref_text_fortytwo-back_ref_text_zero,r5
 add.w forw_ref_text_fortytwo-forw_ref_text_zero,r5

 add.w back_ref_data_three2767-back_ref_data_zero,r5
 add.w forw_ref_data_three2767-forw_ref_data_zero,r5
 add.w back_ref_text_three2767-back_ref_text_zero,r5
 add.w forw_ref_text_three2767-forw_ref_text_zero,r5

 add.d back_ref_data_fortytwo-back_ref_data_zero,r5
 add.d forw_ref_data_fortytwo-forw_ref_data_zero,r5
 add.d back_ref_text_fortytwo-back_ref_text_zero,r5
 add.d forw_ref_text_fortytwo-forw_ref_text_zero,r5

 add.d back_ref_data_three2767-back_ref_data_zero,r5
 add.d forw_ref_data_three2767-forw_ref_data_zero,r5
 add.d back_ref_text_three2767-back_ref_text_zero,r5
 add.d forw_ref_text_three2767-forw_ref_text_zero,r5

 add.d back_ref_data_three27767-back_ref_data_zero,r5
 add.d forw_ref_data_three27767-forw_ref_data_zero,r5
 add.d back_ref_text_three27767-back_ref_text_zero,r5
 add.d forw_ref_text_three27767-forw_ref_text_zero,r5

 .text
; Don't have references to addresses immediately after the
; tested code (I'm superstitious).
 .byte 56,43,42

 .globl forw_ref_text_zero
forw_ref_text_zero:
 .space 42,0
 .globl forw_ref_text_fortytwo
forw_ref_text_fortytwo:
 .space 32767-42
 .globl forw_ref_text_three2767
forw_ref_text_three2767:
 .space 327767-32767,0
 .globl forw_ref_text_three27767
forw_ref_text_three27767:

 .data
 .globl forw_ref_data_zero
forw_ref_data_zero:
 .space 42,0
 .globl forw_ref_data_fortytwo
forw_ref_data_fortytwo:
 .globl forw_ref_data_three2767
 .space 32767-42
forw_ref_data_three2767:
 .space 327767-32767,0
 .globl forw_ref_data_three27767
forw_ref_data_three27767:
