# $FreeBSD: src/gnu/usr.bin/binutils/size/Makefile,v 1.4.2.1 2000/07/06 22:16:11 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		size
SRCS=		size.c
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
DPADD=		${RELTOP}/libbinutils/libbinutils.a
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a
LDADD=		${DPADD}

.include <bsd.prog.mk>
