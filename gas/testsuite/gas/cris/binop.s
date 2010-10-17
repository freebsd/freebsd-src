; @OC@ test
; Generic binary operations supporting all sizes and their various
; addressing modes.

; Some fairly big pseudorandom numbers we don't want to compute
; as differences in actual data or code.

 .set const_int_32, 0x1b94452b
 .set const_int_m32, -3513208907
 .set two701867, 2701867

; Other constants that are not differences
 .set forty2, 42
 .set mforty2, -42

 .set three2767, 32767
 .set six5535, 65535

 .text
 .syntax no_register_prefix
notstart:
 .dword 0

start:

;;;;;;;;;;;;;;;;;
;
; r,r
 @OC@.b r3,r5
 @OC@.w r5,r13
 @OC@.d r10,r1

;;;;;;;;;;;;;;;;;
;
; [r],r
 @OC@.b [r0],r5
 @OC@.w [r5],r13
 @OC@.d [r10],r1

 @OC@ r13,[r5]
 @OC@ r1,[r10]

;;;;;;;;;;;;;;;;;
;
; [r+],r
 @OC@.b [r0+],r5
 @OC@.w [r5+],r13
 @OC@.d [r10+],r1

 @OC@ r13,[r5+]
 @OC@ r4,[r10+]

;;;;;;;;;;;;;;;;;
;
; const,r
 @OC@.b 0,r5
 @OC@.b 1,r5
 @OC@.b 127,r5
 @OC@.b 128,r5
 @OC@.b -1,r5
 @OC@.b -127,r5
 @OC@.b -128,r5
 @OC@.b 255,r5

 @OC@.b 42,r5
 @OC@.b -42,r5
 @OC@.b forty2,r5
 @OC@.b mforty2,r5
 @OC@.b -forty2,r5
 @OC@.b -mforty2,r5
 @OC@.b externalsym,r5

 @OC@.w 0,r13
 @OC@.w 1,r13
 @OC@.w 127,r13
 @OC@.w 128,r13
 @OC@.w -1,r13
 @OC@.w -127,r13
 @OC@.w -128,r13
 @OC@.w -129,r13
 @OC@.w 255,r13
 @OC@.w -255,r13
 @OC@.w 256,r13
 @OC@.w -8856,r13
 @OC@.w 8856,r13

 @OC@.w 42,r13
 @OC@.w -42,r13
 @OC@.w forty2,r13
 @OC@.w mforty2,r13
 @OC@.w -forty2,r5
 @OC@.w -mforty2,r5

 @OC@.w three2767,r5
 @OC@.w three2767+1,r5
 @OC@.w three2767+2,r13
 @OC@.w -three2767,r13
 @OC@.w -(three2767+1),r13
 @OC@.w six5535,r5
 @OC@.w externalsym,r5

 @OC@.d 0,r1
 @OC@.d 1,r1
 @OC@.d 127,r1
 @OC@.d 128,r1
 @OC@.d -1,r1
 @OC@.d -127,r1
 @OC@.d -128,r1
 @OC@.d 255,r1
 @OC@.d -255,r1
 @OC@.d 256,r1
 @OC@.d -8856,r1
 @OC@.d 8856,r1

 @OC@.d 2781868,r1
 @OC@.d -2701867,r1

 @OC@.d 0x9ec0ceac,r1
 @OC@.d -0x7ec0cead,r1
 @OC@.d const_int_m32,r1
 @OC@.d const_int_32,r1

 @OC@.d 42,r13
 @OC@.d -42,r5
 @OC@.d forty2,r5
 @OC@.d mforty2,r5
 @OC@.d -forty2,r5
 @OC@.d -mforty2,r5

 @OC@.d three2767,r5
 @OC@.d three2767+1,r5
 @OC@.d three2767+2,r5
 @OC@.d -three2767,r5
 @OC@.d -(three2767+1),r13
 @OC@.d -(three2767+2),r13
 @OC@.d six5535,r5
 @OC@.d six5535+1,r13
 @OC@.d two701867,r5
 @OC@.d -two701867,r5

 @OC@.d externalsym,r5

;;;;;;;;;;;;;;;;;
;
; [r+X],r

; [r+r.b],r

 @OC@.b [r2+r0.b],r5
 @OC@.w [r2+r5.b],r13
 @OC@.d [r2+r10.b],r1

 @OC@ r13,[r2+r5.b]
 @OC@ r3,[r2+r10.b]

