# $FreeBSD: src/sys/boot/i386/cdboot/Makefile,v 1.13.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x7c00

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
