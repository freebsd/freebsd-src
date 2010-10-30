@ Neon tests. Basic bitfield tests, using zero for as many registers/fields as
@ possible, but without causing instructions to be badly-formed.

	.arm
	.syntax unified
	.text

	.macro regs3_1 op opq vtype
	\op\vtype q0,q0,q0
	\opq\vtype q0,q0,q0
	\op\vtype d0,d0,d0
	.endm

	.macro dregs3_1 op vtype
	\op\vtype d0,d0,d0
	.endm

	.macro regn3_1 op operand2 vtype
	\op\vtype d0,q0,\operand2
	.endm

	.macro regl3_1 op operand2 vtype
	\op\vtype q0,d0,\operand2
	.endm

	.macro regw3_1 op operand2 vtype
	\op\vtype q0,q0,\operand2
	.endm

	.macro regs2_1 op opq vtype
	\op\vtype q0,q0
	\opq\vtype q0,q0
	\op\vtype d0,d0
	.endm

	.macro regs3_su_32 op opq
	regs3_1 \op \opq .s8
	regs3_1 \op \opq .s16
	regs3_1 \op \opq .s32
	regs3_1 \op \opq .u8
	regs3_1 \op \opq .u16
	regs3_1 \op \opq .u32
	.endm

	regs3_su_32 vaba vabaq
	regs3_su_32 vhadd vhaddq
	regs3_su_32 vrhadd vrhaddq
	regs3_su_32 vhsub vhsubq

	.macro regs3_su_64 op opq
	regs3_1 \op \opq .s8
	regs3_1 \op \opq .s16
	regs3_1 \op \opq .s32
	regs3_1 \op \opq .s64
	regs3_1 \op \opq .u8
	regs3_1 \op \opq .u16
	regs3_1 \op \opq .u32
	regs3_1 \op \opq .u64
	.endm

	regs3_su_64 vqadd vqaddq
	regs3_su_64 vqsub vqsubq
	regs3_su_64 vrshl vrshlq
	regs3_su_64 vqrshl vqrshlq

	regs3_su_64 vshl vshlq
	regs3_su_64 vqshl vqshlq

	.macro regs2i_1 op opq imm vtype
	\op\vtype q0,q0,\imm
	\opq\vtype q0,q0,\imm
	\op\vtype d0,d0,\imm
	.endm

	.macro regs2i_su_64 op opq imm
	regs2i_1 \op \opq \imm .s8
	regs2i_1 \op \opq \imm .s16
	regs2i_1 \op \opq \imm .s32
	regs2i_1 \op \opq \imm .s64
	regs2i_1 \op \opq \imm .u8
	regs2i_1 \op \opq \imm .u16
	regs2i_1 \op \opq \imm .u32
	regs2i_1 \op \opq \imm .u64
	.endm

	.macro regs2i_i_64 op opq imm
	regs2i_1 \op \opq \imm .i8
	regs2i_1 \op \opq \imm .i16
	regs2i_1 \op \opq \imm .i32
	regs2i_1 \op \opq \imm .s32
	regs2i_1 \op \opq \imm .u32
	regs2i_1 \op \opq \imm .i64
	.endm

	regs2i_i_64 vshl vshlq 0
	regs2i_su_64 vqshl vqshlq 0

	.macro regs3_ntyp op opq
	regs3_1 \op \opq .8
	.endm

	regs3_ntyp vand vandq
	regs3_ntyp vbic vbicq
	regs3_ntyp vorr vorrq
	regs3_ntyp vorn vornq
	regs3_ntyp veor veorq

	.macro logic_imm_1 op opq imm vtype
	\op\vtype q0,\imm
	\opq\vtype q0,\imm
	\op\vtype d0,\imm
	.endm

	.macro logic_imm op opq
	logic_imm_1 \op \opq 0x000000a5000000a5 .i64
	logic_imm_1 \op \opq 0x0000a5000000a500 .i64
	logic_imm_1 \op \opq 0x00a5000000a50000 .i64
	logic_imm_1 \op \opq 0xa5000000a5000000 .i64
	logic_imm_1 \op \opq 0x00a500a500a500a5 .i64
	logic_imm_1 \op \opq 0xa500a500a500a500 .i64
	logic_imm_1 \op \opq 0x000000ff .i32
	logic_imm_1 \op \opq 0x000000ff .s32
	logic_imm_1 \op \opq 0x000000ff .u32
	logic_imm_1 \op \opq 0x0000ff00 .i32
	logic_imm_1 \op \opq 0x00ff0000 .i32
	logic_imm_1 \op \opq 0xff000000 .i32
	logic_imm_1 \op \opq 0x00a500a5 .i32
	logic_imm_1 \op \opq 0xa500a500 .i32
	logic_imm_1 \op \opq 0x00ff .i16
	logic_imm_1 \op \opq 0xff00 .i16
	logic_imm_1 \op \opq 0x00 .i8
	.endm

	logic_imm vbic vbicq
	logic_imm vorr vorrq

	.macro logic_inv_imm op opq
	logic_imm_1 \op \opq 0xffffff5affffff5a .i64
	logic_imm_1 \op \opq 0xffff5affffff5aff .i64
	logic_imm_1 \op \opq 0xff5affffff5affff .i64
	logic_imm_1 \op \opq 0x5affffff5affffff .i64
	logic_imm_1 \op \opq 0xff5aff5aff5aff5a .i64
	logic_imm_1 \op \opq 0x5aff5aff5aff5aff .i64
	logic_imm_1 \op \opq 0xffffff00 .i32
	logic_imm_1 \op \opq 0xffffff00 .s32
	logic_imm_1 \op \opq 0xffffff00 .u32
	logic_imm_1 \op \opq 0xffff00ff .i32
	logic_imm_1 \op \opq 0xff00ffff .i32
	logic_imm_1 \op \opq 0x00ffffff .i32
	logic_imm_1 \op \opq 0xff5aff5a .i32
	logic_imm_1 \op \opq 0x5aff5aff .i32
	logic_imm_1 \op \opq 0xff00 .i16
	logic_imm_1 \op \opq 0x00ff .i16
	logic_imm_1 \op \opq 0xff .i8
	.endm

	logic_inv_imm vand vandq
	logic_inv_imm vorn vornq

	regs3_ntyp vbsl vbslq
	regs3_ntyp vbit vbitq
	regs3_ntyp vbif vbifq

	.macro regs3_suf_32 op opq
	regs3_1 \op \opq .s8
	regs3_1 \op \opq .s16
	regs3_1 \op \opq .s32
	regs3_1 \op \opq .u8
	regs3_1 \op \opq .u16
	regs3_1 \op \opq .u32
	regs3_1 \op \opq .f32
	.endm

	.macro regs3_if_32 op opq
	regs3_1 \op \opq .i8
	regs3_1 \op \opq .i16
	regs3_1 \op \opq .i32
	regs3_1 \op \opq .s32
	regs3_1 \op \opq .u32
	regs3_1 \op \opq .f32
	.endm

	regs3_suf_32 vabd vabdq
	regs3_suf_32 vmax vmaxq
	regs3_suf_32 vmin vminq

	regs3_suf_32 vcge vcgeq
	regs3_suf_32 vcgt vcgtq
	regs3_suf_32 vcle vcleq
	regs3_suf_32 vclt vcltq

	regs3_if_32 vceq vceqq

	.macro regs2i_sf_0 op opq
	regs2i_1 \op \opq 0 .s8
	regs2i_1 \op \opq 0 .s16
	regs2i_1 \op \opq 0 .s32
	regs2i_1 \op \opq 0 .f32
	.endm

	regs2i_sf_0 vcge vcgeq
	regs2i_sf_0 vcgt vcgtq
	regs2i_sf_0 vcle vcleq
	regs2i_sf_0 vclt vcltq

	.macro regs2i_if_0 op opq
	regs2i_1 \op \opq 0 .i8
	regs2i_1 \op \opq 0 .i16
	regs2i_1 \op \opq 0 .i32
	regs2i_1 \op \opq 0 .s32
	regs2i_1 \op \opq 0 .u32
	regs2i_1 \op \opq 0 .f32
	.endm

	regs2i_if_0 vceq vceqq

	.macro dregs3_suf_32 op
	dregs3_1 \op .s8
	dregs3_1 \op .s16
	dregs3_1 \op .s32
	dregs3_1 \op .u8
	dregs3_1 \op .u16
	dregs3_1 \op .u32
	dregs3_1 \op .f32
	.endm

	dregs3_suf_32 vpmax
	dregs3_suf_32 vpmin

	.macro sregs3_1 op opq vtype
	\op\vtype q0,q0,q0
	\opq\vtype q0,q0,q0
	\op\vtype d0,d0,d0
	.endm

	.macro sclr21_1 op opq vtype
	\op\vtype q0,q0,d0[0]
	\opq\vtype q0,q0,d0[0]
	\op\vtype d0,d0,d0[0]
	.endm

	.macro mul_incl_scalar op opq
	regs3_1 \op \opq .i8
	regs3_1 \op \opq .i16
	regs3_1 \op \opq .i32
	regs3_1 \op \opq .s32
	regs3_1 \op \opq .u32
	regs3_1 \op \opq .f32
	sclr21_1 \op \opq .i16
	sclr21_1 \op \opq .i32
	sclr21_1 \op \opq .s32
	sclr21_1 \op \opq .u32
	sclr21_1 \op \opq .f32
	.endm

	mul_incl_scalar vmla vmlaq
	mul_incl_scalar vmls vmlsq

	.macro dregs3_if_32 op
	dregs3_1 \op .i8
	dregs3_1 \op .i16
	dregs3_1 \op .i32
	dregs3_1 \op .s32
	dregs3_1 \op .u32
	dregs3_1 \op .f32
	.endm

	dregs3_if_32 vpadd

	.macro regs3_if_64 op opq
	regs3_1 \op \opq .i8
	regs3_1 \op \opq .i16
	regs3_1 \op \opq .i32
	regs3_1 \op \opq .s32
	regs3_1 \op \opq .u32
	regs3_1 \op \opq .i64
	regs3_1 \op \opq .f32
	.endm

	regs3_if_64 vadd vaddq
	regs3_if_64 vsub vsubq

	.macro regs3_sz_32 op opq
	regs3_1 \op \opq .8
	regs3_1 \op \opq .16
	regs3_1 \op \opq .32
	.endm

	regs3_sz_32 vtst vtstq

	.macro regs3_ifp_32 op opq
        regs3_1 \op \opq .i8
	regs3_1 \op \opq .i16
	regs3_1 \op \opq .i32
	regs3_1 \op \opq .s32
	regs3_1 \op \opq .u32
	regs3_1 \op \opq .f32
	regs3_1 \op \opq .p8
	.endm

	regs3_ifp_32 vmul vmulq

	.macro dqmulhs op opq
	regs3_1 \op \opq .s16
	regs3_1 \op \opq .s32
	sclr21_1 \op \opq .s16
	sclr21_1 \op \opq .s32
	.endm

	dqmulhs vqdmulh vqdmulhq
	dqmulhs vqrdmulh vqrdmulhq

	regs3_1 vacge vacgeq .f32
	regs3_1 vacgt vacgtq .f32
	regs3_1 vacle vacleq .f32
	regs3_1 vaclt vacltq .f32
	regs3_1 vrecps vrecpsq .f32
	regs3_1 vrsqrts vrsqrtsq .f32

	.macro regs2_sf_32 op opq
	regs2_1 \op \opq .s8
	regs2_1 \op \opq .s16
	regs2_1 \op \opq .s32
	regs2_1 \op \opq .f32
	.endm

	regs2_sf_32 vabs vabsq
	regs2_sf_32 vneg vnegq

	.macro rshift_imm op opq
	regs2i_1 \op \opq 7 .s8
	regs2i_1 \op \opq 15 .s16
	regs2i_1 \op \opq 31 .s32
	regs2i_1 \op \opq 63 .s64
	regs2i_1 \op \opq 7 .u8
	regs2i_1 \op \opq 15 .u16
	regs2i_1 \op \opq 31 .u32
	regs2i_1 \op \opq 63 .u64
	.endm

	rshift_imm vshr vshrq
	rshift_imm vrshr vrshrq
	rshift_imm vsra vsraq
	rshift_imm vrsra vrsraq

	regs2i_1 vsli vsliq 0 .8
	regs2i_1 vsli vsliq 0 .16
	regs2i_1 vsli vsliq 0 .32
	regs2i_1 vsli vsliq 0 .64

	regs2i_1 vsri vsriq 7 .8
	regs2i_1 vsri vsriq 15 .16
	regs2i_1 vsri vsriq 31 .32
	regs2i_1 vsri vsriq 63 .64

	regs2i_1 vqshlu vqshluq 0 .s8
	regs2i_1 vqshlu vqshluq 0 .s16
	regs2i_1 vqshlu vqshluq 0 .s32
	regs2i_1 vqshlu vqshluq 0 .s64

	.macro qrshift_imm op
	regn3_1 \op 7 .s16
	regn3_1 \op 15 .s32
	regn3_1 \op 31 .s64
	regn3_1 \op 7 .u16
	regn3_1 \op 15 .u32
	regn3_1 \op 31 .u64
	.endm

	.macro qrshiftu_imm op
	regn3_1 \op 7 .s16
	regn3_1 \op 15 .s32
	regn3_1 \op 31 .s64
	.endm

	.macro qrshifti_imm op
	regn3_1 \op 7 .i16
	regn3_1 \op 15 .i32
	regn3_1 \op 15 .s32
	regn3_1 \op 15 .u32
	regn3_1 \op 31 .i64
	.endm

	qrshift_imm vqshrn
	qrshift_imm vqrshrn
	qrshiftu_imm vqshrun
	qrshiftu_imm vqrshrun

	qrshifti_imm vshrn
	qrshifti_imm vrshrn

	regl3_1 vshll 1 .s8
	regl3_1 vshll 1 .s16
	regl3_1 vshll 1 .s32
	regl3_1 vshll 1 .u8
	regl3_1 vshll 1 .u16
	regl3_1 vshll 1 .u32

	regl3_1 vshll 8 .i8
	regl3_1 vshll 16 .i16
	regl3_1 vshll 32 .i32
	regl3_1 vshll 32 .s32
	regl3_1 vshll 32 .u32

	.macro convert op opr arg="" t1=".s32.f32" t2=".u32.f32" t3=".f32.s32" t4=".f32.u32"
	\op\t1 \opr,\opr\arg
	\op\t2 \opr,\opr\arg
	\op\t3 \opr,\opr\arg
	\op\t4 \opr,\opr\arg
	.endm

	convert vcvt q0
	convert vcvtq q0
	convert vcvt d0
	convert vcvt q0 ",1"
	convert vcvtq q0 ",1"
	convert vcvt d0 ",1"

	vmov q0,q0
	vmov d0,d0
	vmov.8 d0[0],r0
	vmov.16 d0[0],r0
	vmov.32 d0[0],r0
	vmov d0,r0,r0
	vmov.s8 r0,d0[0]
	vmov.s16 r0,d0[0]
	vmov.u8 r0,d0[0]
	vmov.u16 r0,d0[0]
	vmov.32 r0,d0[0]
	vmov r0,r1,d0

	.macro mov_imm op imm vtype
	\op\vtype q0,\imm
	\op\vtype d0,\imm
	.endm

	mov_imm vmov 0x00000077 .i32
	mov_imm vmov 0x00000077 .s32
	mov_imm vmov 0x00000077 .u32
	mov_imm vmvn 0x00000077 .i32
	mov_imm vmvn 0x00000077 .s32
	mov_imm vmvn 0x00000077 .u32
	mov_imm vmov 0x00007700 .i32
	mov_imm vmvn 0x00007700 .i32
	mov_imm vmov 0x00770000 .i32
	mov_imm vmvn 0x00770000 .i32
	mov_imm vmov 0x77000000 .i32
	mov_imm vmvn 0x77000000 .i32
	mov_imm vmov 0x0077 .i16
	mov_imm vmvn 0x0077 .i16
	mov_imm vmov 0x7700 .i16
	mov_imm vmvn 0x7700 .i16
	mov_imm vmov 0x000077ff .i32
	mov_imm vmvn 0x000077ff .i32
	mov_imm vmov 0x0077ffff .i32
	mov_imm vmvn 0x0077ffff .i32
	mov_imm vmov 0x77 .i8
	mov_imm vmov 0xff0000ff000000ff .i64
	mov_imm vmov 4.25 .f32

	mov_imm vmov 0xa5a5 .i16
	mov_imm vmvn 0xa5a5 .i16
	mov_imm vmov 0xa5a5a5a5 .i32
	mov_imm vmvn 0xa5a5a5a5 .i32
	mov_imm vmov 0x00a500a5 .i32
	mov_imm vmov 0xa500a500 .i32
	mov_imm vmov 0xa5a5a5a5a5a5a5a5 .i64
	mov_imm vmvn 0xa5a5a5a5a5a5a5a5 .i64
	mov_imm vmov 0x00a500a500a500a5 .i64
	mov_imm vmov 0xa500a500a500a500 .i64
	mov_imm vmov 0x000000a5000000a5 .i64
	mov_imm vmov 0x0000a5000000a500 .i64
	mov_imm vmov 0x00a5000000a50000 .i64
	mov_imm vmov 0xa5000000a5000000 .i64
	mov_imm vmov 0x0000a5ff0000a5ff .i64
	mov_imm vmov 0x00a5ffff00a5ffff .i64
	mov_imm vmov 0xa5ffffffa5ffffff .i64

	vmvn q0,q0
	vmvnq q0,q0
	vmvn d0,d0

	.macro long_ops op
	regl3_1 \op d0 .s8
	regl3_1 \op d0 .s16
	regl3_1 \op d0 .s32
	regl3_1 \op d0 .u8
	regl3_1 \op d0 .u16
	regl3_1 \op d0 .u32
	.endm

	long_ops vabal
	long_ops vabdl
	long_ops vaddl
	long_ops vsubl

	.macro long_mac op
	regl3_1 \op d0 .s8
	regl3_1 \op d0 .s16
	regl3_1 \op d0 .s32
	regl3_1 \op d0 .u8
	regl3_1 \op d0 .u16
	regl3_1 \op d0 .u32
	regl3_1 \op "d0[0]" .s16
	regl3_1 \op "d0[0]" .s32
	regl3_1 \op "d0[0]" .u16
	regl3_1 \op "d0[0]" .u32
	.endm

	long_mac vmlal
	long_mac vmlsl

	.macro wide_ops op
	regw3_1 \op d0 .s8
	regw3_1 \op d0 .s16
	regw3_1 \op d0 .s32
	regw3_1 \op d0 .u8
	regw3_1 \op d0 .u16
	regw3_1 \op d0 .u32
	.endm

	wide_ops vaddw
	wide_ops vsubw

	.macro narr_ops op
	regn3_1 \op q0 .i16
	regn3_1 \op q0 .i32
	regn3_1 \op q0 .s32
	regn3_1 \op q0 .u32
	regn3_1 \op q0 .i64
	.endm

	narr_ops vaddhn
	narr_ops vraddhn
	narr_ops vsubhn
	narr_ops vrsubhn

	.macro long_dmac op
	regl3_1 \op d0 .s16
	regl3_1 \op d0 .s32
	regl3_1 \op "d0[0]" .s16
	regl3_1 \op "d0[0]" .s32
	.endm

	long_dmac vqdmlal
	long_dmac vqdmlsl
	long_dmac vqdmull

	regl3_1 vmull d0 .s8
	regl3_1 vmull d0 .s16
	regl3_1 vmull d0 .s32
	regl3_1 vmull d0 .u8
	regl3_1 vmull d0 .u16
	regl3_1 vmull d0 .u32
	regl3_1 vmull d0 .p8
	regl3_1 vmull "d0[0]" .s16
	regl3_1 vmull "d0[0]" .s32
	regl3_1 vmull "d0[0]" .u16
	regl3_1 vmull "d0[0]" .u32

	vext.8 q0,q0,q0,0
	vextq.8 q0,q0,q0,0
	vext.8 d0,d0,d0,0
	vext.8 q0,q0,q0,8

	.macro revs op opq vtype
	\op\vtype q0,q0
	\opq\vtype q0,q0
	\op\vtype d0,d0
	.endm

	revs vrev64 vrev64q .8
	revs vrev64 vrev64q .16
	revs vrev64 vrev64q .32
	revs vrev32 vrev32q .8
	revs vrev32 vrev32q .16
	revs vrev16 vrev16q .8

	.macro dups op opq vtype
	\op\vtype q0,r0
	\opq\vtype q0,r0
	\op\vtype d0,r0
	\op\vtype q0,d0[0]
	\opq\vtype q0,d0[0]
	\op\vtype d0,d0[0]
	.endm

	dups vdup vdupq .8
	dups vdup vdupq .16
	dups vdup vdupq .32

	.macro binop_3typ op op1 op2 t1 t2 t3
	\op\t1 \op1,\op2
	\op\t2 \op1,\op2
	\op\t3 \op1,\op2
	.endm

	binop_3typ vmovl q0 d0 .s8 .s16 .s32
	binop_3typ vmovl q0 d0 .u8 .u16 .u32
	binop_3typ vmovn d0 q0 .i16 .i32 .i64
	vmovn.s32 d0, q0
	vmovn.u32 d0, q0
	binop_3typ vqmovn d0 q0 .s16 .s32 .s64
	binop_3typ vqmovn d0 q0 .u16 .u32 .u64
	binop_3typ vqmovun d0 q0 .s16 .s32 .s64

	.macro binops op opq vtype="" rhs="0"
	\op\vtype q0,q\rhs
	\opq\vtype q0,q\rhs
	\op\vtype d0,d\rhs
	.endm

	.macro regs2_sz_32 op opq
	binops \op \opq .8 1
	binops \op \opq .16 1
	binops \op \opq .32 1
	.endm

	regs2_sz_32 vzip vzipq
	regs2_sz_32 vuzp vuzpq

	.macro regs2_s_32 op opq
	binops \op \opq .s8
	binops \op \opq .s16
	binops \op \opq .s32
	.endm

	regs2_s_32 vqabs vqabsq
	regs2_s_32 vqneg vqnegq

	.macro regs2_su_32 op opq
	regs2_s_32 \op \opq
	binops \op \opq .u8
	binops \op \opq .u16
	binops \op \opq .u32
	.endm

	regs2_su_32 vpadal vpadalq
	regs2_su_32 vpaddl vpaddlq

	binops vrecpe vrecpeq .u32
	binops vrecpe vrecpeq .f32
	binops vrsqrte vrsqrteq .u32
	binops vrsqrte vrsqrteq .f32

	regs2_s_32 vcls vclsq

	.macro regs2_i_32 op opq
	binops \op \opq .i8
	binops \op \opq .i16
	binops \op \opq .i32
	binops \op \opq .s32
	binops \op \opq .u32
	.endm

	regs2_i_32 vclz vclzq

	binops vcnt vcntq .8

	binops vswp vswpq "" 1

	regs2_sz_32 vtrn vtrnq

	vtbl.8 d0,{d0},d0
	vtbx.8 d0,{d0},d0
	
