# $FreeBSD$

PROG=	${BOOT}
INTERNALPROG=
FILES=	${BOOT}
NOMAN=
SRCS=	${BOOT}.s
CLEANFILES= ${BOOT}

BOOT=	cdboot
ORG=	0x7c00

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S -Wl,--oformat -Wl,binary

.include <bsd.prog.mk>
