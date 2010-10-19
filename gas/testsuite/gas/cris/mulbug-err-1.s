; Test error message for mul insns at locations likely to trig
; a hardware bug.

;  { dg-do assemble { target cris-*-* } }
;  { dg-options "--em=criself" }

 ; First, .text isn't dword-aligned by default.
 .text
 muls.w $r1,$r4 ; { dg-error "align" }
 nop
 muls.b $r1,$r4 ; { dg-error "align" }
 mulu.d $r1,$r4 ; { dg-error "align" }

; Neither are other code sections, aligned to word.
 .section .text.1,"ax",@progbits
 .p2align 1
 muls.w $r1,$r4 ; { dg-error "align" }
 nop
 mulu.b $r1,$r4 ; { dg-error "align" }
 muls.d $r1,$r4 ; { dg-error "align" }

; Now, a section aligned to dword.  Errors for certain relative
; positions only.
 .section .text.2,"ax",@progbits
 .p2align 2
 mulu.w $r1,$r4
 nop
 muls.d $r1,$r4
 mulu.w $r1,$r4 ; { dg-error "align" }

; For good measure, a cache-line-aligned section.
 .section .text.3,"ax",@progbits
 .p2align 5
 muls.w $r1,$r4
 mulu.d $r4,$r1
 mulu.b $r1,$r4
 .rept 12
 nop
 .endr
 mulu.b $r1,$r4 ; { dg-error "align" }
 mulu.b $r1,$r4

; Last, make sure typical alignment use by a fixed gcc passes.
 .section .text.4,"ax",@progbits
 .align 1
 moveq 0,$r13
 moveq 1,$r13
 .p2alignw 5,0x050f,2
 muls.d $r1,$r4
 .rept 12
 moveq 2,$r13
 .endr
 .p2alignw 5,0x050f,2
 muls.w $r1,$r4
 .p2alignw 5,0x050f,2
 muls.b $r4,$r1
