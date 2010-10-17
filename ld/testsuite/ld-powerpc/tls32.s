	.section ".tbss","awT",@nobits
	.global _start,gd0,ld0,ld1,ld2,ie0,le0,le1
	.align 2
gd0:	.space 4
ld0:	.space 4
ld1:	.space 4
ld2:	.space 4
ie0:	.space 4
le0:	.space 4
le1:	.space 4

	.section ".tdata","awT",@progbits
	.align 2
gd4:	.long 0x12345678
ld4:	.long 0x23456789
ld5:	.long 0x3456789a
ld6:	.long 0x456789ab
ie4:	.long 0x56789abc
le4:	.long 0x6789abcd
le5:	.long 0x789abcde

	.text
_start:
#extern syms
#GD
 addi 3,31,gd@got@tlsgd		#R_PPC_GOT_TLSGD16	gd
 bl __tls_get_addr		#R_PPC_REL24		__tls_get_addr

#LD
 addi 3,31,ld@got@tlsld		#R_PPC_GOT_TLSLD16	ld
 bl __tls_get_addr		#R_PPC_REL24		__tls_get_addr

#global syms
#GD
 addi 3,31,gd0@got@tlsgd	#R_PPC_GOT_TLSGD16	gd0
 bl __tls_get_addr@plt		#R_PPC_PLTREL24		__tls_get_addr

#LD
 addi 3,31,ld0@got@tlsld	#R_PPC_GOT_TLSLD16	ld0
 bl __tls_get_addr@plt		#R_PPC_PLTREL24		__tls_get_addr

 addi 9,3,ld0@dtprel		#R_PPC_DTPREL16		ld0

 addis 9,3,ld1@dtprel@ha	#R_PPC_DTPREL16_HA	ld1
 lwz 10,ld1@dtprel@l(9)		#R_PPC_DTPREL16_LO	ld1

#IE
 lwz 9,ie0@got@tprel(31)	#R_PPC_GOT_TPREL16	ie0
 lhzx 10,9,ie0@tls		#R_PPC_TLS		ie0

#LE
 lbz 10,le0@tprel(2)		#R_PPC_TPREL16		le0

 addis 9,2,le1@tprel@ha		#R_PPC_TPREL16_HA	le1
 stb 10,le1@tprel@l(9)		#R_PPC_TPREL16_LO	le1

#local syms, use a different got reg too.
#GD
 addi 3,30,gd4@got@tlsgd	#R_PPC_GOT_TLSGD16	gd4
 bl __tls_get_addr		#R_PPC_REL24		__tls_get_addr

#LD
 addi 3,30,ld4@got@tlsld	#R_PPC_GOT_TLSLD16	ld4
 bl __tls_get_addr		#R_PPC_REL24		__tls_get_addr

 stw 10,ld4@dtprel(3)		#R_PPC_DTPREL16	ld4

 addis 9,3,ld5@dtprel@ha	#R_PPC_DTPREL16_HA	ld5
 stw 10,ld5@dtprel@l(9)		#R_PPC_DTPREL16_LO	ld5

#IE
 lwz 9,ie0@got@tprel(30)	#R_PPC_GOT_TPREL16	ie4
 sthx 10,9,ie0@tls		#R_PPC_TLS		ie4

#LE
 lhz 10,le4@tprel(2)		#R_PPC_TPREL16	le4

 addis 9,2,le5@tprel@ha		#R_PPC_TPREL16_HA	le5
 lha 10,le5@tprel@l(9)		#R_PPC_TPREL16_LO	le5

