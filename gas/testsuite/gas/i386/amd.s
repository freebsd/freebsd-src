#AMD 3DNow! instructions

.text
foo:
 prefetch	(%ebx)
 prefetchw	0x1000(,%esi,2)
 femms
 pavgusb	(%eax),%mm0
 pf2id		2(%eax),%mm1
 pfacc		0x100(%eax),%mm2
 pfadd		(%esi),%mm3
 pfcmpeq	2(%esi),%mm4
 pfcmpge	0x9090(%esi),%mm5
 pfcmpgt	(%ebp,%esi,2),%mm6
 pfmax		2(%ebp,%esi,2),%mm7
 pfmin		0x90909090(%ebp,%esi,2),%mm0
 pfmul		4,%mm1
 pfrcp		%cs:7(%ebx,%eax,8),%mm2
 pfrcpit1	%mm0,%mm3
 pfrcpit2	%mm1,%mm4
 pfrsqit1	%mm2,%mm5
 pfrsqrt	%mm3,%mm6
 pfsub		%mm4,%mm7
 pfsubr		%mm5,%mm0
 pi2fd		%mm6,%mm1
 pmulhrw	%mm7,%mm2

# This is a 3DNow! instruction, with a prefix, that isn't quite right
# Everything's good bar the opcode suffix
.byte 0x2e, 0x0f, 0x0f, 0x54, 0xc3, 0x07, 0xc3

# Pad out to a good alignment
 .byte 0x90,0x90,0x90,0x90,0x90,0x90
