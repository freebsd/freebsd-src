# $FreeBSD: src/gnu/usr.bin/binutils/ar/Makefile,v 1.4.2.1 2000/07/06 22:15:41 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		ar
SRCS=		ar.c not-ranlib.c
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
DPADD=		${RELTOP}/libbinutils/libbinutils.a
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a
LDADD=		${DPADD}

.include <bsd.prog.mk>
