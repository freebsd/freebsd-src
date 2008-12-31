# $FreeBSD: src/lib/libc/sparc64/fpu/Makefile.inc,v 1.5.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/sparc64/fpu

CFLAGS+= -I${.CURDIR}/sparc64/sys

SRCS+=	fpu.c fpu_add.c fpu_compare.c fpu_div.c fpu_explode.c fpu_implode.c \
	fpu_mul.c fpu_qp.c fpu_reg.S fpu_sqrt.c fpu_subr.c
