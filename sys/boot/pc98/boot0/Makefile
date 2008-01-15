# $FreeBSD: src/sys/boot/pc98/boot0/Makefile,v 1.4 2004/12/21 08:47:16 ru Exp $

PROG=	${BOOT}.out
INTERNALPROG=
FILES=	${BOOT}
NO_MAN=
SRCS=	${BOOT}.s
CLEANFILES= ${BOOT}

BOOT=	boot0

# The base address that we the boot0 code to to run it.  Don't change this
# unless you are glutton for punishment.
BOOT_BOOT0_ORG?=	0x0000

LDFLAGS=-N -e start -Ttext ${BOOT_BOOT0_ORG}

${BOOT}: ${BOOT}.out
	objcopy -S -O binary ${BOOT}.out ${.TARGET}

.include <bsd.prog.mk>
