# $FreeBSD$

PROG=	mbr
STRIP=
BINMODE=${NOBINMODE}
NOMAN=
SRCS=	${PROG}.s

ORG=	0x600

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
