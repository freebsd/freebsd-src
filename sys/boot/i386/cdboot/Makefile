# $FreeBSD$

PROG=	${BOOT}.out
INTERNALPROG=
FILES=	${BOOT}
NOMAN=
SRCS=	${BOOT}.s
CLEANFILES= ${BOOT}

BOOT=	cdboot
ORG=	0x7c00

${BOOT}: ${BOOT}.out
	objcopy -S -O binary ${BOOT}.out ${.TARGET}

LDFLAGS=-N -e start -Ttext ${ORG}

.include <bsd.prog.mk>
