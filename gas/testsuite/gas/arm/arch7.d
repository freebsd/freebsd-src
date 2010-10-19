#name: ARM V7 instructions
#as: -march=armv7r
#skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
#objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> f6d6f008 	pli	\[r6, r8\]
0+004 <[^>]*> f6d9f007 	pli	\[r9, r7\]
0+008 <[^>]*> f6d0f101 	pli	\[r0, r1, lsl #2\]
0+00c <[^>]*> f4d5f000 	pli	\[r5\]
0+010 <[^>]*> f4d5ffff 	pli	\[r5, #4095\]
0+014 <[^>]*> f455ffff 	pli	\[r5, #-4095\]
0+018 <[^>]*> e320f0f0 	dbg	#0
0+01c <[^>]*> e320f0ff 	dbg	#15
0+020 <[^>]*> f57ff05f 	dmb	sy
0+024 <[^>]*> f57ff05f 	dmb	sy
0+028 <[^>]*> f57ff04f 	dsb	sy
0+02c <[^>]*> f57ff04f 	dsb	sy
0+030 <[^>]*> f57ff047 	dsb	un
0+034 <[^>]*> f57ff04e 	dsb	st
0+038 <[^>]*> f57ff046 	dsb	unst
0+03c <[^>]*> f57ff06f 	isb	sy
0+040 <[^>]*> f57ff06f 	isb	sy
0+044 <[^>]*> f916 f008 	pli	\[r6, r8\]
0+048 <[^>]*> f919 f007 	pli	\[r9, r7\]
0+04c <[^>]*> f910 f021 	pli	\[r0, r1, lsl #2\]
0+050 <[^>]*> f995 f000 	pli	\[r5\]
0+054 <[^>]*> f995 ffff 	pli	\[r5, #4095\]
0+058 <[^>]*> f915 fcff 	pli	\[r5, #-255\]
0+05c <[^>]*> f99f ffff 	pli	\[pc, #4095\]	; 0000105f <[^>]*>
0+060 <[^>]*> f91f ffff 	pli	\[pc, #-4095\]	; fffff065 <[^>]*>
0+064 <[^>]*> f3af 80f0 	dbg	#0
0+068 <[^>]*> f3af 80ff 	dbg	#15
0+06c <[^>]*> f3bf 8f5f 	dmb	sy
0+070 <[^>]*> f3bf 8f5f 	dmb	sy
0+074 <[^>]*> f3bf 8f4f 	dsb	sy
0+078 <[^>]*> f3bf 8f4f 	dsb	sy
0+07c <[^>]*> f3bf 8f47 	dsb	un
0+080 <[^>]*> f3bf 8f4e 	dsb	st
0+084 <[^>]*> f3bf 8f46 	dsb	unst
0+088 <[^>]*> f3bf 8f6f 	isb	sy
0+08c <[^>]*> f3bf 8f6f 	isb	sy
0+090 <[^>]*> fb99 f6fc 	sdiv	r6, r9, ip
0+094 <[^>]*> fb96 f9f3 	sdiv	r9, r6, r3
0+098 <[^>]*> fbb6 f9f3 	udiv	r9, r6, r3
0+09c <[^>]*> fbb9 f6fc 	udiv	r6, r9, ip
# V7M APSR has the same encoding as V7A CPSR_f
0+0a0 <[^>]*> f3ef 8000 	mrs	r0, (CPSR|APSR)
0+0a4 <[^>]*> f3ef 8001 	mrs	r0, IAPSR
0+0a8 <[^>]*> f3ef 8002 	mrs	r0, EAPSR
0+0ac <[^>]*> f3ef 8003 	mrs	r0, PSR
0+0b0 <[^>]*> f3ef 8005 	mrs	r0, IPSR
0+0b4 <[^>]*> f3ef 8006 	mrs	r0, EPSR
0+0b8 <[^>]*> f3ef 8007 	mrs	r0, IEPSR
0+0bc <[^>]*> f3ef 8008 	mrs	r0, MSP
0+0c0 <[^>]*> f3ef 8009 	mrs	r0, PSP
0+0c4 <[^>]*> f3ef 8010 	mrs	r0, PRIMASK
0+0c8 <[^>]*> f3ef 8011 	mrs	r0, BASEPRI
0+0cc <[^>]*> f3ef 8012 	mrs	r0, BASEPRI_MASK
0+0d0 <[^>]*> f3ef 8013 	mrs	r0, FAULTMASK
0+0d4 <[^>]*> f3ef 8014 	mrs	r0, CONTROL
0+0d8 <[^>]*> f380 8800 	msr	(CPSR_f|APSR), r0
0+0dc <[^>]*> f380 8801 	msr	IAPSR, r0
0+0e0 <[^>]*> f380 8802 	msr	EAPSR, r0
0+0e4 <[^>]*> f380 8803 	msr	PSR, r0
0+0e8 <[^>]*> f380 8805 	msr	IPSR, r0
0+0ec <[^>]*> f380 8806 	msr	EPSR, r0
0+0f0 <[^>]*> f380 8807 	msr	IEPSR, r0
0+0f4 <[^>]*> f380 8808 	msr	MSP, r0
0+0f8 <[^>]*> f380 8809 	msr	PSP, r0
0+0fc <[^>]*> f380 8810 	msr	PRIMASK, r0
0+100 <[^>]*> f380 8811 	msr	BASEPRI, r0
0+104 <[^>]*> f380 8812 	msr	BASEPRI_MASK, r0
0+108 <[^>]*> f380 8813 	msr	FAULTMASK, r0
0+10c <[^>]*> f380 8814 	msr	CONTROL, r0
