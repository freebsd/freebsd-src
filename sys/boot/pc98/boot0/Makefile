# $FreeBSD$

FILES=	${BOOT}
CLEANFILES= ${BOOT} ${BOOT}.out ${BOOT}.o

BOOT=	boot0

# The base address that we the boot0 code to to run it.  Don't change this
# unless you are glutton for punishment.
BOOT_BOOT0_ORG?=	0x0000

${BOOT}: ${BOOT}.out
	objcopy -S -O binary ${BOOT}.out ${.TARGET}

${BOOT}.out: ${BOOT}.o
	${LD} -N -e start -Ttext ${BOOT_BOOT0_ORG} -o ${.TARGET} ${BOOT}.o

.include <bsd.prog.mk>
