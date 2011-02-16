# $FreeBSD: src/tools/tools/ncpus/Makefile,v 1.3.14.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	ncpus
NO_MAN=
SRCS=	ncpus.c acpi.c
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
SRCS+=	biosmptable.c
.endif
BINDIR=	/usr/local/bin
WARNS?=	6

DPADD=	${LIBDEVINFO}
LDADD=	-ldevinfo

.include <bsd.prog.mk>
