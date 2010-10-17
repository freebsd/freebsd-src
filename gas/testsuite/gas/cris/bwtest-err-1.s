; File bwtest-err-1.s

;  { dg-do assemble { target cris-*-* } }

; A variant of exbwtest.s.  This is an example of invalid use of the broken-
; dot-word function.  The nearest label occurs about 32 kbytes after the primary
; jump table so the secondary jump table can't be reached by word displace-
; ments and the broken words overflow.

;  main()
;  {
;    byte i;
;
;    for (i=0; i <= 3; i++) {
;      result[i] = funct(i);
;    }
;  }
;
;  Register use :  r1 - i
;                  r2 - result address

        .text
	.syntax no_register_prefix
        .word   0
main:   move.d  stack,sp
        moveq   0,r1
        move.d  result,r2
for1:   cmpq    3,r1
        bgt     endfor1
        move.d  r1,r0
        jsr     funct
        move.w  r0,[r2+r1.w]
        ba      for1
        addq    1,r1
endfor1:
end:    ba      end
        nop


;  uword funct(i)
;    byte i;
;  {
;    switch (i) {
;      case 0 :  return 0x1111;
;      case 1 :  return 0x2222;
;      case 2 :  return 0x3333;
;      case 3 :  return 0x4444;
;    }
;  }
;
;  Parameters   :  r0 - i
;
;  Register use :  r1 - pjt address

funct:  push    r1
        move.d  pjt,r1
        adds.w  [r1+r0.w],pc
pjt:    .word   near1 - pjt
        .word   near2 - pjt
        .word   far1 - pjt
        .word   far2 - pjt

; Note that the line-number of the source-location of the error
; seems slightly off from the user perspective, but it's the
; best I could get without major changes in BW-handling.  Not
; sure it it's worth fixing.  May need adjustments if
; BW-handling changes.  Four errors from four .words are what's
; expected.

        .space  32760,0xFF; { dg-error "Adjusted signed \.word \(.*\) overflow.*" }

near1:  move.w  0x1111,r0
        ba      ret1
        nop

near2:  move.w  0x2222,r0
        ba      ret1
        nop

far1:   move.w  0x3333,r0
        ba      ret1
        nop

far2:   move.w  0x4444,r0
ret1:   pop     r1
        ret


result: .space  4 * 2   ; static uword result[4];

        .space  4
stack:
