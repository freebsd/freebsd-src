# $FreeBSD: src/sys/boot/pc98/boot0.5/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	${BOOT}.out
INTERNALPROG=
FILES=	${BOOT}
NO_MAN=
SRCS=	start.s boot.s boot0.5.s disk.s selector.s support.s syscons.s \
	putssjis.s
CLEANFILES= ${BOOT} ${BOOT}.bin

BOOT=	boot0.5

# The base address that we the boot0 code to to run it.  Don't change this
# unless you are glutton for punishment.
BOOT_BOOT0_ORG?=	0x0000

LDFLAGS=-N -e start -Ttext ${BOOT_BOOT0_ORG} -Wl,-T,${.CURDIR}/ldscript

# The size of boot0.5 must be 7168 bytes
${BOOT}: ${BOOT}.bin
	cat ${BOOT}.bin /dev/zero | dd of=${BOOT} bs=1 count=7168

${BOOT}.bin: ${BOOT}.out
	objcopy -S -O binary ${BOOT}.out ${.TARGET}

.include <bsd.prog.mk>
