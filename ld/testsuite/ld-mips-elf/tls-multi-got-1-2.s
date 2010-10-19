.macro	one_sym	count
.globl	sym_2_\count
sym_2_\count:
	la	$2, sym_2_\count
.endm

.irp	thou,0,1,2,3,4,5,6,7,8,9
.irp	hund,0,1,2,3,4,5,6,7,8,9
.irp	tens,0,1,2,3,4,5,6,7,8,9
.irp	ones,0,1,2,3,4,5,6,7,8,9
one_sym	\thou\hund\tens\ones
.endr
.endr
.endr
.endr

tls_bits_2:
	addiu	$4,$28,%tlsgd(tlsvar_gd)
	addiu	$4,$28,%tlsldm(tlsvar_ld)
	addiu	$4,$2,%gottprel(tlsvar_ie)
