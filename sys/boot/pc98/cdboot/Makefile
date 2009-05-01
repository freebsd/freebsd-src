# $FreeBSD: src/sys/boot/pc98/cdboot/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x0000

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
