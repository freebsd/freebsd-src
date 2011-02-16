# $FreeBSD: src/sys/boot/i386/pmbr/Makefile,v 1.1.6.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	pmbr
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x600

AFLAGS+=--defsym FLAGS=${BOOT_MBR_FLAGS}
LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
