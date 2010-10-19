.macro	one_sym	count
.globl	sym_1_\count
sym_1_\count:
	la	$2, sym_1_\count
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

tls_bits_1:
	addiu	$4,$28,%tlsgd(tlsvar_gd)
	addiu	$4,$28,%tlsldm(tlsvar_ld)
	addiu	$4,$2,%gottprel(tlsvar_ie)

        .section                .tbss,"awT",@nobits
        .align  2
        .global tlsvar_gd
        .type   tlsvar_gd,@object
        .size   tlsvar_gd,4
tlsvar_gd:
        .space  4
        .global tlsvar_ie
        .type   tlsvar_ie,@object
        .size   tlsvar_ie,4
tlsvar_ie:
        .space  4
        .global tlsvar_ld
        .hidden tlsvar_ld
        .type   tlsvar_ld,@object
        .size   tlsvar_ld,4
tlsvar_ld:
        .word   1
