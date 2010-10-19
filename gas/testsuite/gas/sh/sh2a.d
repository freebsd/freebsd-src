#objdump: -dr --prefix-addresses --show-raw-insn
#name: SH2a new instructions
#as: -isa=sh2a

dump.o:     file format elf32-sh.*

Disassembly of section .text:
0x00000000 33 79 4f ff 	band.b	#7,@\(4095,r3\)
0x00000004 33 79 cf ff 	bandnot.b	#7,@\(4095,r3\)
0x00000008 33 79 0f ff 	bclr.b	#7,@\(4095,r3\)
0x0000000c 86 37       	bclr	#7,r3
0x0000000e 33 79 3f ff 	bld.b	#7,@\(4095,r3\)
0x00000012 87 3f       	bld	#7,r3
0x00000014 33 79 bf ff 	bldnot.b	#7,@\(4095,r3\)
0x00000018 33 79 5f ff 	bor.b	#7,@\(4095,r3\)
0x0000001c 33 79 df ff 	bornot.b	#7,@\(4095,r3\)
0x00000020 33 79 1f ff 	bset.b	#7,@\(4095,r3\)
0x00000024 86 3f       	bset	#7,r3
0x00000026 33 79 2f ff 	bst.b	#7,@\(4095,r3\)
0x0000002a 87 37       	bst	#7,r3
0x0000002c 33 79 6f ff 	bxor.b	#7,@\(4095,r3\)
0x00000030 43 91       	clips.b	r3
0x00000032 43 95       	clips.w	r3
0x00000034 43 81       	clipu.b	r3
0x00000036 43 85       	clipu.w	r3
0x00000038 43 94       	divs	r0,r3
0x0000003a 43 84       	divu	r0,r3
0x0000003c 33 31 3f ff 	fmov.s	fr3,@\(16380,r3\)
0x00000040 33 21 3f ff 	fmov.d	dr2,@\(32760,r3\)
0x00000044 33 31 7f ff 	fmov.s	@\(16380,r3\),fr3
0x00000048 32 31 7f ff 	fmov.d	@\(32760,r3\),dr2
0x0000004c 43 4b       	jsr/n	@r3
0x0000004e 83 ff       	jsr/n	@@\(1020,tbr\)
0x00000050 43 e5       	ldbank	@r3,r0
0x00000052 43 4a       	ldc	r3,tbr
0x00000054 34 31 0f ff 	mov.b	r3,@\(4095,r4\)
0x00000058 34 31 1f ff 	mov.w	r3,@\(8190,r4\)
0x0000005c 34 31 2f ff 	mov.l	r3,@\(16380,r4\)
0x00000060 35 41 4f ff 	mov.b	@\(4095,r4\),r5
0x00000064 35 41 5f ff 	mov.w	@\(8190,r4\),r5
0x00000068 35 41 6f ff 	mov.l	@\(16380,r4\),r5
0x0000006c 43 8b       	mov.b	r0,@r3\+
0x0000006e 43 9b       	mov.w	r0,@r3\+
0x00000070 43 ab       	mov.l	r0,@r3\+
0x00000072 43 cb       	mov.b	@-r3,r0
0x00000074 43 db       	mov.w	@-r3,r0
0x00000076 43 eb       	mov.l	@-r3,r0
0x00000078 03 70 ff ff 	movi20	#524287,r3
0x0000007c 03 80 00 00 	movi20	#-524288,r3
0x00000080 03 71 ff ff 	movi20s	#134217472,r3
0x00000084 03 81 00 00 	movi20s	#-134217728,r3
0x00000088 43 f1       	movml.l	r3,@-r15
0x0000008a 43 f5       	movml.l	@r15\+,r3
0x0000008c 43 f0       	movmu.l	r3,@-r15
0x0000008e 43 f4       	movmu.l	@r15\+,r3
0x00000090 03 39       	movrt	r3
0x00000092 34 31 8f ff 	movu.b	@\(4095,r3\),r4
0x00000096 34 31 9f ff 	movu.w	@\(8190,r3\),r4
0x0000009a 44 80       	mulr	r0,r4
0x0000009c 00 68       	nott	
0x0000009e 05 83       	pref	@r5
0x000000a0 00 5b       	resbank	
0x000000a2 00 6b       	rts/n	
0x000000a4 03 7b       	rtv/n	r3
0x000000a6 44 3c       	shad	r3,r4
0x000000a8 44 3d       	shld	r3,r4
0x000000aa 45 e1       	stbank	r0,@r5
0x000000ac 04 4a       	stc	tbr,r4
