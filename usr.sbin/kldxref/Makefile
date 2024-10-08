PACKAGE=	runtime
PROG=	kldxref
MAN=	kldxref.8
SRCS=	kldxref.c

CFLAGS+=-I${SRCTOP}/lib/libkldelf

WARNS?=	2

LIBADD=	elf kldelf

.include <bsd.prog.mk>
