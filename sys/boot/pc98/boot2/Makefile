# $FreeBSD$
#

PROG=	boot
# Order is very important on the SRCS line for this prog
SRCS=	start.S table.c boot2.S boot.c asm.S bios.S serial.S
SRCS+=	probe_keyboard.c io.c disk.c sys.c

BINDIR=		/boot
BINMODE=	444
CFLAGS=		-elf -Os -mrtd \
		-ffreestanding -fno-builtin -fno-guess-branch-probability \
		-D_KERNEL -DPC98 -DBOOTWAIT=${BOOTWAIT} -DTIMEOUT=${TIMEOUT}
CFLAGS+=	-DBOOTSEG=${BOOTSEG} -DBOOTSTACK=${BOOTSTACK}
CFLAGS+=	${CWARNFLAGS}
CFLAGS+=	-I${.CURDIR}/../../.. -I.

# By default, if a serial port is going to be used as console, use COM1
# (aka /dev/ttyd0).
#BOOT_COMCONSOLE_PORT?=0x30
BOOT_COMCONSOLE_PORT?=0x238
BOOT_COMCONSOLE_CLK?=16
BOOT_COMCONSOLE_MODE=0x0c
CFLAGS+=	-DCOMCONSOLE=${BOOT_COMCONSOLE_PORT} \
		-DCOMCONSOLE_CLK=${BOOT_COMCONSOLE_CLK} \
		-DCOMCONSOLE_MODE=${BOOT_COMCONSOLE_MODE}

# feature not implemented
BOOT_COMCONSOLE_SPEED?=9600
CFLAGS+=	-DCOMSPEED=${BOOT_COMCONSOLE_SPEED}

# Enable code to take the default boot string from a fixed location on the
# disk.  See nextboot(8) and README.386BSD for more info.
#CFLAGS+=	-DNAMEBLOCK
#CFLAGS+=	-DNAMEBLOCK_WRITEBACK

# Bias the conversion from the BIOS drive number to the FreeBSD unit number
# for hard disks.  This may be useful for people booting in a mixed IDE/SCSI
# environment (set BOOT_HD_BIAS to the number of IDE drives).
#CFLAGS+=	-DBOOT_HD_BIAS=1
#
# Details: this only applies if BOOT_HD_BIAS > 0.  If the BIOS drive number
# for the boot drive is >= BOOT_HD_BIAS, then the boot drive is assumed to
# be SCSI and have unit number (BIOS_drive_number - BOOT_HD_BIAS).  E.g.,
# BOOT_HD_BIAS=1 makes BIOS drive 1 correspond to 1:da(0,a) instead of
# 1:wd(1,a).  If `da' is given explicitly, then the drive is assumed to be
# SCSI and have BIOS drive number (da_unit_number + BOOT_HD_BIAS).  E.g.,
# BOOT_HD_BIAS=1 makes da(0,a) correspond to 1:da(0,a) instead of 0:da(0,a).

CLEANFILES+=	boot.nohdr boot.strip boot1 boot2 sizetest
LDFLAGS+=	-N -Ttext 0 -nostdlib -e start
NOSHARED=	YES
NOMAN=
STRIP=

# tunable timeout parameter, waiting for keypress, calibrated in ms
BOOTWAIT?=	5000
# tunable timeout during string input, calibrated in ms
#TIMEOUT?=	30000

# Location that boot2 is loaded at
BOOTSEG=	0x1000

# Offset in BOOTSEG for the top of the stack, keep this 16 byte aligned
BOOTSTACK=	0xFFF0

boot.nohdr:	boot
	objcopy -S -O binary boot boot.nohdr
	ls -l boot.nohdr

boot1:		boot.nohdr
	dd if=boot.nohdr of=boot1 bs=512 count=1

boot2:		boot.nohdr
	dd if=boot.nohdr of=boot2 bs=512 skip=1
	@dd if=boot2 skip=14 of=sizetest 2> /dev/null
	@if [ -s sizetest ] ; then \
		echo "boot2 is too big" >&2 ; \
		rm boot2 ; \
		exit 2 ; \
	fi

all:		boot1 boot2

install:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		boot1 boot2 ${DESTDIR}${BINDIR}

# If it's not there, don't consider it a target
.if exists(${.CURDIR}/../../../i386/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

.endif

CLEANFILES+=	machine

.include <bsd.kern.mk>
.include <bsd.prog.mk>
