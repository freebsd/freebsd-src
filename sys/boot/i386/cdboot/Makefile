# $FreeBSD$

PROG=	${BOOT}
INTERNALPROG=
FILES=	${BOOT}
NOMAN=
SRCS=	${BOOT}.s

BOOT=	cdboot
ORG=	0x7c00

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
