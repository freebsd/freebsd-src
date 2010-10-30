
.*: .*file format elf32-(big|little)arm

Disassembly of section \.text:

00008000 <_start>:
    8000:	0a000001 	beq	800c <__vfp11_veneer_0>

00008004 <__vfp11_veneer_0_r>:
    8004:	ed927a00 	flds	s14, \[r2\]
    8008:	e12fff1e 	bx	lr

0000800c <__vfp11_veneer_0>:
    800c:	0e474a20 	fmacseq	s9, s14, s1
    8010:	eafffffb 	b	8004 <__vfp11_veneer_0_r>