; [r+[r].b],r
 @OC@.b [r2+[r0].b],r5
 @OC@.w [r2+[r5].b],r13
 @OC@.d [r2+[r10].b],r1

 @OC@ r13,[r2+[r5].b]
 @OC@ r2,[r2+[r10].b]

; [r+[r+].b],r
 @OC@.b [r2+[r0+].b],r5
 @OC@.w [r2+[r5+].b],r13
 @OC@.d [r2+[r10+].b],r1

 @OC@.w [r2+[r5+].b],r13

 @OC@.d [r2+[r10+].b],r1

 @OC@ r0,[r2+[r5+].b]
 @OC@ r12,[r2+[r10+].b]

; [r+r.w],r

 @OC@.b [r2+r0.w],r5
 @OC@.w [r2+r5.w],r13
 @OC@.d [r2+r10.w],r1

; [r+[r].w],r
 @OC@.b [r2+[r0].w],r5
 @OC@.w [r2+[r5].w],r13
 @OC@.d [r2+[r10].w],r1

 @OC@ r2,[r2+[r5].w]
 @OC@ r6,[r2+[r10].w]

; [r+[r+].w],r
 @OC@.b [r2+[r0+].w],r5
 @OC@.w [r2+[r5+].w],r13
 @OC@.d [r2+[r10+].w],r1

 @OC@.w [r2+[r5+].w],r13

 @OC@.d [r2+[r10+].w],r1

 @OC@ r7,[r2+[r5+].w]
 @OC@ r8,[r2+[r10+].w]

; [r+r.d],r

 @OC@.b [r2+r0.d],r5
 @OC@.w [r2+r5.d],r13
 @OC@.d [r2+r10.d],r1

 @OC@ r12,[r2+r5.d]
 @OC@ r9,[r2+r10.d]

; [r+[r].d],r
 @OC@.b [r2+[r0].d],r5
 @OC@.w [r2+[r5].d],r13
 @OC@.d [r2+[r10].d],r1

 @OC@ r13,[r2+[r5].d]
 @OC@ r8,[r2+[r10].d]

; [r+[r+].d],r
 @OC@.b [r2+[r0+].d],r5
 @OC@.w [r2+[r5+].d],r13
 @OC@.d [r2+[r10+].d],r1

 @OC@.w [r2+[r5+].d],r13

 @OC@.d [r2+[r10+].d],r1

 @OC@ r0,[r2+[r5+].d]
 @OC@ r5,[r2+[r10+].d]

; [r+const],r

; Note that I forgot 16-bit offsets and 32-bit offsets here and later.
; Maybe add them later if it feels necessary.

 @OC@.b [r2+0],r5
 @OC@.b [r2+1],r5
 @OC@.b [r2+127],r5
 @OC@.b [r2+128],r5
 @OC@.b [r2+-1],r5
 @OC@.b [r2+-127],r5
 @OC@.b [r2+-128],r5
 @OC@.b [r2+255],r5

 @OC@.b [r2+42],r5
 @OC@.b [r2+-42],r5
 @OC@.b [r2-42],r5
 @OC@.b [r2+forty2],r5
 @OC@.b [r2+mforty2],r5
 @OC@.b [r2+-forty2],r5
 @OC@.b [r2+-mforty2],r5
 @OC@.b [r2-forty2],r5
 @OC@.b [r2-mforty2],r5
 @OC@.b [r2+externalsym],r5

