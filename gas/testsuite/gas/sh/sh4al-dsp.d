#as: -dsp
#objdump: -fdr --prefix-addresses --show-raw-insn
#name: SH4al DSP constructs

.*:     file format elf.*sh.*
architecture: sh4al-dsp, flags 0x00000010:
HAS_SYMS
start address 0x00000000

Disassembly of section \.text:
0x00000000 43 34       	ldrc	r3
0x00000002 4c 34       	ldrc	r12
0x00000004 8a 0a       	ldrc	#10
0x00000006 8a f3       	ldrc	#-13
0x00000008 00 98       	setdmx	
0x0000000a 00 c8       	setdmy	
0x0000000c 00 88       	clrdmxy	

0x0000000e f1 16       	movx\.w	@r4,x0	movy\.w	a0,@r7\+
0x00000010 f1 84       	movx\.w	@r0,x1
0x00000012 f3 48       	movx\.w	@r1\+,y0
0x00000014 f2 cc       	movx\.w	@r5\+r8,y1
0x00000016 f2 94       	movx\.l	@r5,x1
0x00000018 f1 14       	movx\.l	@r0,x0
0x0000001a f3 58       	movx\.l	@r1\+,y0
0x0000001c f0 dc       	movx\.l	@r4\+r8,y1

0x0000001e f0 2b       	movx\.w	a0,@r4\+	movy\.w	@r6\+r9,y0
0x00000020 f3 64       	movx\.w	x0,@r1
0x00000022 f1 a8       	movx\.w	a1,@r0\+
0x00000024 f2 ec       	movx\.w	x1,@r5\+r8
0x00000026 f2 34       	movx\.l	a0,@r5
0x00000028 f1 74       	movx\.l	x0,@r0
0x0000002a f3 f8       	movx\.l	x1,@r1\+
0x0000002c f0 bc       	movx\.l	a1,@r4\+r8

0x0000002e f1 ed       	movx\.w	a1,@r4\+r8	movy\.w	@r7,y1
0x00000030 f3 01       	movy\.w	@r3,y0
0x00000032 f2 c2       	movy\.w	@r2\+,x1
0x00000034 f0 83       	movy\.w	@r6\+r9,x0
0x00000036 f0 61       	movy\.l	@r6,y1
0x00000038 f2 21       	movy\.l	@r2,y0
0x0000003a f3 a2       	movy\.l	@r3\+,x0
0x0000003c f1 e3       	movy\.l	@r7\+r9,x1

0x0000003e f2 de       	movx\.w	@r5\+r8,x1	movy\.w	a1,@r6\+
0x00000040 f2 d1       	movy\.w	y1,@r2
0x00000042 f3 12       	movy\.w	a0,@r3\+
0x00000044 f1 93       	movy\.w	y0,@r7\+r9
0x00000046 f1 71       	movy\.l	a1,@r7
0x00000048 f3 b1       	movy\.l	y0,@r3
0x0000004a f2 f2       	movy\.l	y1,@r2\+
0x0000004c f0 33       	movy\.l	a0,@r6\+r9

0x0000004e f8 00 88 47 	pabs	x1,a0
0x00000052 f8 00 a8 0e 	pabs	y0,m1
0x00000056 f8 00 8a dc 	dct pabs	a1,m0
0x0000005a f8 00 8a 19 	dct pabs	x0,x1
0x0000005e f8 00 8b 9b 	dcf pabs	a0,y1
0x00000062 f8 00 8b 57 	dcf pabs	x1,a0
0x00000066 f8 00 aa 58 	dct pabs	y1,x0
0x0000006a f8 00 aa 6e 	dct pabs	m0,m1
0x0000006e f8 00 ab 7a 	dcf pabs	m1,y0
0x00000072 f8 00 ab 45 	dcf pabs	y0,a1
0x00000076 f8 00 4e 00 	pmuls	a1,x0,m0
0x0000007a f8 00 4b 04 	pmuls	y0,a1,m1
0x0000007e f8 00 8d 07 	pclr	a0
0x00000082 f8 00 8e 05 	dct pclr	a1
0x00000086 f8 00 4e 10 	pclr x0 	pmuls	a1,x0,m0
0x0000008a f8 00 40 1b 	pclr a1 	pmuls	x0,y0,a0
0x0000008e f8 00 45 1e 	pclr a0 	pmuls	x1,y1,a1
0x00000092 f8 00 4b 15 	pclr y0 	pmuls	y0,a1,m1
0x00000096 f8 00 a1 a8 	psub	a0,m0,x0
0x0000009a f8 00 85 79 	psub	m1,x1,x1
0x0000009e f8 00 85 8a 	psub	y0,a0,y0
0x000000a2 f8 00 a2 db 	dct psub	a1,y1,y1
0x000000a6 f8 00 86 67 	dct psub	m0,x1,a0
0x000000aa f8 00 86 95 	dct psub	y1,a0,a1
0x000000ae f8 00 a3 7c 	dcf psub	x1,m1,m0
0x000000b2 f8 00 87 4e 	dcf psub	y0,x1,m1
0x000000b6 f8 00 87 b5 	dcf psub	m1,a0,a1
0x000000ba f8 00 9d de 	pswap	a1,m1
0x000000be f8 00 9d 17 	pswap	x0,a0
0x000000c2 f8 00 bd 7a 	pswap	m1,y0
0x000000c6 f8 00 bd 49 	pswap	y0,x1
0x000000ca f8 00 9e 9b 	dct pswap	a0,y1
0x000000ce f8 00 9e 58 	dct pswap	x1,x0
0x000000d2 f8 00 be 55 	dct pswap	y1,a1
0x000000d6 f8 00 be 6c 	dct pswap	m0,m0
0x000000da f8 00 9f 97 	dcf pswap	a0,a0
0x000000de f8 00 9f 5e 	dcf pswap	x1,m1
0x000000e2 f8 00 bf 78 	dcf pswap	m1,x0
0x000000e6 f8 00 bf 4b 	dcf pswap	y0,y1
0x000000ea f8 00 98 85 	prnd	a0,a1
0x000000ee f8 00 b8 1c 	prnd	y1,m0
0x000000f2 f8 00 9a d8 	dct prnd	a1,x0
0x000000f6 f8 00 9a 1b 	dct prnd	x0,y1
0x000000fa f8 00 ba 77 	dct prnd	m1,a0
0x000000fe f8 00 ba 49 	dct prnd	y0,x1
0x00000102 f8 00 9b 9a 	dcf prnd	a0,y0
0x00000106 f8 00 9b 5e 	dcf prnd	x1,m1
0x0000010a f8 00 bb 57 	dcf prnd	y1,a0
0x0000010e f8 00 bb 65 	dcf prnd	m0,a1
0x00000112 00 09       	nop	
