# $FreeBSD: src/sys/boot/i386/mbr/Makefile,v 1.15.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	mbr
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

# MBR flags: 0x80 -- try packet interface (also known as EDD or LBA)
BOOT_MBR_FLAGS?=	0x80

ORG=	0x600

AFLAGS+=--defsym FLAGS=${BOOT_MBR_FLAGS}
LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
