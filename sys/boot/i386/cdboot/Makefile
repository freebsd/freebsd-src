# $FreeBSD$

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NOMAN=
SRCS=	${PROG}.s

ORG=	0x7c00

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
