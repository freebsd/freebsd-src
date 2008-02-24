# $FreeBSD: src/sys/boot/pc98/cdboot/Makefile,v 1.1 2006/05/14 07:26:02 nyan Exp $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x0000

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
