#	$Id$
#
# Makefile for NETBOOT
#
# Options:
#	-DASK_BOOT	- Ask "Boot from Network (Y/N) ?" at startup
#	-DSMALL_ROM	- Compile for 8K ROMS
#	-DROMSIZE	- Size of EPROM - Must be set (even for .COM files)
#	-DRELOC		- Relocation address (usually 0x90000)
#	-DINCLUDE_WD	- Include Western Digital/SMC support
#	-DINCLUDE_NE	- Include NE1000/NE2000 support
#	-DNE_BASE	- Base I/O address for NE1000/NE2000
#	-DWD_DEFAULT_MEM- Default memory location for WD/SMC cards
#

PROG=	netboot.com
# Order is very important on the SRCS line for this prog
SRCS=	start2.S main.c misc.c ether.c bootmenu.c rpc.c

BINDIR=		/usr/mdec
BINMODE=	555
CFLAGS=		-O2 -DNFS -DROMSIZE=${ROMSIZE} -DRELOC=${RELOCADDR}
CFLAGS+=	-DINCLUDE_WD -DWD_DEFAULT_MEM=0xD0000
#CFLAGS+=	-DINCLUDE_NE -DNE_BASE=0x320
CLEANFILES+=	netboot.com.nohdr netboot.com.strip
CLEANFILES+=	netboot.rom.nohdr netboot.rom.strip netboot.rom
CLEANFILES+=	makerom start2.ro
LDFLAGS+=	-N -T ${RELOCADDR} -e _start -nostdlib
NOSHARED=	YES
NOMAN=
STRIP=

ROMSIZE=16384
RELOCADDR=0x90000

.SUFFIXES:	.ro

.S.ro:
	${CC} ${CFLAGS} -DBOOTROM -o ${.TARGET} -c ${.IMPSRC}

makerom: makerom.c
	${CC} -o ${.TARGET} -DROMSIZE=${ROMSIZE} ${.CURDIR}/makerom.c

netboot.rom:	makerom start2.ro ${OBJS}
	${LD} ${LDFLAGS} -o ${.TARGET} ${OBJS:S/start2.o/start2.ro/}

netboot.rom.strip:	netboot.rom
	cp -p netboot.rom ${.TARGET}
	strip ${.TARGET}
	size ${.TARGET}

netboot.rom.nohdr:	netboot.rom.strip
	dd ibs=32 skip=1 if=netboot.rom.strip of=${.TARGET}
	${.OBJDIR}/makerom ${.TARGET}

netboot.com.strip:	netboot.com
	cp -p netboot.com ${.TARGET}
	strip ${.TARGET}
	size ${.TARGET}

netboot.com.nohdr:	netboot.com.strip
	dd ibs=32 skip=1 if=netboot.com.strip of=${.TARGET}

all:	netboot.com.nohdr netboot.rom.nohdr

install:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}\
		netboot.com.nohdr ${DESTDIR}${BINDIR}/netboot.com
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}\
		netboot.rom.nohdr ${DESTDIR}${BINDIR}/netboot.rom

.include <bsd.prog.mk>
