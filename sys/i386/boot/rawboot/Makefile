#	$Id: Makefile,v 1.1 1996/09/11 19:25:11 phk Exp $
#

PROG=	boot

# Order is very important on the SRCS line for this prog
SRCS=	start.S table.c boot2.S boot.c asm.S bios.S serial.S
SRCS+=  probe_keyboard.c io.c disk.c sys.c

.PATH:	${.CURDIR}/../biosboot

BINDIR=		/usr/mdec
BINMODE=	444
CFLAGS=         -O2 \
		-DRAWBOOT \
		-I${.CURDIR}/../biosboot \
		-DBOOTWAIT=${BOOTWAIT} -DTIMEOUT=${TIMEOUT}
CFLAGS+=	-DBOOTSEG=${BOOTSEG} -DBOOTSTACK=${BOOTSTACK}

# Probe the keyboard and use the serial console if the keyboard isn't found.
.if defined(BOOT_PROBE_KEYBOARD)
CFLAGS+=	-DPROBE_KEYBOARD
.endif

# Probe the keyboard lock and use the serial console if the keyboard is locked.
.if defined(BOOT_PROBE_KEYBOARD_LOCK)
CFLAGS+=	-DPROBE_KEYBOARD_LOCK
.endif

# Force use of the serial console.
.if defined(BOOT_FORCE_COMCONSOLE)
CFLAGS+=	-DFORCE_COMCONSOLE
.endif

# By default, if a serial port is going to be used as console, use COM1 
# (aka /dev/ttyd0).
BOOT_COMCONSOLE_PORT?=0x3F8  
CFLAGS+=        -DCOMCONSOLE=${BOOT_COMCONSOLE_PORT}

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
# BOOT_HD_BIAS=1 makes BIOS drive 1 correspond to 1:sd(0,a) instead of
# 1:wd(1,a).  If `sd' is given explicitly, then the drive is assumed to be
# SCSI and have BIOS drive number (sd_unit_number + BOOT_HD_BIAS).  E.g.,
# BOOT_HD_BIAS=1 makes sd(0,a) correspond to 1:sd(0,a) instead of 0:sd(0,a).

CLEANFILES+=	boot.nohdr boot.strip rawboot sizetest
DPADD=		${LIBC}
LDFLAGS+=	-N -T 0 -nostdlib
LDADD=		-lc
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

boot.strip:	boot
	cp -p boot boot.strip
	strip boot.strip
	size boot.strip

boot.nohdr:	boot.strip
	dd if=boot.strip of=boot.nohdr ibs=32 skip=1 obs=1024b
	ls -l boot.nohdr

rawboot:	boot.nohdr
	dd if=boot.nohdr of=rawboot bs=8k count=1 conv=sync

all:		rawboot 

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}\
		rawboot ${DESTDIR}${BINDIR}/rawboot

.include <bsd.prog.mk>
