# as: -march=armv6kt2
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]+> f60f 0000 	addw	r0, pc, #2048	; 0x800
0+004 <[^>]+> f20f 0900 	addw	r9, pc, #0	; 0x0
0+008 <[^>]+> f20f 4900 	addw	r9, pc, #1024	; 0x400
0+00c <[^>]+> f509 6880 	add.w	r8, r9, #1024	; 0x400
0+010 <[^>]+> f209 1801 	addw	r8, r9, #257	; 0x101
0+014 <[^>]+> f201 1301 	addw	r3, r1, #257	; 0x101
0+018 <[^>]+> f6af 0000 	subw	r0, pc, #2048	; 0x800
0+01c <[^>]+> f2af 0900 	subw	r9, pc, #0	; 0x0
0+020 <[^>]+> f2af 4900 	subw	r9, pc, #1024	; 0x400
0+024 <[^>]+> f5a9 6880 	sub.w	r8, r9, #1024	; 0x400
0+028 <[^>]+> f2a9 1801 	subw	r8, r9, #257	; 0x101
0+02c <[^>]+> f2a1 1301 	subw	r3, r1, #257	; 0x101
0+030 <[^>]+> f103 0301 	add.w	r3, r3, #1	; 0x1
0+034 <[^>]+> f1a3 0301 	sub.w	r3, r3, #1	; 0x1
0+038 <[^>]+> b0c0      	sub	sp, #256
0+03a <[^>]+> f5ad 7d00 	sub.w	sp, sp, #512	; 0x200
0+03e <[^>]+> f2ad 1d01 	subw	sp, sp, #257	; 0x101
0+042 <[^>]+> b040      	add	sp, #256
0+044 <[^>]+> f50d 7d00 	add.w	sp, sp, #512	; 0x200
0+048 <[^>]+> f20d 1d01 	addw	sp, sp, #257	; 0x101
0+04c <[^>]+> a840      	add	r0, sp, #256
0+04e <[^>]+> f50d 6580 	add.w	r5, sp, #1024	; 0x400
0+052 <[^>]+> f20d 1901 	addw	r9, sp, #257	; 0x101
0+056 <[^>]+> 4271      	negs	r1, r6
