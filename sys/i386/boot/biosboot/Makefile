#	$Id: Makefile,v 1.23 1994/12/18 19:14:08 bde Exp $
#

PROG=	boot
# Order is very important on the SRCS line for this prog
SRCS=	start.S table.c boot2.S boot.c asm.S bios.S serial.S
SRCS+=  probe_keyboard.c io.c disk.c sys.c

BINDIR=		/usr/mdec
BINMODE=	444
CFLAGS=		-O2 -DDO_BAD144 -DBOOTWAIT=${BOOTWAIT} -DCOMCONSOLE=0x3F8
CFLAGS+=	-DBOOTSEG=${BOOTSEG} -DBOOTSTACK=${BOOTSTACK}
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../.. -DRB_SERIAL=0x1000
CLEANFILES+=	boot.nohdr boot.strip boot1 boot2
DPADD=		${LIBC}
LDFLAGS+=	-N -T 0 -nostdlib
LDADD=		-lc
LINKS=		${BINDIR}/sdboot ${BINDIR}/wdboot\
		${BINDIR}/sdboot ${BINDIR}/fdboot\
		${BINDIR}/bootsd ${BINDIR}/bootwd\
		${BINDIR}/bootsd ${BINDIR}/bootfd
NOSHARED=	YES
NOMAN=
STRIP=

# tunable timeout parameter, waiting for keypress, calibrated in mS
BOOTWAIT?=	5000

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

all:		boot1 boot2

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}\
		boot1 ${DESTDIR}${BINDIR}/sdboot
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}\
		boot2 ${DESTDIR}${BINDIR}/bootsd
	
.include <bsd.prog.mk>