; Note that I missed 32-bit offsets (except -32769) here and later.
; Maybe add them later if it feels necessary.

 @OC@.w [r2+0],r13
 @OC@.w [r2+1],r13
 @OC@.w [r2+127],r13
 @OC@.w [r2+128],r13
 @OC@.w [r2+-1],r13
 @OC@.w [r2-1],r13
 @OC@.w [r2+-127],r13
 @OC@.w [r2+-128],r13
 @OC@.w [r2+-129],r13
 @OC@.w [r2-127],r13
 @OC@.w [r2-128],r13
 @OC@.w [r2-129],r13
 @OC@.w [r2+255],r13
 @OC@.w [r2+-255],r13
 @OC@.w [r2-255],r13
 @OC@.w [r2+256],r13
 @OC@.w [r2-256],r13
 @OC@.w [r2+-8856],r13
 @OC@.w [r2-8856],r13
 @OC@.w [r2+8856],r13

 @OC@.w [r2+42],r13
 @OC@.w [r2+-42],r13
 @OC@.w [r2-42],r13
 @OC@.w [r2+forty2],r13
 @OC@.w [r2+mforty2],r13
 @OC@.w [r2+-forty2],r5
 @OC@.w [r2-forty2],r5
 @OC@.w [r2+-mforty2],r5

 @OC@.w [r2+three2767],r5
 @OC@.w [r2+three2767+1],r5
 @OC@.w [r2+three2767+2],r13
 @OC@.w [r2+-three2767],r13
 @OC@.w [r2+-(three2767+1)],r13
 @OC@.w [r2+-(three2767+2)],r5
 @OC@.w [r2-three2767],r13
 @OC@.w [r2-(three2767+1)],r13
 @OC@.w [r2-(three2767+2)],r5
 @OC@.w [r2+six5535],r5
 @OC@.w [r2+externalsym],r5

 @OC@.d [r2+0],r1
 @OC@.d [r2+1],r1
 @OC@.d [r2+127],r1
 @OC@.d [r2+128],r1
 @OC@.d [r2+-1],r1
 @OC@.d [r2-1],r1
 @OC@.d [r2+-127],r1
 @OC@.d [r2+-128],r1
 @OC@.d [r2-127],r1
 @OC@.d [r2-128],r1
 @OC@.d [r2+255],r1
 @OC@.d [r2+-255],r1
 @OC@.d [r2-255],r1
 @OC@.d [r2+256],r1
 @OC@.d [r2-256],r1
 @OC@.d [r2-8856],r1
 @OC@.d [r2+-256],r1
 @OC@.d [r2+-8856],r1
 @OC@.d [r2+8856],r1

 @OC@.d [r2+2781868],r1
 @OC@.d [r2+-2701867],r1

 @OC@.d [r2+0x9ec0ceac],r1
 @OC@.d [r2+-0x7ec0cead],r1
 @OC@.d [r2-0x7ec0cead],r1
 @OC@.d [r2+const_int_m32],r1
 @OC@.d [r2+const_int_32],r1

 @OC@.d [r2+42],r13
 @OC@.d [r2-42],r5
 @OC@.d [r2+-42],r5
 @OC@.d [r2+forty2],r5
 @OC@.d [r2+mforty2],r5
 @OC@.d [r2-forty2],r5
 @OC@.d [r2-mforty2],r5
 @OC@.d [r2+-forty2],r5
 @OC@.d [r2+-mforty2],r5

 @OC@.d [r2+three2767],r5
 @OC@.d [r2+three2767+1],r5
 @OC@.d [r2+three2767+2],r5
 @OC@.d [r2+-three2767],r5
 @OC@.d [r2+-(three2767+1)],r13
 @OC@.d [r2+-(three2767+2)],r13
 @OC@.d [r2-three2767],r5
 @OC@.d [r2-(three2767+1)],r13
 @OC@.d [r2-(three2767+2)],r13
 @OC@.d [r2+six5535],r5
 @OC@.d [r2+six5535+1],r13
 @OC@.d [r2+two701867],r5
 @OC@.d [r2+-two701867],r5
 @OC@.d [r2-two701867],r5

 @OC@.d [r2+externalsym],r5

 @OC@ r1,[r2+0]
 @OC@ r1,[r2+1]
 @OC@ r1,[r2+127]
 @OC@ r1,[r2+128]
 @OC@ r1,[r2+-1]
 @OC@ r1,[r2-1]
 @OC@ r1,[r2+-127]
 @OC@ r1,[r2+-128]
 @OC@ r1,[r2-127]
 @OC@ r1,[r2-128]
 @OC@ r1,[r2+255]
 @OC@ r1,[r2+-255]
 @OC@ r1,[r2-255]
 @OC@ r1,[r2+256]
 @OC@ r1,[r2-256]
 @OC@ r1,[r2-8856]
 @OC@ r1,[r2+-256]
 @OC@ r1,[r2+-8856]
 @OC@ r1,[r2+8856]

 @OC@ r1,[r2+2781868]
 @OC@ r1,[r2+-2701867]

 @OC@ r1,[r2+0x9ec0ceac]
 @OC@ r1,[r2+-0x7ec0cead]
 @OC@ r1,[r2-0x7ec0cead]
 @OC@ r1,[r2+const_int_m32]
 @OC@ r1,[r2+const_int_32]

 @OC@ r13,[r2+42]
 @OC@ r5,[r2-42]
 @OC@ r5,[r2+-42]
 @OC@ r5,[r2+forty2]
 @OC@ r5,[r2+mforty2]
 @OC@ r5,[r2-forty2]
 @OC@ r5,[r2-mforty2]
 @OC@ r5,[r2+-forty2]
 @OC@ r5,[r2+-mforty2]

 @OC@ r5,[r2+three2767]
 @OC@ r5,[r2+three2767+1]
 @OC@ r5,[r2+three2767+2]
 @OC@ r5,[r2+-three2767]
 @OC@ r13,[r2+-(three2767+1)]
 @OC@ r13,[r2+-(three2767+2)]
 @OC@ r5,[r2-three2767]
 @OC@ r13,[r2-(three2767+1)]
 @OC@ r13,[r2-(three2767+2)]
 @OC@ r5,[r2+six5535]
 @OC@ r13,[r2+six5535+1]
 @OC@ r5,[r2+two701867]
 @OC@ r5,[r2+-two701867]
 @OC@ r5,[r2-two701867]

 @OC@ r5,[r2+externalsym]

