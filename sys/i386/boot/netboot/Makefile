#	Makefile,v 1.4 1994/12/31 17:16:49 jkh Exp
#
# Makefile for NETBOOT
#
# Basic Options:
#	-DASK_BOOT	- Ask "Boot from Network (Y/N) ?" at startup
#	-DROMSIZE	- Size of EPROM - Must be set (even for .COM files)
#	-DRELOC		- Relocation address (usually 0x90000)
#
# NS8390 Options:
#	-DINCLUDE_WD	- Include Western Digital/SMC support
#	-DINCLUDE_NE	- Include NE1000/NE2000 support
#	-DINCLUDE_3COM  - Include 3c503 support
#       -D_3COM_USE_AUI - Disable transceiver on 3c503 by default
#	-DNE_BASE	- Base I/O address for NE1000/NE2000
#       -D_3COM_BASE    - Base I/O address for 3c503
#	-DWD_DEFAULT_MEM- Default memory location for WD/SMC cards
# XXX nothing depends on this Makefile so you must run `make clean' after
# changing an option.
#

### options for PCI cards
###
PCI_VENDOR=0x10ec
PCI_DEVICE=0x8029
PCI_CLASS=0x02,0x00,0x00

PROG=	nb8390.com nb3c509.com nb8390.rom nb3c509.rom
# Order is very important on the SRCS line for this prog
SRCS=	start2.S main.c misc.c bootmenu.c rpc.c

BINDIR=		/usr/mdec
BINMODE=	555
#CFLAGS=		-O2 -DNFS -DROMSIZE=${ROMSIZE} -DRELOC=${RELOCADDR} -DASK_BOOT
CFLAGS=		-O2 -DNFS -DROMSIZE=${ROMSIZE} -DRELOC=${RELOCADDR} # -DASK_BOOT
CFLAGS += -DPCI -DPCI_VENDOR=${PCI_VENDOR} -DPCI_DEVICE=${PCI_DEVICE}
CFLAGS += -DPCI_CLASS=${PCI_CLASS} -DASK_BOOT
#NS8390=		-DINCLUDE_WD -DWD_DEFAULT_MEM=0xD0000
NS8390=	-DINCLUDE_NE
#NS8390+=	-DINCLUDE_3COM -D_3COM_BASE=0x300
CLEANFILES+=	netboot.com
CLEANFILES+=	makerom start2.ro 3c509.o ns8390.o
LDFLAGS+=	-N -T ${RELOCADDR} -e _start -nostdlib
NOSHARED=	YES
MAN8=	netboot.8
STRIP=

ROMSIZE=16384
RELOCADDR=0x90000

.SUFFIXES:	.ro

.S.ro:
	${CC} ${CFLAGS} -DBOOTROM -o ${.TARGET} -c ${.IMPSRC}

ns8390.o:	ns8390.c
	${CC} ${CFLAGS} $(NS8390) -o ${.TARGET} -c $<

makerom: makerom.c
	${CC} ${CFLAGS} -o ${.TARGET} -DROMSIZE=${ROMSIZE} ${.CURDIR}/makerom.c

nb8390.rom:	makerom start2.ro ${SRCS:N*.h:R:S/$/.o/g} ns8390.o
	${LD} ${LDFLAGS} -o ${.TARGET} ${OBJS:S/start2.o/start2.ro/} ns8390.o
	strip ${.TARGET}
	size ${.TARGET}
	${.OBJDIR}/makerom ${.TARGET}

nb3c509.rom:	makerom start2.ro ${SRCS:N*.h:R:S/$/.o/g} 3c509.o
	${LD} ${LDFLAGS} -o ${.TARGET} ${OBJS:S/start2.o/start2.ro/} 3c509.o
	strip ${.TARGET}
	size ${.TARGET}
	${.OBJDIR}/makerom ${.TARGET}

nb8390.com:	makerom start2.ro ${SRCS:N*.h:R:S/$/.o/g} ns8390.o
	${LD} ${LDFLAGS} -o netboot.com ${OBJS} ns8390.o
	strip netboot.com
	size netboot.com
	dd ibs=32 skip=1 if=netboot.com of=${.TARGET}

nb3c509.com:	start2.o ${SRCS:N*.h:R:S/$/.o/g} 3c509.o
	${LD} ${LDFLAGS} -o netboot.com ${OBJS} 3c509.o
	strip netboot.com
	size netboot.com
	dd ibs=32 skip=1 if=netboot.com of=${.TARGET}


.include <bsd.prog.mk>
