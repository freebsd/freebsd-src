# $FreeBSD$

PROG=	${BOOT}.out
INTERNALPROG=
FILES=	${BOOT}
NOMAN=
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
