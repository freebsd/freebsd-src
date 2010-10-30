# Check for illegal conditional Neon instructions in ARM mode. The instructions
# which overlap with VFP are the tricky cases, so test those.

	.include "itblock.s"

	.syntax unified
	.text
func:
	itblock 4 eq
	vmoveq q0,q1
        vmoveq d0,d1
	vmoveq.i32 q0,#0
        vmoveq.i32 d0,#0
        @ Following four *can* be conditional.
	itblock 4 eq
        vmoveq.32 d0[1], r2
	vmoveq d0,r1,r2
        vmoveq.32 r2,d1[0]
        vmoveq r0,r1,d2

	.macro dyadic_eq op eq="eq" f32=".f32"
	itblock 2 eq
        \op\eq\f32 d0,d1,d2
        \op\eq\f32 q0,q1,q2
        .endm
       
        dyadic_eq vmul
        dyadic_eq vmla
        dyadic_eq vmls
        dyadic_eq vadd
        dyadic_eq vsub
        
	.macro monadic_eq op eq="eq" f32=".f32"
	itblock 2 eq
        \op\eq\f32 d0,d1
        \op\eq\f32 q0,q1
        .endm

	monadic_eq vabs
        monadic_eq vneg
        
        .macro cvt to from dot="."
	itblock 2 eq
        vcvteq\dot\to\dot\from d0,d1
        vcvteq\dot\to\dot\from q0,q1
        .endm
        
	cvt s32 f32
        cvt u32 f32
        cvt f32 s32
        cvt f32 u32

	itblock 4 eq
	vdupeq.32 d0,r1
	vdupeq.32 q0,r1
	vdupeq.32 d0,d1[0]
	vdupeq.32 q0,d1[1]
