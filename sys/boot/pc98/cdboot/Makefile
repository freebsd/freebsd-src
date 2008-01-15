# $FreeBSD: src/sys/boot/pc98/cdboot/Makefile,v 1.1.2.1 2006/06/16 13:40:50 nyan Exp $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x0000

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
