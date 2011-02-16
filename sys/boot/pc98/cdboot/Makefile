# $FreeBSD: src/sys/boot/pc98/cdboot/Makefile,v 1.1.12.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x0000

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
