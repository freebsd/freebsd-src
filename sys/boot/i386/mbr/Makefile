# $FreeBSD$

PROG=	${BOOT}
INTERNALPROG=
FILES=	${BOOT}
NOMAN=
SRCS=	${BOOT}.s
CLEANFILES= ${BOOT}

BOOT=	mbr
ORG=	0x600

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S -Wl,--oformat -Wl,binary

.include <bsd.prog.mk>
