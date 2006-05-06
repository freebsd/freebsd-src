# $FreeBSD: src/tools/tools/ncpus/Makefile,v 1.1.2.2 2006/03/10 22:04:07 sam Exp $

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
