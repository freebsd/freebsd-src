# $NetBSD: Makefile,v 1.15 1998/03/28 00:21:35 thorpej Exp $
# $FreeBSD: src/sys/boot/alpha/boot1/Makefile,v 1.17 2004/02/11 22:01:17 ru Exp $

PROG=	boot1.sym
INTERNALPROG=
NOMAN=
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
