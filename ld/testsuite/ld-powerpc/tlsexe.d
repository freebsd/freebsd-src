#source: tls.s
#as: -a64
#ld: -melf64ppc tmpdir/libtlslib.so
#objdump: -dr
#target: powerpc64*-*-*

.*: +file format elf64-powerpc

Disassembly of section \.text:

.* <_start-0x1c>:
.*	3d 82 00 00 	addis   r12,r2,0
.*	f8 41 00 28 	std     r2,40\(r1\)
.*	e9 6c 80 48 	ld      r11,-32696\(r12\)
.*	e8 4c 80 50 	ld      r2,-32688\(r12\)
.*	7d 69 03 a6 	mtctr   r11
.*	e9 6c 80 58 	ld      r11,-32680\(r12\)
.*	4e 80 04 20 	bctr

.* <_start>:
.*	e8 62 80 10 	ld      r3,-32752\(r2\)
.*	60 00 00 00 	nop
.*	7c 63 6a 14 	add     r3,r3,r13
.*	38 62 80 18 	addi    r3,r2,-32744
.*	4b ff ff d5 	bl      .*
.*	e8 41 00 28 	ld      r2,40\(r1\)
.*	3c 6d 00 00 	addis   r3,r13,0
.*	60 00 00 00 	nop
.*	38 63 90 38 	addi    r3,r3,-28616
.*	3c 6d 00 00 	addis   r3,r13,0
.*	60 00 00 00 	nop
.*	38 63 10 00 	addi    r3,r3,4096
.*	39 23 80 40 	addi    r9,r3,-32704
.*	3d 23 00 00 	addis   r9,r3,0
.*	81 49 80 48 	lwz     r10,-32696\(r9\)
.*	e9 22 80 28 	ld      r9,-32728\(r2\)
.*	7d 49 18 2a 	ldx     r10,r9,r3
.*	3d 2d 00 00 	addis   r9,r13,0
.*	a1 49 90 58 	lhz     r10,-28584\(r9\)
.*	89 4d 90 60 	lbz     r10,-28576\(r13\)
.*	3d 2d 00 00 	addis   r9,r13,0
.*	99 49 90 68 	stb     r10,-28568\(r9\)
.*	3c 6d 00 00 	addis   r3,r13,0
.*	60 00 00 00 	nop
.*	38 63 90 00 	addi    r3,r3,-28672
.*	3c 6d 00 00 	addis   r3,r13,0
.*	60 00 00 00 	nop
.*	38 63 10 00 	addi    r3,r3,4096
.*	f9 43 80 08 	std     r10,-32760\(r3\)
.*	3d 23 00 00 	addis   r9,r3,0
.*	91 49 80 10 	stw     r10,-32752\(r9\)
.*	e9 22 80 08 	ld      r9,-32760\(r2\)
.*	7d 49 19 2a 	stdx    r10,r9,r3
.*	3d 2d 00 00 	addis   r9,r13,0
.*	b1 49 90 58 	sth     r10,-28584\(r9\)
.*	e9 4d 90 2a 	lwa     r10,-28632\(r13\)
.*	3d 2d 00 00 	addis   r9,r13,0
.*	a9 49 90 30 	lha     r10,-28624\(r9\)
.*	7d 89 02 a6 	mfctr   r12
.*	78 0b 1f 24 	rldicr  r11,r0,3,60
.*	34 40 80 00 	addic\.  r2,r0,-32768
.*	7d 8b 60 50 	subf    r12,r11,r12
.*	7c 42 fe 76 	sradi   r2,r2,63
.*	78 0b 17 64 	rldicr  r11,r0,2,61
.*	7c 42 58 38 	and     r2,r2,r11
.*	7d 8b 60 50 	subf    r12,r11,r12
.*	7d 8c 12 14 	add     r12,r12,r2
.*	3d 8c 00 01 	addis   r12,r12,1
.*	e9 6c 01 c4 	ld      r11,452\(r12\)
.*	39 8c 01 c4 	addi    r12,r12,452
.*	e8 4c 00 08 	ld      r2,8\(r12\)
.*	7d 69 03 a6 	mtctr   r11
.*	e9 6c 00 10 	ld      r11,16\(r12\)
.*	4e 80 04 20 	bctr
.*	38 00 00 00 	li      r0,0
.*	4b ff ff bc 	b       .*
