; @OC@ test
; Generic unary operations supporting all sizes and their various
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
; r
 @OC@.b r3
 @OC@.w r5
 @OC@.d r10

 @OC@ r7
 @OC@ r6

;;;;;;;;;;;;;;;;;
;
; [r]
 @OC@.b [r0]
 @OC@.w [r5]
 @OC@.d [r10]

 @OC@ [r0]
 @OC@ [r3]

;;;;;;;;;;;;;;;;;
;
; [r+]
 @OC@.b [r0+]
 @OC@.w [r5+]
 @OC@.d [r10+]

 @OC@ [r4+]
 @OC@ [r1+]

;;;;;;;;;;;;;;;;;
;
; [r+X]

; [r+r.b]

 @OC@.b [r2+r0.b]
 @OC@.w [r2+r5.b]
 @OC@.d [r2+r10.b]

 @OC@ [r11+r13.b]
 @OC@ [r2+r10.b]

; [r+[r].b]
 @OC@.b [r2+[r0].b]
 @OC@.w [r2+[r5].b]
 @OC@.d [r2+[r10].b]

 @OC@ [r12+[r5].b]
 @OC@ [r13+[r10].b]

; [r+[r+].b]
 @OC@.b [r2+[r0+].b]
 @OC@.w [r2+[r5+].b]
 @OC@.d [r2+[r10+].b]

 @OC@ [r2+[r13+].b]
 @OC@ [r12+[r0+].b]

; [r+r.w]

 @OC@.b [r2+r0.w]
 @OC@.w [r2+r5.w]
 @OC@.d [r2+r10.w]

 @OC@ [r5+r11.w]
 @OC@ [r1+r1.w]

; [r+[r].w]
 @OC@.b [r2+[r0].w]
 @OC@.w [r2+[r5].w]
 @OC@.d [r2+[r10].w]

 @OC@ [r0+[r0].w]
 @OC@ [r2+[r7].w]

; [r+[r+].w]
 @OC@.b [r2+[r0+].w]
 @OC@.w [r2+[r5+].w]
 @OC@.d [r2+[r10+].w]

 @OC@ [r2+[r3+].w]
 @OC@ [r7+[r8+].w]

; [r+r.d]

 @OC@.b [r2+r0.d]
 @OC@.w [r2+r5.d]
 @OC@.d [r2+r10.d]

 @OC@ [r2+r5.d]
 @OC@ [r3+r10.d]

; [r+[r].d]
 @OC@.b [r2+[r0].d]
 @OC@.w [r2+[r5].d]
 @OC@.d [r2+[r10].d]

 @OC@ [r5+[r2].d]
 @OC@ [r12+[r10].d]

; [r+[r+].d]
 @OC@.b [r2+[r0+].d]
 @OC@.w [r2+[r5+].d]
 @OC@.d [r2+[r10+].d]

 @OC@ [r1+[r5+].d]
 @OC@ [r2+[r10+].d]

; [r+const]

; Note that I forgot 16-bit offsets and 32-bit offsets here and later.
; Maybe add them later if it feels necessary.

 @OC@.b [r2+0]
 @OC@.b [r2+1]
 @OC@.b [r2+127]
 @OC@.b [r2+128]
 @OC@.b [r2+-1]
 @OC@.b [r2+-127]
 @OC@.b [r2+-128]
 @OC@.b [r2+255]

 @OC@.b [r2+42]
 @OC@.b [r2+-42]
 @OC@.b [r2-42]
 @OC@.b [r2+forty2]
 @OC@.b [r2+mforty2]
 @OC@.b [r2+-forty2]
 @OC@.b [r2+-mforty2]
 @OC@.b [r2-forty2]
 @OC@.b [r2-mforty2]
 @OC@.b [r2+externalsym]

; Note that I missed 32-bit offsets (except -32769) here and later.
; Maybe add them later if it feels necessary.

 @OC@.w [r2+0]
 @OC@.w [r2+1]
 @OC@.w [r2+127]
 @OC@.w [r2+128]
 @OC@.w [r2+-1]
 @OC@.w [r2-1]
 @OC@.w [r2+-127]
 @OC@.w [r2+-128]
 @OC@.w [r2+-129]
 @OC@.w [r2-127]
 @OC@.w [r2-128]
 @OC@.w [r2-129]
 @OC@.w [r2+255]
 @OC@.w [r2+-255]
 @OC@.w [r2-255]
 @OC@.w [r2+256]
 @OC@.w [r2-256]
 @OC@.w [r2+-8856]
 @OC@.w [r2-8856]
 @OC@.w [r2+8856]

 @OC@.w [r2+42]
 @OC@.w [r2+-42]
 @OC@.w [r2-42]
 @OC@.w [r2+forty2]
 @OC@.w [r2+mforty2]
 @OC@.w [r2+-forty2]
 @OC@.w [r2-forty2]
 @OC@.w [r2+-mforty2]

 @OC@.w [r2+three2767]
 @OC@.w [r2+three2767+1]
 @OC@.w [r2+three2767+2]
 @OC@.w [r2+-three2767]
 @OC@.w [r2+-(three2767+1)]
 @OC@.w [r2+-(three2767+2)]
 @OC@.w [r2-three2767]
 @OC@.w [r2-(three2767+1)]
 @OC@.w [r2-(three2767+2)]
 @OC@.w [r2+six5535]
 @OC@.w [r2+externalsym]

 @OC@.d [r2+0]
 @OC@.d [r2+1]
 @OC@.d [r2+127]
 @OC@.d [r2+128]
 @OC@.d [r2+-1]
 @OC@.d [r2-1]
 @OC@.d [r2+-127]
 @OC@.d [r2+-128]
 @OC@.d [r2-127]
 @OC@.d [r2-128]
 @OC@.d [r2+255]
 @OC@.d [r2+-255]
 @OC@.d [r2-255]
 @OC@.d [r2+256]
 @OC@.d [r2-256]
 @OC@.d [r2-8856]
 @OC@.d [r2+-256]
 @OC@.d [r2+-8856]
 @OC@.d [r2+8856]

 @OC@.d [r2+2781868]
 @OC@.d [r2+-2701867]

 @OC@.d [r2+0x9ec0ceac]
 @OC@.d [r2+-0x7ec0cead]
 @OC@.d [r2-0x7ec0cead]
 @OC@.d [r2+const_int_m32]
 @OC@.d [r2+const_int_32]

 @OC@.d [r2+42]
 @OC@.d [r2-42]
 @OC@.d [r2+-42]
 @OC@.d [r2+forty2]
 @OC@.d [r2+mforty2]
 @OC@.d [r2-forty2]
 @OC@.d [r2-mforty2]
 @OC@.d [r2+-forty2]
 @OC@.d [r2+-mforty2]

 @OC@.d [r2+three2767]
 @OC@.d [r2+three2767+1]
 @OC@.d [r2+three2767+2]
 @OC@.d [r2+-three2767]
 @OC@.d [r2+-(three2767+1)]
 @OC@.d [r2+-(three2767+2)]
 @OC@.d [r2-three2767]
 @OC@.d [r2-(three2767+1)]
 @OC@.d [r2-(three2767+2)]
 @OC@.d [r2+six5535]
 @OC@.d [r2+six5535+1]
 @OC@.d [r2+two701867]
 @OC@.d [r2+-two701867]
 @OC@.d [r2-two701867]

 @OC@.d [r2+externalsym]

 @OC@ [r2+0]
 @OC@ [r2+1]
 @OC@ [r2+127]
 @OC@ [r2+128]
 @OC@ [r2+-1]
 @OC@ [r2-1]
 @OC@ [r2+-127]
 @OC@ [r2+-128]
 @OC@ [r2-127]
 @OC@ [r2-128]
 @OC@ [r2+255]
 @OC@ [r2+-255]
 @OC@ [r2-255]
 @OC@ [r2+256]
 @OC@ [r2-256]
 @OC@ [r2-8856]
 @OC@ [r2+-256]
 @OC@ [r2+-8856]
 @OC@ [r2+8856]

 @OC@ [r2+2781868]
 @OC@ [r2+-2701867]

 @OC@ [r2+0x9ec0ceac]
 @OC@ [r2+-0x7ec0cead]
 @OC@ [r2-0x7ec0cead]
 @OC@ [r2+const_int_m32]
 @OC@ [r2+const_int_32]

 @OC@ [r2+42]
 @OC@ [r2-42]
 @OC@ [r2+-42]
 @OC@ [r2+forty2]
 @OC@ [r2+mforty2]
 @OC@ [r2-forty2]
 @OC@ [r2-mforty2]
 @OC@ [r2+-forty2]
 @OC@ [r2+-mforty2]

 @OC@ [r2+three2767]
 @OC@ [r2+three2767+1]
 @OC@ [r2+three2767+2]
 @OC@ [r2+-three2767]
 @OC@ [r2+-(three2767+1)]
 @OC@ [r2+-(three2767+2)]
 @OC@ [r2-three2767]
 @OC@ [r2-(three2767+1)]
 @OC@ [r2-(three2767+2)]
 @OC@ [r2+six5535]
 @OC@ [r2+six5535+1]
 @OC@ [r2+two701867]
 @OC@ [r2+-two701867]
 @OC@ [r2-two701867]

 @OC@ [r2+externalsym]

 @OC@ [r2+0]
 @OC@ [r2+1]
 @OC@ [r2+127]
 @OC@ [r2+128]
 @OC@ [r2+-1]
 @OC@ [r2-1]
 @OC@ [r2+-127]
 @OC@ [r2+-128]
 @OC@ [r2-127]
 @OC@ [r2-128]
 @OC@ [r2+255]
 @OC@ [r2+-255]
 @OC@ [r2-255]
 @OC@ [r2+256]
 @OC@ [r2-256]
 @OC@ [r2-8856]
 @OC@ [r2+-256]
 @OC@ [r2+-8856]
 @OC@ [r2+8856]

 @OC@ [r2+2781868]
 @OC@ [r2+-2701867]

 @OC@ [r2+0x9ec0ceac]
 @OC@ [r2+-0x7ec0cead]
 @OC@ [r2-0x7ec0cead]
 @OC@ [r2+const_int_m32]
 @OC@ [r2+const_int_32]

 @OC@ [r2+42]
 @OC@ [r2-42]
 @OC@ [r2+-42]
 @OC@ [r2+forty2]
 @OC@ [r2+mforty2]
 @OC@ [r2-forty2]
 @OC@ [r2-mforty2]
 @OC@ [r2+-forty2]
 @OC@ [r2+-mforty2]

 @OC@ [r2+three2767]
 @OC@ [r2+three2767+1]
 @OC@ [r2+three2767+2]
 @OC@ [r2+-three2767]
 @OC@ [r2+-(three2767+1)]
 @OC@ [r2+-(three2767+2)]
 @OC@ [r2-three2767]
 @OC@ [r2-(three2767+1)]
 @OC@ [r2-(three2767+2)]
 @OC@ [r2+six5535]
 @OC@ [r2+six5535+1]
 @OC@ [r2+two701867]
 @OC@ [r2+-two701867]
 @OC@ [r2-two701867]

 @OC@ [r2+externalsym]

