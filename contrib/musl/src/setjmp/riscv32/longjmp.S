.global __longjmp
.global _longjmp
.global longjmp
.type __longjmp, %function
.type _longjmp,  %function
.type longjmp,   %function
__longjmp:
_longjmp:
longjmp:
	lw s0,    0(a0)
	lw s1,    4(a0)
	lw s2,    8(a0)
	lw s3,    12(a0)
	lw s4,    16(a0)
	lw s5,    20(a0)
	lw s6,    24(a0)
	lw s7,    28(a0)
	lw s8,    32(a0)
	lw s9,    36(a0)
	lw s10,   40(a0)
	lw s11,   44(a0)
	lw sp,    48(a0)
	lw ra,    52(a0)

#ifndef __riscv_float_abi_soft
#ifdef __riscv_float_abi_double
#define FLX fld
#else
#define FLX flw
#endif

	FLX fs0,  56(a0)
	FLX fs1,  64(a0)
	FLX fs2,  72(a0)
	FLX fs3,  80(a0)
	FLX fs4,  88(a0)
	FLX fs5,  96(a0)
	FLX fs6,  104(a0)
	FLX fs7,  112(a0)
	FLX fs8,  120(a0)
	FLX fs9,  128(a0)
	FLX fs10, 136(a0)
	FLX fs11, 144(a0)
#endif

	seqz a0, a1
	add a0, a0, a1
	ret
