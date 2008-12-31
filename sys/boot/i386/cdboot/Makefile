# $FreeBSD: src/sys/boot/i386/cdboot/Makefile,v 1.13.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x7c00

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
