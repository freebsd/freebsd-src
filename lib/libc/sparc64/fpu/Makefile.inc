# $FreeBSD$

.PATH: ${.CURDIR}/../libc/sparc64/fpu/

SRCS+=	fpu.c fpu_add.c fpu_compare.c fpu_div.c fpu_explode.c fpu_implode.c \
	fpu_mul.c fpu_reg.S fpu_sqrt.c fpu_subr.c
