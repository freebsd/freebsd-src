# $FreeBSD$

FILES=	${BOOT}
CLEANFILES= ${BOOT} ${BOOT}.bin ${BOOT}.out ${OBJS}

BOOT=	boot0.5

SRCS=	start.s boot.s boot0.5.s disk.s selector.s support.s syscons.s
OBJS=	${SRCS:N*.h:R:S/$/.o/g}

# The base address that we the boot0 code to to run it.  Don't change this
# unless you are glutton for punishment.
BOOT_BOOT0_ORG?=	0x0000

# The size of boot0.5 must be 7168 bytes
${BOOT}: ${BOOT}.bin
	cat ${BOOT}.bin /dev/zero | dd of=${BOOT} bs=1 count=7168

${BOOT}.bin: ${BOOT}.out
	objcopy -S -O binary ${BOOT}.out ${.TARGET}

${BOOT}.out: ${OBJS}
	${LD} -N -e start -Ttext ${BOOT_BOOT0_ORG} -o ${.TARGET} ${.ALLSRC}

.include <bsd.prog.mk>
