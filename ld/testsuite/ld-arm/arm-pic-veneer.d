
.*:     file format.*

Disassembly of section .text:

00008000 <_start>:
    8000:	ea000000 	b	8008 <__foo_from_arm>

00008004 <foo>:
    8004:	46c0      	nop			\(mov r8, r8\)
    8006:	4770      	bx	lr

00008008 <__foo_from_arm>:
    8008:	e59fc004 	ldr	ip, \[pc, #4\]	; 8014 <__foo_from_arm\+0xc>
    800c:	e08cc00f 	add	ip, ip, pc
    8010:	e12fff1c 	bx	ip
    8014:	fffffff1 	.word	0xfffffff1
