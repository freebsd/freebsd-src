# $FreeBSD: src/sys/boot/i386/mbr/Makefile,v 1.13 2004/04/28 21:31:21 ru Exp $

PROG=	mbr
STRIP=
BINMODE=${NOBINMODE}
NOMAN=
SRCS=	${PROG}.s

ORG=	0x600

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
