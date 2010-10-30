
.*: .*file format elf32-(big|little)arm

Disassembly of section \.text:

00008000 <_start>:
    8000:	0a000002 	beq	8010 <__vfp11_veneer_0>

00008004 <__vfp11_veneer_0_r>:
    8004:	e1a02003 	mov	r2, r3
    8008:	ed927a00 	flds	s14, \[r2\]
    800c:	e12fff1e 	bx	lr

00008010 <__vfp11_veneer_0>:
    8010:	0e474a20 	fmacseq	s9, s14, s1
    8014:	eafffffa 	b	8004 <__vfp11_veneer_0_r>