;;;;;;;;;;;;;;;;;
;
; [r+X],r,r

; [r+r.b],r,r

 @OC@.b [r2+r0.b],r5,r8
 @OC@.w [r2+r5.b],r13,r8
 @OC@.d [r2+r10.b],r1,r8

; [r+[r].b],r,r
 @OC@.b [r2+[r0].b],r5,r8
 @OC@.w [r2+[r5].b],r13,r8
 @OC@.d [r2+[r10].b],r1,r8

; [r+[r+].b],r,r
 @OC@.b [r2+[r0+].b],r5,r8
 @OC@.w [r2+[r5+].b],r13,r8
 @OC@.d [r2+[r10+].b],r1,r8

 @OC@.w [r2+[r5+].b],r13,r8

 @OC@.d [r2+[r10+].b],r1,r8

; [r+r.w],r,r

 @OC@.b [r2+r0.w],r5,r8
 @OC@.w [r2+r5.w],r13,r8
 @OC@.d [r2+r10.w],r1,r8

; [r+[r].w],r,r
 @OC@.b [r2+[r0].w],r5,r8
 @OC@.w [r2+[r5].w],r13,r8
 @OC@.d [r2+[r10].w],r1,r8

; [r+[r+].w],r,r
 @OC@.b [r2+[r0+].w],r5,r8
 @OC@.w [r2+[r5+].w],r13,r8
 @OC@.d [r2+[r10+].w],r1,r8

 @OC@.w [r2+[r5+].w],r13,r8

 @OC@.d [r2+[r10+].w],r1,r8

; [r+r.d],r,r

 @OC@.b [r2+r0.d],r5,r8
 @OC@.w [r2+r5.d],r13,r8
 @OC@.d [r2+r10.d],r1,r8

; [r+[r].d],r,r
 @OC@.b [r2+[r0].d],r5,r8
 @OC@.w [r2+[r5].d],r13,r8
 @OC@.d [r2+[r10].d],r1,r8

; [r+[r+].d],r,r
 @OC@.b [r2+[r0+].d],r5,r8
 @OC@.w [r2+[r5+].d],r13,r8
 @OC@.d [r2+[r10+].d],r1,r8

 @OC@.w [r2+[r5+].d],r13,r8

 @OC@.d [r2+[r10+].d],r1,r8

