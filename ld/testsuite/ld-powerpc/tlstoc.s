	.section ".tbss","awT",@nobits
	.global _start,gd0,ld0,ld1,ld2,ie0,le0,le1
	.align 3
gd0:	.space 8
ld0:	.space 8
ld1:	.space 8
ld2:	.space 8
ie0:	.space 8
le0:	.space 8
le1:	.space 8

	.section ".tdata","awT",@progbits
	.align 3
gd4:	.quad 0x123456789abcdef0
ld4:	.quad 0x23456789abcdef01
ld5:	.quad 0x3456789abcdef012
ld6:	.quad 0x456789abcdef0123
ie4:	.quad 0x56789abcdef01234
le4:	.quad 0x6789abcdef012345
le5:	.quad 0x789abcdef0123456

	.text
_start:
#extern syms
#GD
 addi 3,2,.Lgd@toc
 bl .__tls_get_addr
 nop
 .section .toc,"aw",@progbits
.Lgd:
 .quad gd@dtpmod
 .quad gd@dtprel
 .text
#LD
 addi 3,2,.Lld@toc
 bl .__tls_get_addr
 nop
 .section .toc,"aw",@progbits
.Lld:
 .quad ld@dtpmod
 .quad 0
 .text

#global syms
#GD
 addi 3,2,.Lgd0@toc
 bl .__tls_get_addr
 nop
 .section .toc,"aw",@progbits
.Lgd0:
 .quad gd0@dtpmod
 .quad gd0@dtprel
 .text
#LD
 addi 3,2,.Lld0@toc
 bl .__tls_get_addr
 nop
 .section .toc,"aw",@progbits
.Lld0:
 .quad ld0@dtpmod
 .quad 0
 .text

 addi 9,3,ld0@dtprel

 addis 9,3,ld1@dtprel@ha
 lwz 10,ld1@dtprel@l(9)

 ld 9,.Lld2@toc(2)
 ldx 10,9,3
 .section .toc,"aw",@progbits
.Lld2:
 .quad ld2@dtprel
 .text

#IE
 ld 9,.Lie0@toc(2)
 lhzx 10,9,.Lie0@tls
 .section .toc,"aw",@progbits
.Lie0:
 .quad ie0@tprel
 .text

#LE
 lbz 10,le0@tprel(13)		#R_PPC64_TPREL16	le0

 addis 9,13,le1@tprel@ha	#R_PPC64_TPREL16_HA	le1
 stb 10,le1@tprel@l(9)		#R_PPC64_TPREL16_LO	le1
