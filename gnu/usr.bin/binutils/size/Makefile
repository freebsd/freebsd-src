# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROG=	size
SRCS=	size.c
WARNS=	2
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
DPADD:=	${RELTOP}/libbinutils/libbinutils.a \
	${RELTOP}/libbfd/libbfd.a \
	${RELTOP}/libiberty/libiberty.a \
	${DPADD}
LDADD=	${DPADD}

.include <bsd.prog.mk>
