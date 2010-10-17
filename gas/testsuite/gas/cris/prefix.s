; See that prefix insns are assembled right.
 .text
 .syntax no_register_prefix
start:
;
; bdap (8-bit signed offset).
;

 bdap 0,r0
 move.b [r5],r4

 bdap 0,r3
 move.w [r6+],r4

 bdap -1,r1
 move.b [r5],r4

 bdap -1,r0
 move.w [r6+],r4

 bdap -128,r1
 move.b [r5],r4

 bdap -128,r7
 move.w [r6+],r4

 bdap 127,r1
 move.b [r5],r4

 bdap 127,r11
 move.w [r6],r4

;
; bdap.S [],r
;

 bdap.b 0,r4
 move.d [r7+],r9

 bdap.b 1,r5
 move.b [r5],r4

 bdap.b -1,r6
 move.d [r7+],r9

 bdap.b -128,r6
 move.d [r7],r9

 bdap.b 127,r6
 move.w [r6+],r4

 bdap.w 0,r4
 move.d [r7+],r9

 bdap.w 1,r5
 move.b [r5],r4

 bdap.w -1,r6
 move.d [r7+],r9

 bdap.w -128,r6
 move.d [r7],r9

 bdap.w 127,r6
 move.w [r6+],r4

 bdap.w -129,r6
 move.d [r7],r9

 bdap.w 128,r6
 move.d [r7],r9

 bdap.w -32768,r6
 move.b [r5],r4

 bdap.w 32767,r6
 move.w [r5+],r5

 bdap.d 0,r4
 move.d [r7+],r9

 bdap.d 1,r5
 move.b [r5],r4

 bdap.d -1,r6
 move.d [r7+],r9

 bdap.d -128,r6
 move.d [r7],r9

 bdap.d 127,r6
 move.w [r6+],r4

 bdap.d -129,r6
 move.d [r7],r9

 bdap.d 128,r6
 move.d [r7],r9

 bdap.d -32768,r6
 move.b [r5],r4

 bdap.d 32767,r6
 move.w [r5+],r5

 bdap.d -32769,r6
 move.w [r6+],r4

 bdap.d 32768,r6
 move.w [r6],r4

 bdap.d -327680,r6
 move.b [r5],r4

 bdap.d 21474805,r6
 move.w [r5+],r5

 bdap.d -2147483648,r6
 move.d [r7],r9

 bdap.d 2147483647,r6
 move.b [r5],r4

 bdap.d external_symbol,r6
 move.w [r5+],r5

 bdap.b [r0],r2
 move.d [r6+],r4

 bdap.w [r0],r2
 move.b [r5],r4

 bdap.d [r0],r2
 move.d [r6+],r4

 bdap.b [r10],r2
 move.d [r6+],r4

 bdap.w [r10],r2
 move.b [r5],r4

 bdap.d [r10],r2
 move.d [r6+],r4

 bdap.b [r2+],r2
 move.w [r6],r4

 bdap.w [r11+],r2
 move.w [r5+],r5

 bdap.d [r10+],r2
 move.w [r6],r4

;
; BIAP.m (like addi).
;

 biap.b r3,r0
 move.b [r5],r4

 biap.w r5,r3
 move.w [r6+],r4

 biap.d r13,r13
 move.b [r5],r4

 biap.b r6,r6
 move.w [r6+],r4

 biap.w r13,r13
 move.b [r5],r4

 biap.d r11,r12
 move.w [r6+],r4

 biap.w r5,r4
 move.b [r5],r4

 biap.b r3,r3
 move.w [r6+],r4

 biap.d r5,r5
 move.w [r5+],r5

;
; DIP []
;
 dip 0
 move.d [r7+],r9

 dip 1
 move.b [r5],r4

 dip -1
 move.d [r7+],r9

 dip -128
 move.d [r7],r9

 dip 127
 move.w [r6+],r4

 dip -129
 move.d [r7],r9

 dip 128
 move.d [r7],r9

 dip -32768
 move.b [r5],r4

 dip 32767
 move.w [r5+],r5

 dip -32769
 move.w [r6+],r4

 dip 32768
 move.w [r6],r4

 dip -327680
 move.b [r5],r4

 dip 21474805
 move.w [r5+],r5

 dip -2147483648
 move.d [r7],r9

 dip 2147483647
 move.b [r5],r4

 dip external_symbol
 move.w [r5+],r5

 dip [r10]
 move.d [r6+],r4

 dip [r11]
 move.d [r7],r4

 dip [r2+]
 move.w [r6],r4

 dip [r11+]
 move.w [r5+],r5
end:
