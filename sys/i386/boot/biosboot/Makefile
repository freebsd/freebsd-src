#	$Id: Makefile,v 1.38 1996/03/08 06:29:05 bde Exp $
#

PROG=	boot
# Order is very important on the SRCS line for this prog
SRCS=	start.S table.c boot2.S boot.c asm.S bios.S serial.S
SRCS+=  probe_keyboard.c io.c disk.c sys.c

BINDIR=		/usr/mdec
BINMODE=	444
CFLAGS=         -O2 \
		-DDO_BAD144 -DBOOTWAIT=${BOOTWAIT} -DTIMEOUT=${TIMEOUT}
CFLAGS+=	-DCOMCONSOLE=0x3F8
CFLAGS+=	-DBOOTSEG=${BOOTSEG} -DBOOTSTACK=${BOOTSTACK}

# Probe the keyboard and use the serial console if the keyboard isn't found.
#CFLAGS+=	-DPROBE_KEYBOARD

# Force use of the serial console (after probing the keyboard if
# PROBE_KEYBOARD is defined).
#CFLAGS+=	-DFORCE_COMCONSOLE

# Bias the conversion from the BIOS drive number to the FreeBSD unit number
# for hard disks to save typing.  E.g., BOOT_HD_BIAS=1 makes BIOS drive 1
# correspond to 1:sd(0,a) instead of 1:sd(1,a).
# This may be useful for people booting in a mixed IDE/SCSI environment
# (set BOOT_HD_BIAS to the number of IDE drives).
#CFLAGS+=	-DBOOT_HD_BIAS=1

CLEANFILES+=	boot.nohdr boot.strip boot1 boot2 sizetest
DPADD=		${LIBC}
LDFLAGS+=	-N -T 0 -nostdlib
LDADD=		-lc
#LINKS=		${BINDIR}/sdboot ${BINDIR}/wdboot\
#		${BINDIR}/sdboot ${BINDIR}/fdboot\
#		${BINDIR}/bootsd ${BINDIR}/bootwd\
#		${BINDIR}/bootsd ${BINDIR}/bootfd
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

boot1:		boot.nohdr
	dd if=boot.nohdr of=boot1 bs=512 count=1

boot2:		boot.nohdr
	dd if=boot.nohdr of=boot2 bs=512 skip=1
	@dd if=boot2 skip=14 of=sizetest 2> /dev/null
	@if [ -s sizetest ] ; then \
		echo "*** Boot2 is too BIG ***" ; exit 2 ; \
	fi

all:		boot1 boot2

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}\
		boot1 ${DESTDIR}${BINDIR}/boot1
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}\
		boot2 ${DESTDIR}${BINDIR}/boot2
	for i in sd fd wd od vn ; do \
		( cd ${DESTDIR}${BINDIR} ; \
		rm -f boot$${i} $${i}boot ; \
		ln -s boot1 $${i}boot ; \
		ln -s boot2 boot$${i} ; ) \
	done

.include <bsd.prog.mk>
