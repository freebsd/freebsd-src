# $FreeBSD: src/sys/boot/i386/cdboot/Makefile,v 1.13.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x7c00

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
