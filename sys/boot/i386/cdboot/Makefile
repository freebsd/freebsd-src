# $FreeBSD: src/sys/boot/i386/cdboot/Makefile,v 1.12 2004/04/28 21:31:20 ru Exp $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NOMAN=
SRCS=	${PROG}.s

ORG=	0x7c00

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
