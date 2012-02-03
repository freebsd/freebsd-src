# $FreeBSD$

PROG=	pmbr
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x600

AFLAGS+=--defsym FLAGS=${BOOT_MBR_FLAGS}
LDFLAGS=-e start -Ttext ${ORG} -Wl,-N,-S,--oformat,binary

.include <bsd.prog.mk>