; [r+const],r,r
 @OC@.b [r2+0],r5,r8
 @OC@.b [r2+1],r5,r8
 @OC@.b [r2+127],r5,r8
 @OC@.b [r2+128],r5,r8
 @OC@.b [r2+-1],r5,r8
 @OC@.b [r2+-127],r5,r8
 @OC@.b [r2+-128],r5,r8
 @OC@.b [r2+255],r5,r8

 @OC@.b [r2+42],r5,r8
 @OC@.b [r2+-42],r5,r8
 @OC@.b [r2-42],r5,r8
 @OC@.b [r2+forty2],r5,r8
 @OC@.b [r2+mforty2],r5,r8
 @OC@.b [r2+-forty2],r5,r8
 @OC@.b [r2+-mforty2],r5,r8
 @OC@.b [r2-forty2],r5,r8
 @OC@.b [r2-mforty2],r5,r8
 @OC@.b [r2+externalsym],r5,r8

 @OC@.w [r2+0],r13,r8
 @OC@.w [r2+1],r13,r8
 @OC@.w [r2+127],r13,r8
 @OC@.w [r2+128],r13,r8
 @OC@.w [r2+-1],r13,r8
 @OC@.w [r2-1],r13,r8
 @OC@.w [r2+-127],r13,r8
 @OC@.w [r2+-128],r13,r8
 @OC@.w [r2+-129],r13,r8
 @OC@.w [r2-127],r13,r8
 @OC@.w [r2-128],r13,r8
 @OC@.w [r2-129],r13,r8
 @OC@.w [r2+255],r13,r8
 @OC@.w [r2+-255],r13,r8
 @OC@.w [r2-255],r13,r8
 @OC@.w [r2+256],r13,r8
 @OC@.w [r2-256],r13,r8
 @OC@.w [r2+-8856],r13,r8
 @OC@.w [r2-8856],r13,r8
 @OC@.w [r2+8856],r13,r8

 @OC@.w [r2+42],r13,r8
 @OC@.w [r2+-42],r13,r8
 @OC@.w [r2-42],r13,r8
 @OC@.w [r2+forty2],r13,r8
 @OC@.w [r2+mforty2],r13,r8
 @OC@.w [r2+-forty2],r5,r8
 @OC@.w [r2-forty2],r5,r8
 @OC@.w [r2+-mforty2],r5,r8

 @OC@.w [r2+three2767],r5,r8
 @OC@.w [r2+three2767+1],r5,r8
 @OC@.w [r2+three2767+2],r13,r8
 @OC@.w [r2+-three2767],r13,r8
 @OC@.w [r2+-(three2767+1)],r13,r8
 @OC@.w [r2+-(three2767+2)],r5,r8
 @OC@.w [r2-three2767],r13,r8
 @OC@.w [r2-(three2767+1)],r13,r8
 @OC@.w [r2-(three2767+2)],r5,r8
 @OC@.w [r2+six5535],r5,r8
 @OC@.w [r2+externalsym],r5,r8

 @OC@.d [r2+0],r1,r8
 @OC@.d [r2+1],r1,r8
 @OC@.d [r2+127],r1,r8
 @OC@.d [r2+128],r1,r8
 @OC@.d [r2+-1],r1,r8
 @OC@.d [r2-1],r1,r8
 @OC@.d [r2+-127],r1,r8
 @OC@.d [r2+-128],r1,r8
 @OC@.d [r2-127],r1,r8
 @OC@.d [r2-128],r1,r8
 @OC@.d [r2+255],r1,r8
 @OC@.d [r2+-255],r1,r8
 @OC@.d [r2-255],r1,r8
 @OC@.d [r2+256],r1,r8
 @OC@.d [r2-256],r1,r8
 @OC@.d [r2-8856],r1,r8
 @OC@.d [r2+-256],r1,r8
 @OC@.d [r2+-8856],r1,r8
 @OC@.d [r2+8856],r1,r8

 @OC@.d [r2+2781868],r1,r8
 @OC@.d [r2+-2701867],r1,r8

 @OC@.d [r2+0x9ec0ceac],r1,r8
 @OC@.d [r2+-0x7ec0cead],r1,r8
 @OC@.d [r2-0x7ec0cead],r1,r8
 @OC@.d [r2+const_int_m32],r1,r8
 @OC@.d [r2+const_int_32],r1,r8

 @OC@.d [r2+42],r13,r8
 @OC@.d [r2-42],r5,r8
 @OC@.d [r2+-42],r5,r8
 @OC@.d [r2+forty2],r5,r8
 @OC@.d [r2+mforty2],r5,r8
 @OC@.d [r2-forty2],r5,r8
 @OC@.d [r2-mforty2],r5,r8
 @OC@.d [r2+-forty2],r5,r8
 @OC@.d [r2+-mforty2],r5,r8

 @OC@.d [r2+three2767],r5,r8
 @OC@.d [r2+three2767+1],r5,r8
 @OC@.d [r2+three2767+2],r5,r8
 @OC@.d [r2+-three2767],r5,r8
 @OC@.d [r2+-(three2767+1)],r13,r8
 @OC@.d [r2+-(three2767+2)],r13,r8
 @OC@.d [r2-three2767],r5,r8
 @OC@.d [r2-(three2767+1)],r13,r8
 @OC@.d [r2-(three2767+2)],r13,r8
 @OC@.d [r2+six5535],r5,r8
 @OC@.d [r2+six5535+1],r13,r8
 @OC@.d [r2+two701867],r5,r8
 @OC@.d [r2+-two701867],r5,r8
 @OC@.d [r2-two701867],r5,r8

 @OC@.d [r2+externalsym],r5,r8

