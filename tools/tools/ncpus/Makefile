# $FreeBSD: src/tools/tools/ncpus/Makefile,v 1.3.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

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
