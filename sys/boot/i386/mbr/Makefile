# $FreeBSD$

PROG=	${BOOT}.out
INTERNALPROG=
FILES=	${BOOT}
NOMAN=
SRCS=	${BOOT}.s
CLEANFILES= ${BOOT}

BOOT=	mbr
ORG=	0x600

${BOOT}: ${BOOT}.out
	objcopy -S -O binary ${BOOT}.out ${.TARGET}

LDFLAGS=-N -e start -Ttext ${ORG}

.include <bsd.prog.mk>