;;;;;;;;;;;;;;;;;
;
; [r=r+X],r

; [r=r+r.b],r

 @OC@.b [r12=r2+r0.b],r5
 @OC@.w [r12=r2+r5.b],r13
 @OC@.d [r12=r2+r10.b],r1

 @OC@ r13,[r12=r2+r5.b]
 @OC@ r1,[r12=r2+r10.b]

; [r=r+[r].b],r
 @OC@.b [r12=r2+[r0].b],r5
 @OC@.w [r12=r2+[r5].b],r13
 @OC@.d [r12=r2+[r10].b],r1

 @OC@ r4,[r12=r2+[r5].b]
 @OC@ r6,[r12=r2+[r10].b]

; [r=r+[r+].b],r
 @OC@.b [r12=r2+[r0+].b],r5
 @OC@.w [r12=r2+[r5+].b],r13
 @OC@.d [r12=r2+[r10+].b],r1

 @OC@.w [r12=r2+[r5+].b],r13

 @OC@.d [r12=r2+[r10+].b],r1

 @OC@ r3,[r12=r2+[r5+].b]
 @OC@ r2,[r12=r2+[r10+].b]

; [r=r+r.w],r

 @OC@.b [r12=r2+r0.w],r5
 @OC@.w [r12=r2+r5.w],r13
 @OC@.d [r12=r2+r10.w],r1

 @OC@ r5,[r12=r2+r5.w]
 @OC@ r8,[r12=r2+r10.w]

; [r=r+[r].w],r
 @OC@.b [r12=r2+[r0].w],r5
 @OC@.w [r12=r2+[r5].w],r13
 @OC@.d [r12=r2+[r10].w],r1

 @OC@ r4,[r12=r2+[r5].w]
 @OC@ r3,[r12=r2+[r10].w]

; [r=r+[r+].w],r
 @OC@.b [r12=r2+[r0+].w],r5
 @OC@.w [r12=r2+[r5+].w],r13
 @OC@.d [r12=r2+[r10+].w],r1

 @OC@.w [r12=r2+[r5+].w],r13

 @OC@.d [r12=r2+[r10+].w],r1

 @OC@ r2,[r12=r2+[r5+].w]
 @OC@ r7,[r12=r2+[r10+].w]

; [r=r+r.d],r

 @OC@.b [r12=r2+r0.d],r5
 @OC@.w [r12=r2+r5.d],r13
 @OC@.d [r12=r2+r10.d],r1

 @OC@ r4,[r12=r2+r5.d]
 @OC@ r8,[r12=r2+r10.d]

; [r=r+[r].d],r
 @OC@.b [r12=r2+[r0].d],r5
 @OC@.w [r12=r2+[r5].d],r13
 @OC@.d [r12=r2+[r10].d],r1

 @OC@ r2,[r12=r2+[r5].d]
 @OC@ r0,[r12=r2+[r10].d]

; [r=r+[r+].d],r
 @OC@.b [r12=r2+[r0+].d],r5
 @OC@.w [r12=r2+[r5+].d],r13
 @OC@.d [r12=r2+[r10+].d],r1

 @OC@.w [r12=r2+[r5+].d],r13

 @OC@.d [r12=r2+[r10+].d],r1

 @OC@ r3,[r12=r2+[r5+].d]
 @OC@ r2,[r12=r2+[r10+].d]

