# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		ar
SRCS=		ar.c not-ranlib.c
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
NOSHARED?=	yes
DPADD=		${RELTOP}/libbinutils/libbinutils.a
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a
LDADD=		${DPADD}

.include <bsd.prog.mk>
