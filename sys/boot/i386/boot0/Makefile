# $FreeBSD$

PROG=		boot0
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444

M4?=	m4

# The default set of flags compiled into boot0.  This enables update (writing
# the modified boot0 back to disk after running so that the selection made is
# saved), packet mode (detect and use the BIOS EDD extensions if we try to
# boot past the 1024 cylinder liimt), and booting from all valid slices.
BOOT_BOOT0_FLAGS?=	0xf

# The number of timer ticks to wait for a keypress before assuming the default
# selection.  Since there are 18.2 ticks per second, the default value of
# 0xb6 (182d) corresponds to 10 seconds.
BOOT_BOOT0_TICKS?=	0xb6

# The base address that we the boot0 code to to run it.  Don't change this
# unless you are glutton for punishment.
BOOT_BOOT0_ORG?=	0x600

boot0: boot0.o
	${LD} -N -e start -Ttext ${BOOT_BOOT0_ORG} -o boot0.out boot0.o
	objcopy -S -O binary boot0.out ${.TARGET}

boot0.o: boot0.s
	${AS} ${AFLAGS} --defsym FLAGS=${BOOT_BOOT0_FLAGS} \
		--defsym TICKS=${BOOT_BOOT0_TICKS} ${.IMPSRC} -o ${.TARGET}

boot0sio: boot0sio.o
	${LD} -N -e start -Ttext ${BOOT_BOOT0_ORG} -o boot0sio.out boot0sio.o
	objcopy -S -O binary boot0sio.out ${.TARGET}

boot0sio.o: boot0sio.s
	${AS} ${AFLAGS} --defsym FLAGS=${BOOT_BOOT0_FLAGS} \
		--defsym TICKS=${BOOT_BOOT0_TICKS} ${.IMPSRC} -o ${.TARGET}

CLEANFILES+= boot0.out boot0.o boot0sio.out boot0sio.o

.include <bsd.prog.mk>