; [r=r+const],r
 @OC@.b [r12=r2+0],r5
 @OC@.b [r12=r2+1],r5
 @OC@.b [r12=r2+127],r5
 @OC@.b [r12=r2+128],r5
 @OC@.b [r12=r2+-1],r5
 @OC@.b [r12=r2+-127],r5
 @OC@.b [r12=r2+-128],r5
 @OC@.b [r12=r2+255],r5

 @OC@.b [r12=r2+42],r5
 @OC@.b [r12=r2+-42],r5
 @OC@.b [r12=r2-42],r5
 @OC@.b [r12=r2+forty2],r5
 @OC@.b [r12=r2+mforty2],r5
 @OC@.b [r12=r2+-forty2],r5
 @OC@.b [r12=r2+-mforty2],r5
 @OC@.b [r12=r2-forty2],r5
 @OC@.b [r12=r2-mforty2],r5
 @OC@.b [r12=r2+externalsym],r5

 @OC@.w [r12=r2+0],r13
 @OC@.w [r12=r2+1],r13
 @OC@.w [r12=r2+127],r13
 @OC@.w [r12=r2+128],r13
 @OC@.w [r12=r2+-1],r13
 @OC@.w [r12=r2-1],r13
 @OC@.w [r12=r2+-127],r13
 @OC@.w [r12=r2+-128],r13
 @OC@.w [r12=r2+-129],r13
 @OC@.w [r12=r2-127],r13
 @OC@.w [r12=r2-128],r13
 @OC@.w [r12=r2-129],r13
 @OC@.w [r12=r2+255],r13
 @OC@.w [r12=r2+-255],r13
 @OC@.w [r12=r2-255],r13
 @OC@.w [r12=r2+256],r13
 @OC@.w [r12=r2-256],r13
 @OC@.w [r12=r2+-8856],r13
 @OC@.w [r12=r2-8856],r13
 @OC@.w [r12=r2+8856],r13

 @OC@.w [r12=r2+42],r13
 @OC@.w [r12=r2+-42],r13
 @OC@.w [r12=r2-42],r13
 @OC@.w [r12=r2+forty2],r13
 @OC@.w [r12=r2+mforty2],r13
 @OC@.w [r12=r2+-forty2],r5
 @OC@.w [r12=r2-forty2],r5
 @OC@.w [r12=r2+-mforty2],r5

 @OC@.w [r12=r2+three2767],r5
 @OC@.w [r12=r2+three2767+1],r5
 @OC@.w [r12=r2+three2767+2],r13
 @OC@.w [r12=r2+-three2767],r13
 @OC@.w [r12=r2+-(three2767+1)],r13
 @OC@.w [r12=r2+-(three2767+2)],r5
 @OC@.w [r12=r2-three2767],r13
 @OC@.w [r12=r2-(three2767+1)],r13
 @OC@.w [r12=r2-(three2767+2)],r5
 @OC@.w [r12=r2+six5535],r5
 @OC@.w [r12=r2+externalsym],r5

 @OC@.d [r12=r2+0],r1
 @OC@.d [r12=r2+1],r1
 @OC@.d [r12=r2+127],r1
 @OC@.d [r12=r2+128],r1
 @OC@.d [r12=r2+-1],r1
 @OC@.d [r12=r2-1],r1
 @OC@.d [r12=r2+-127],r1
 @OC@.d [r12=r2+-128],r1
 @OC@.d [r12=r2-127],r1
 @OC@.d [r12=r2-128],r1
 @OC@.d [r12=r2+255],r1
 @OC@.d [r12=r2+-255],r1
 @OC@.d [r12=r2-255],r1
 @OC@.d [r12=r2+256],r1
 @OC@.d [r12=r2-256],r1
 @OC@.d [r12=r2-8856],r1
 @OC@.d [r12=r2+-256],r1
 @OC@.d [r12=r2+-8856],r1
 @OC@.d [r12=r2+8856],r1

 @OC@.d [r12=r2+2781868],r1
 @OC@.d [r12=r2+-2701867],r1

 @OC@.d [r12=r2+0x9ec0ceac],r1
 @OC@.d [r12=r2+-0x7ec0cead],r1
 @OC@.d [r12=r2-0x7ec0cead],r1
 @OC@.d [r12=r2+const_int_m32],r1
 @OC@.d [r12=r2+const_int_32],r1

 @OC@.d [r12=r2+42],r13
 @OC@.d [r12=r2-42],r5
 @OC@.d [r12=r2+-42],r5
 @OC@.d [r12=r2+forty2],r5
 @OC@.d [r12=r2+mforty2],r5
 @OC@.d [r12=r2-forty2],r5
 @OC@.d [r12=r2-mforty2],r5
 @OC@.d [r12=r2+-forty2],r5
 @OC@.d [r12=r2+-mforty2],r5

 @OC@.d [r12=r2+three2767],r5
 @OC@.d [r12=r2+three2767+1],r5
 @OC@.d [r12=r2+three2767+2],r5
 @OC@.d [r12=r2+-three2767],r5
 @OC@.d [r12=r2+-(three2767+1)],r13
 @OC@.d [r12=r2+-(three2767+2)],r13
 @OC@.d [r12=r2-three2767],r5
 @OC@.d [r12=r2-(three2767+1)],r13
 @OC@.d [r12=r2-(three2767+2)],r13
 @OC@.d [r12=r2+six5535],r5
 @OC@.d [r12=r2+six5535+1],r13
 @OC@.d [r12=r2+two701867],r5
 @OC@.d [r12=r2+-two701867],r5
 @OC@.d [r12=r2-two701867],r5

 @OC@.d [r12=r2+externalsym],r5

 @OC@ r1,[r12=r2+0]
 @OC@ r1,[r12=r2+1]
 @OC@ r1,[r12=r2+127]
 @OC@ r1,[r12=r2+128]
 @OC@ r1,[r12=r2+-1]
 @OC@ r1,[r12=r2-1]
 @OC@ r1,[r12=r2+-127]
 @OC@ r1,[r12=r2+-128]
 @OC@ r1,[r12=r2-127]
 @OC@ r1,[r12=r2-128]
 @OC@ r1,[r12=r2+255]
 @OC@ r1,[r12=r2+-255]
 @OC@ r1,[r12=r2-255]
 @OC@ r1,[r12=r2+256]
 @OC@ r1,[r12=r2-256]
 @OC@ r1,[r12=r2-8856]
 @OC@ r1,[r12=r2+-256]
 @OC@ r1,[r12=r2+-8856]
 @OC@ r1,[r12=r2+8856]

 @OC@ r1,[r12=r2+2781868]
 @OC@ r1,[r12=r2+-2701867]

 @OC@ r1,[r12=r2+0x9ec0ceac]
 @OC@ r1,[r12=r2+-0x7ec0cead]
 @OC@ r1,[r12=r2-0x7ec0cead]
 @OC@ r1,[r12=r2+const_int_m32]
 @OC@ r1,[r12=r2+const_int_32]

 @OC@ r13,[r12=r2+42]
 @OC@ r5,[r12=r2-42]
 @OC@ r5,[r12=r2+-42]
 @OC@ r5,[r12=r2+forty2]
 @OC@ r5,[r12=r2+mforty2]
 @OC@ r5,[r12=r2-forty2]
 @OC@ r5,[r12=r2-mforty2]
 @OC@ r5,[r12=r2+-forty2]
 @OC@ r5,[r12=r2+-mforty2]

 @OC@ r5,[r12=r2+three2767]
 @OC@ r5,[r12=r2+three2767+1]
 @OC@ r5,[r12=r2+three2767+2]
 @OC@ r5,[r12=r2+-three2767]
 @OC@ r13,[r12=r2+-(three2767+1)]
 @OC@ r13,[r12=r2+-(three2767+2)]
 @OC@ r5,[r12=r2-three2767]
 @OC@ r13,[r12=r2-(three2767+1)]
 @OC@ r13,[r12=r2-(three2767+2)]
 @OC@ r5,[r12=r2+six5535]
 @OC@ r13,[r12=r2+six5535+1]
 @OC@ r5,[r12=r2+two701867]
 @OC@ r5,[r12=r2+-two701867]
 @OC@ r5,[r12=r2-two701867]

 @OC@ r5,[r12=r2+externalsym]

