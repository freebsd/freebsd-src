# $NetBSD: Makefile,v 1.15 1998/03/28 00:21:35 thorpej Exp $
# $FreeBSD$

PROG=	boot1.sym
INTERNALPROG=
NO_MAN=
LINKS=	${BINDIR}/boot1 ${BINDIR}/boot

.PATH: ${.CURDIR}/../libalpha

SRCS=		start.S boot1.c
CFLAGS+=	-DSECONDARY_LOAD_ADDRESS=${SECONDARY_LOAD_ADDRESS} -DMINIMAL
CFLAGS+=	-DBOOT1
CFLAGS+=	-I${.CURDIR}/../../common

BOOT_RELOC = ${PRIMARY_LOAD_ADDRESS}

CLEANFILES= boot1

FILES=	boot1

boot1: boot1.sym
	objcopy -S -O binary boot1.sym ${.TARGET}

LDFLAGS=-Ttext ${BOOT_RELOC} -N -e start
DPADD=	${LIBALPHA} ${LIBSTAND}
LDADD=	${LIBALPHA} -lstand

.include <bsd.prog.mk>