;;;;;;;;;;;;;;;;;
;
; [r=r+X],r

; [r=r+r.b],r

 @OC@.b [r12=r2+r0.b]
 @OC@.w [r12=r2+r5.b]
 @OC@.d [r12=r2+r10.b]

 @OC@ [r1=r2+r3.b]
 @OC@ [r12=r2+r10.b]

; [r=r+[r].b],r
 @OC@.b [r12=r2+[r0].b]
 @OC@.w [r12=r2+[r5].b]
 @OC@.d [r12=r2+[r10].b]

 @OC@ [r0=r2+[r5].b]
 @OC@ [r3=r2+[r10].b]

; [r=r+[r+].b],r
 @OC@.b [r12=r2+[r0+].b]
 @OC@.w [r12=r2+[r5+].b]
 @OC@.d [r12=r2+[r10+].b]

 @OC@.w [r12=r2+[r5+].b]

 @OC@.d [r12=r2+[r10+].b]

 @OC@ [r5=r2+[r4+].b]
 @OC@ [r2=r4+[r7+].b]

; [r=r+r.w],r

 @OC@.b [r12=r2+r0.w]
 @OC@.w [r12=r2+r5.w]
 @OC@.d [r12=r2+r10.w]

 @OC@ [r12=r12+r5.w]
 @OC@ [r1=r3+r10.w]

; [r=r+[r].w],r
 @OC@.b [r12=r2+[r0].w]
 @OC@.w [r12=r2+[r5].w]
 @OC@.d [r12=r2+[r10].w]

 @OC@ [r12=r2+[r5].w]
 @OC@ [r12=r7+[r10].w]

; [r=r+[r+].w],r
 @OC@.b [r12=r2+[r0+].w]
 @OC@.w [r12=r2+[r5+].w]
 @OC@.d [r12=r2+[r10+].w]

 @OC@.w [r12=r2+[r5+].w]

 @OC@.d [r12=r2+[r10+].w]

 @OC@ [r12=r6+[r7+].w]
 @OC@ [r12=r3+[r1+].w]

; [r=r+r.d],r

 @OC@.b [r12=r2+r0.d]
 @OC@.w [r12=r2+r5.d]
 @OC@.d [r12=r2+r10.d]

 @OC@ [r4=r2+r5.d]
 @OC@ [r12=r2+r10.d]

; [r=r+[r].d],r
 @OC@.b [r12=r2+[r0].d]
 @OC@.w [r12=r2+[r5].d]
 @OC@.d [r12=r2+[r10].d]

 @OC@ [r12=r3+[r5].d]
 @OC@ [r12=r4+[r10].d]

; [r=r+[r+].d],r
 @OC@.b [r12=r2+[r0+].d]
 @OC@.w [r12=r2+[r5+].d]
 @OC@.d [r12=r2+[r10+].d]

 @OC@.w [r12=r2+[r5+].d]

 @OC@.d [r12=r2+[r10+].d]

 @OC@ [r12=r8+[r5+].d]
 @OC@ [r12=r9+[r10+].d]

; [r=r+const],r
 @OC@.b [r12=r2+0]
 @OC@.b [r12=r2+1]
 @OC@.b [r12=r2+127]
 @OC@.b [r12=r2+128]
 @OC@.b [r12=r2+-1]
 @OC@.b [r12=r2+-127]
 @OC@.b [r12=r2+-128]
 @OC@.b [r12=r2+255]

 @OC@.b [r12=r2+42]
 @OC@.b [r12=r2+-42]
 @OC@.b [r12=r2-42]
 @OC@.b [r12=r2+forty2]
 @OC@.b [r12=r2+mforty2]
 @OC@.b [r12=r2+-forty2]
 @OC@.b [r12=r2+-mforty2]
 @OC@.b [r12=r2-forty2]
 @OC@.b [r12=r2-mforty2]
 @OC@.b [r12=r2+externalsym]

 @OC@.w [r12=r2+0]
 @OC@.w [r12=r2+1]
 @OC@.w [r12=r2+127]
 @OC@.w [r12=r2+128]
 @OC@.w [r12=r2+-1]
 @OC@.w [r12=r2-1]
 @OC@.w [r12=r2+-127]
 @OC@.w [r12=r2+-128]
 @OC@.w [r12=r2+-129]
 @OC@.w [r12=r2-127]
 @OC@.w [r12=r2-128]
 @OC@.w [r12=r2-129]
 @OC@.w [r12=r2+255]
 @OC@.w [r12=r2+-255]
 @OC@.w [r12=r2-255]
 @OC@.w [r12=r2+256]
 @OC@.w [r12=r2-256]
 @OC@.w [r12=r2+-8856]
 @OC@.w [r12=r2-8856]
 @OC@.w [r12=r2+8856]

 @OC@.w [r12=r2+42]
 @OC@.w [r12=r2+-42]
 @OC@.w [r12=r2-42]
 @OC@.w [r12=r2+forty2]
 @OC@.w [r12=r2+mforty2]
 @OC@.w [r12=r2+-forty2]
 @OC@.w [r12=r2-forty2]
 @OC@.w [r12=r2+-mforty2]

 @OC@.w [r12=r2+three2767]
 @OC@.w [r12=r2+three2767+1]
 @OC@.w [r12=r2+three2767+2]
 @OC@.w [r12=r2+-three2767]
 @OC@.w [r12=r2+-(three2767+1)]
 @OC@.w [r12=r2+-(three2767+2)]
 @OC@.w [r12=r2-three2767]
 @OC@.w [r12=r2-(three2767+1)]
 @OC@.w [r12=r2-(three2767+2)]
 @OC@.w [r12=r2+six5535]
 @OC@.w [r12=r2+externalsym]

 @OC@.d [r12=r2+0]
 @OC@.d [r12=r2+1]
 @OC@.d [r12=r2+127]
 @OC@.d [r12=r2+128]
 @OC@.d [r12=r2+-1]
 @OC@.d [r12=r2-1]
 @OC@.d [r12=r2+-127]
 @OC@.d [r12=r2+-128]
 @OC@.d [r12=r2-127]
 @OC@.d [r12=r2-128]
 @OC@.d [r12=r2+255]
 @OC@.d [r12=r2+-255]
 @OC@.d [r12=r2-255]
 @OC@.d [r12=r2+256]
 @OC@.d [r12=r2-256]
 @OC@.d [r12=r2-8856]
 @OC@.d [r12=r2+-256]
 @OC@.d [r12=r2+-8856]
 @OC@.d [r12=r2+8856]

 @OC@.d [r12=r2+2781868]
 @OC@.d [r12=r2+-2701867]

 @OC@.d [r12=r2+0x9ec0ceac]
 @OC@.d [r12=r2+-0x7ec0cead]
 @OC@.d [r12=r2-0x7ec0cead]
 @OC@.d [r12=r2+const_int_m32]
 @OC@.d [r12=r2+const_int_32]

 @OC@.d [r12=r2+42]
 @OC@.d [r12=r2-42]
 @OC@.d [r12=r2+-42]
 @OC@.d [r12=r2+forty2]
 @OC@.d [r12=r2+mforty2]
 @OC@.d [r12=r2-forty2]
 @OC@.d [r12=r2-mforty2]
 @OC@.d [r12=r2+-forty2]
 @OC@.d [r12=r2+-mforty2]

 @OC@.d [r12=r2+three2767]
 @OC@.d [r12=r2+three2767+1]
 @OC@.d [r12=r2+three2767+2]
 @OC@.d [r12=r2+-three2767]
 @OC@.d [r12=r2+-(three2767+1)]
 @OC@.d [r12=r2+-(three2767+2)]
 @OC@.d [r12=r2-three2767]
 @OC@.d [r12=r2-(three2767+1)]
 @OC@.d [r12=r2-(three2767+2)]
 @OC@.d [r12=r2+six5535]
 @OC@.d [r12=r2+six5535+1]
 @OC@.d [r12=r2+two701867]
 @OC@.d [r12=r2+-two701867]
 @OC@.d [r12=r2-two701867]

 @OC@.d [r12=r2+externalsym]

 @OC@ [r12=r2+0]
 @OC@ [r12=r2+1]
 @OC@ [r12=r2+127]
 @OC@ [r12=r2+128]
 @OC@ [r12=r2+-1]
 @OC@ [r12=r2-1]
 @OC@ [r12=r2+-127]
 @OC@ [r12=r2+-128]
 @OC@ [r12=r2-127]
 @OC@ [r12=r2-128]
 @OC@ [r12=r2+255]
 @OC@ [r12=r2+-255]
 @OC@ [r12=r2-255]
 @OC@ [r12=r2+256]
 @OC@ [r12=r2-256]
 @OC@ [r12=r2-8856]
 @OC@ [r12=r2+-256]
 @OC@ [r12=r2+-8856]
 @OC@ [r12=r2+8856]

 @OC@ [r12=r2+2781868]
 @OC@ [r12=r2+-2701867]

 @OC@ [r12=r2+0x9ec0ceac]
 @OC@ [r12=r2+-0x7ec0cead]
 @OC@ [r12=r2-0x7ec0cead]
 @OC@ [r12=r2+const_int_m32]
 @OC@ [r12=r2+const_int_32]

 @OC@ [r12=r2+42]
 @OC@ [r12=r2-42]
 @OC@ [r12=r2+-42]
 @OC@ [r12=r2+forty2]
 @OC@ [r12=r2+mforty2]
 @OC@ [r12=r2-forty2]
 @OC@ [r12=r2-mforty2]
 @OC@ [r12=r2+-forty2]
 @OC@ [r12=r2+-mforty2]

 @OC@ [r12=r2+three2767]
 @OC@ [r12=r2+three2767+1]
 @OC@ [r12=r2+three2767+2]
 @OC@ [r12=r2+-three2767]
 @OC@ [r12=r2+-(three2767+1)]
 @OC@ [r12=r2+-(three2767+2)]
 @OC@ [r12=r2-three2767]
 @OC@ [r12=r2-(three2767+1)]
 @OC@ [r12=r2-(three2767+2)]
 @OC@ [r12=r2+six5535]
 @OC@ [r12=r2+six5535+1]
 @OC@ [r12=r2+two701867]
 @OC@ [r12=r2+-two701867]
 @OC@ [r12=r2-two701867]

 @OC@ [r12=r2+externalsym]

;;;;;;;;;;;;;;;;;;;
;
; [[r(+)]],r

 @OC@.b [[r3]]
 @OC@.w [[r2]]
 @OC@.d [[r3]]

 @OC@ [[r2]]
 @OC@ [[r3]]

 @OC@.b [[r9+]]
 @OC@.w [[r3+]]
 @OC@.d [[r1+]]

 @OC@ [[r3+]]
 @OC@ [[r1+]]

 @OC@.b [externalsym]
 @OC@.w [externalsym]
 @OC@.d [externalsym]

 @OC@ [externalsym]

 @OC@.b [notstart]
 @OC@.w [notstart]
 @OC@.d [notstart]

 @OC@ [notstart]

end:
