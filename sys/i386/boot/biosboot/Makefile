#	$Id: Makefile,v 1.33 1995/10/10 21:11:06 ache Exp $
#

PROG=	boot
# Order is very important on the SRCS line for this prog
SRCS=	start.S table.c boot2.S boot.c asm.S bios.S serial.S
SRCS+=  probe_keyboard.c io.c disk.c sys.c

BINDIR=		/usr/mdec
BINMODE=	444
CFLAGS=         -O2 \
		-DDO_BAD144 -DBOOTWAIT=${BOOTWAIT} -DCOMCONSOLE=0x3F8
CFLAGS+=	-DBOOTSEG=${BOOTSEG} -DBOOTSTACK=${BOOTSTACK}
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../..

# force usage of serial console instead of keyboard probing
#CFLAGS+=	-DFORCE_COMCONSOLE

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
	for i in sd fd wd od ; do \
		( cd ${DESTDIR}${BINDIR} ; \
		rm -f boot$${i} $${i}boot ; \
		ln -s boot1 $${i}boot ; \
		ln -s boot2 boot$${i} ; ) \
	done

.include <bsd.prog.mk>
