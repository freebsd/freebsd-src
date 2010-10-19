        .section        .tbss,"awT",@nobits
        .align 4
	.globl x
        .type   x, @tls_object
        .size   x, 4
x:
        .zero   4
