# $FreeBSD$

PROG=	${BOOT}
INTERNALPROG=
FILES=	${BOOT}
NOMAN=
SRCS=	${BOOT}.s

BOOT=	mbr
ORG=	0x600

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
