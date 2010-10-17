; Check that invalid PIC reloc and instruction size combinations are
; recognized.  Note that sizes of byte operands are not error-checked for
; not being in 16-bit range, so no error is recognized for a 16-bit operand.

; { dg-do assemble { target cris-*-* } }
; { dg-options "--pic --no-underscore --em=criself" }

 .syntax no_register_prefix
 .text
start:
 move.b extsym:GOTPLT16,r4	; { dg-error "PIC relocation size does not match" "" { xfail *-*-* } }
 move.b extsym12:GOTPLT,r5	; { dg-error "PIC relocation size does not match" }
 move.w extsym2:GOTPLT,r5	; { dg-error "PIC relocation size does not match" }
 move.d extsym3:GOTPLT16,r6	; { dg-error "PIC relocation size does not match" }
 move extsym4:GOTPLT16,srp	; { dg-error "PIC relocation size does not match" }
 move.b extsym5:GOT16,r4	; { dg-error "PIC relocation size does not match" "" { xfail *-*-* } }
 move.b extsym15:GOT,r7		; { dg-error "PIC relocation size does not match" }
 move.w extsym6:GOT,r5		; { dg-error "PIC relocation size does not match" }
 move.d extsym7:GOT16,r6	; { dg-error "PIC relocation size does not match" }
 move extsym8:GOT16,srp		; { dg-error "PIC relocation size does not match" }
