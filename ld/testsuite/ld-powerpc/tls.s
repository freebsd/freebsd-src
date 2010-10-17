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
 addi 3,2,gd@got@tlsgd		#R_PPC64_GOT_TLSGD16	gd
 bl .__tls_get_addr		#R_PPC64_REL24		.__tls_get_addr
 nop

#LD
 addi 3,2,ld@got@tlsld		#R_PPC64_GOT_TLSLD16	ld
 bl .__tls_get_addr		#R_PPC64_REL24		.__tls_get_addr
 nop

#global syms
#GD
 addi 3,2,gd0@got@tlsgd		#R_PPC64_GOT_TLSGD16	gd0
 bl .__tls_get_addr		#R_PPC64_REL24		.__tls_get_addr
 nop

#LD
 addi 3,2,ld0@got@tlsld		#R_PPC64_GOT_TLSLD16	ld0
 bl .__tls_get_addr		#R_PPC64_REL24		.__tls_get_addr
 nop

 addi 9,3,ld0@dtprel		#R_PPC64_DTPREL16	ld0

 addis 9,3,ld1@dtprel@ha	#R_PPC64_DTPREL16_HA	ld1
 lwz 10,ld1@dtprel@l(9)		#R_PPC64_DTPREL16_LO	ld1

 ld 9,ld2@got@dtprel(2)		#R_PPC64_GOT_DTPREL16_DS ld2
 ldx 10,9,3

#IE
 ld 9,ie0@got@tprel(2)		#R_PPC64_GOT_TPREL16_DS	ie0
 lhzx 10,9,ie0@tls		#R_PPC64_TLS		ie0

#LE
 lbz 10,le0@tprel(13)		#R_PPC64_TPREL16	le0

 addis 9,13,le1@tprel@ha	#R_PPC64_TPREL16_HA	le1
 stb 10,le1@tprel@l(9)		#R_PPC64_TPREL16_LO	le1

#local syms
#GD
 addi 3,2,gd4@got@tlsgd		#R_PPC64_GOT_TLSGD16	gd4
 bl .__tls_get_addr		#R_PPC64_REL24		.__tls_get_addr
 nop

#LD
 addi 3,2,ld4@got@tlsld		#R_PPC64_GOT_TLSLD16	ld4
 bl .__tls_get_addr		#R_PPC64_REL24		.__tls_get_addr
 nop

 std 10,ld4@dtprel(3)		#R_PPC64_DTPREL16_DS	ld4

 addis 9,3,ld5@dtprel@ha	#R_PPC64_DTPREL16_HA	ld5
 stw 10,ld5@dtprel@l(9)		#R_PPC64_DTPREL16_LO	ld5

 ld 9,ld6@got@dtprel(2)		#R_PPC64_GOT_DTPREL16_DS ld6
 stdx 10,9,3

#IE
 ld 9,ie0@got@tprel(2)		#R_PPC64_GOT_TPREL16_DS	ie4
 sthx 10,9,ie0@tls		#R_PPC64_TLS		ie4

#LE
 lwa 10,le4@tprel(13)		#R_PPC64_TPREL16	le4

 addis 9,13,le5@tprel@ha	#R_PPC64_TPREL16_HA	le5
 lha 10,le5@tprel@l(9)		#R_PPC64_TPREL16_LO	le5

