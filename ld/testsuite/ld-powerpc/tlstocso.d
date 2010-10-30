#source: tlstoc.s
#as: -a64
#ld: -shared -melf64ppc
#objdump: -dr
#target: powerpc64*-*-*

.*: +file format elf64-powerpc

Disassembly of section \.text:

.* <\.__tls_get_addr>:
.*	3d 82 00 00 	addis   r12,r2,0
.*	f8 41 00 28 	std     r2,40\(r1\)
.*	e9 6c 80 70 	ld      r11,-32656\(r12\)
.*	e8 4c 80 78 	ld      r2,-32648\(r12\)
.*	7d 69 03 a6 	mtctr   r11
.*	e9 6c 80 80 	ld      r11,-32640\(r12\)
.*	4e 80 04 20 	bctr

.* <_start>:
.*	38 62 80 08 	addi    r3,r2,-32760
.*	4b ff ff e1 	bl      .* <\.__tls_get_addr>
.*	e8 41 00 28 	ld      r2,40\(r1\)
.*	38 62 80 18 	addi    r3,r2,-32744
.*	4b ff ff d5 	bl      .* <\.__tls_get_addr>
.*	e8 41 00 28 	ld      r2,40\(r1\)
.*	38 62 80 28 	addi    r3,r2,-32728
.*	4b ff ff c9 	bl      .* <\.__tls_get_addr>
.*	e8 41 00 28 	ld      r2,40\(r1\)
.*	38 62 80 38 	addi    r3,r2,-32712
.*	4b ff ff bd 	bl      .* <\.__tls_get_addr>
.*	e8 41 00 28 	ld      r2,40\(r1\)
.*	39 23 80 40 	addi    r9,r3,-32704
.*	3d 23 00 00 	addis   r9,r3,0
.*	81 49 80 48 	lwz     r10,-32696\(r9\)
.*	e9 22 80 48 	ld      r9,-32696\(r2\)
.*	7d 49 18 2a 	ldx     r10,r9,r3
.*	e9 22 80 50 	ld      r9,-32688\(r2\)
.*	7d 49 6a 2e 	lhzx    r10,r9,r13
.*	89 4d 00 00 	lbz     r10,0\(r13\)
.*	3d 2d 00 00 	addis   r9,r13,0
.*	99 49 00 00 	stb     r10,0\(r9\)
.*	60 00 00 00 	nop
.*	00 00 00 00 .*
.*	00 01 02 18 .*
.*	7d 88 02 a6 	mflr    r12
.*	42 9f 00 05 	bcl-    20,4\*cr7\+so,.*
.*	7d 68 02 a6 	mflr    r11
.*	e8 4b ff f0 	ld      r2,-16\(r11\)
.*	7d 88 03 a6 	mtlr    r12
.*	7d 82 5a 14 	add     r12,r2,r11
.*	e9 6c 00 00 	ld      r11,0\(r12\)
.*	e8 4c 00 08 	ld      r2,8\(r12\)
.*	7d 69 03 a6 	mtctr   r11
.*	e9 6c 00 10 	ld      r11,16\(r12\)
.*	4e 80 04 20 	bctr
.*	60 00 00 00 	nop
.*	60 00 00 00 	nop
.*	60 00 00 00 	nop
.*	38 00 00 00 	li      r0,0
.*	4b ff ff c4 	b       .*