;;;;;;;;;;;;;;;;;;;
;
; [[r(+)]],r

 @OC@.b [[r3]],r5
 @OC@.w [[r2]],r4
 @OC@.d [[r3]],r7

 @OC@ r4,[[r2]]
 @OC@ r7,[[r3]]

 @OC@.b [[r9+]],r7
 @OC@.w [[r3+]],r5
 @OC@.d [[r1+]],r6

 @OC@ r5,[[r3+]]
 @OC@ r6,[[r1+]]

 @OC@.b [externalsym],r5
 @OC@.w [externalsym],r4
 @OC@.d [externalsym],r7

 @OC@ r4,[externalsym]
 @OC@ r7,[externalsym]

 @OC@.b [notstart],r5
 @OC@.w [notstart],r4
 @OC@.d [notstart],r7

 @OC@ r3,[notstart]
 @OC@ r7,[notstart]

;;;;;;;;;;;;;;;;;;;
;
; [[r(+)]],r,r

 @OC@.b [[r3]],r5,r12
 @OC@.w [[r2]],r4,r9
 @OC@.d [[r3]],r7,r9

 @OC@.b [[r9+]],r7,r10
 @OC@.w [[r3+]],r5,r9
 @OC@.d [[r1+]],r6,r9

 @OC@.b [externalsym],r5,r7
 @OC@.w [externalsym],r4,r9
 @OC@.d [externalsym],r7,r9

 @OC@.b [notstart],r5,r9
 @OC@.w [notstart],r4,r12
 @OC@.d [notstart],r7,r9

end:
