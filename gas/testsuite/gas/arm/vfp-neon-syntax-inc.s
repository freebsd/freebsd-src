@ VFP with Neon-style syntax
	.syntax unified

	.include "itblock.s"

func:
	.macro testvmov cond="" f32=".f32" f64=".f64"
	itblock 4 \cond
        vmov\cond\f32 s0,s1
        vmov\cond\f64 d0,d1
        vmov\cond\f32 s0,#0.25
        vmov\cond\f64 d0,#1.0
	itblock 4 \cond
        vmov\cond r0,s1
        vmov\cond s0,r1
        vmov\cond r0,r1,s2,s3
        vmov\cond s0,s1,r2,r4
	.endm

	@ Test VFP vmov variants. These can all be conditional.
	testvmov
	testvmov eq

	.macro monadic op cond="" f32=".f32" f64=".f64"
	itblock 2 \cond
	\op\cond\f32 s0,s1
	\op\cond\f64 d0,d1
	.endm

	.macro monadic_c op
	monadic \op
	monadic \op eq
	.endm

	.macro dyadic op cond="" f32=".f32" f64=".f64"
	itblock 2 \cond
	\op\cond\f32 s0,s1,s2
	\op\cond\f64 d0,d1,d2
	.endm

	.macro dyadic_c op
	dyadic \op
	dyadic \op eq
	.endm

	.macro dyadicz op cond="" f32=".f32" f64=".f64"
	itblock 2 \cond
	\op\cond\f32 s0,#0
	\op\cond\f64 d0,#0
	.endm

	.macro dyadicz_c op
	dyadicz \op
	dyadicz \op eq
	.endm

	monadic_c vsqrt
	monadic_c vabs
	monadic_c vneg
	monadic_c vcmp
	monadic_c vcmpe

	dyadic_c vnmul
	dyadic_c vnmla
	dyadic_c vnmls

	dyadic_c vmul
	dyadic_c vmla
	dyadic_c vmls

	dyadic_c vadd
	dyadic_c vsub

	dyadic_c vdiv

	dyadicz_c vcmp
	dyadicz_c vcmpe

	.macro cvtz cond="" s32=".s32" u32=".u32" f32=".f32" f64=".f64"
	itblock 4 \cond
	vcvtz\cond\s32\f32 s0,s1
	vcvtz\cond\u32\f32 s0,s1
	vcvtz\cond\s32\f64 s0,d1
	vcvtz\cond\u32\f64 s0,d1
	.endm

	cvtz
	cvtz eq

	.macro cvt cond="" s32=".s32" u32=".u32" f32=".f32" f64=".f64"
	itblock 4 \cond
	vcvt\cond\s32\f32 s0,s1
	vcvt\cond\u32\f32 s0,s1
	vcvt\cond\f32\s32 s0,s1
	vcvt\cond\f32\u32 s0,s1
	itblock 4 \cond
	vcvt\cond\f32\f64 s0,d1
	vcvt\cond\f64\f32 d0,s1
	vcvt\cond\s32\f64 s0,d1
	vcvt\cond\u32\f64 s0,d1
	itblock 2 \cond
	vcvt\cond\f64\s32 d0,s1
	vcvt\cond\f64\u32 d0,s1
	.endm

	cvt
	cvt eq

	.macro cvti cond="" s32=".s32" u32=".u32" f32=".f32" f64=".f64" s16=".s16" u16=".u16"
	itblock 4 \cond
	vcvt\cond\s32\f32 s0,s0,#1
	vcvt\cond\u32\f32 s0,s0,#1
	vcvt\cond\f32\s32 s0,s0,#1
	vcvt\cond\f32\u32 s0,s0,#1
	itblock 4 \cond
	vcvt\cond\s32\f64 d0,d0,#1
	vcvt\cond\u32\f64 d0,d0,#1
	vcvt\cond\f64\s32 d0,d0,#1
	vcvt\cond\f64\u32 d0,d0,#1
	itblock 4 \cond
	vcvt\cond\f32\s16 s0,s0,#1
	vcvt\cond\f32\u16 s0,s0,#1
	vcvt\cond\f64\s16 d0,d0,#1
	vcvt\cond\f64\u16 d0,d0,#1
	itblock 4 \cond
	vcvt\cond\s16\f32 s0,s0,#1
	vcvt\cond\u16\f32 s0,s0,#1
	vcvt\cond\s16\f64 d0,d0,#1
	vcvt\cond\u16\f64 d0,d0,#1
	.endm

	cvti
	cvti eq

	.macro multi op cond="" n="" ia="ia" db="db"
	itblock 4 \cond
	\op\n\cond r0,{s3-s6}
	\op\ia\cond r0,{s3-s6}
	\op\ia\cond r0!,{s3-s6}
	\op\db\cond r0!,{s3-s6}
	itblock 4 \cond
	\op\n\cond r0,{d3-d6}
	\op\ia\cond r0,{d3-d6}
	\op\ia\cond r0!,{d3-d6}
	\op\db\cond r0!,{d3-d6}
	.endm

	multi vldm
	multi vldm eq
	multi vstm
	multi vstm eq

	.macro single op cond=""
	itblock 2 \cond
	\op\cond s0,[r0,#4]
	\op\cond d0,[r0,#4]
	.endm

	single vldr
	single vldr eq
	single vstr
	single vstr eq
