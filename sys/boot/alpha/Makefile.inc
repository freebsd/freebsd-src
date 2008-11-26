# $FreeBSD: src/sys/boot/alpha/Makefile.inc,v 1.11.12.1 2008/10/02 02:57:24 kensmith Exp $
# Options used when building app-specific libalpha components

BINDIR?=		/boot

CFLAGS+=		-ffreestanding -mno-fp-regs -Os
LDFLAGS+=		-nostdlib
PRIMARY_LOAD_ADDRESS=	0x20000000	# "Region 1 start"
SECONDARY_LOAD_ADDRESS=	0x2000c000	# "Region 1 start" + 48k
HEAP_LIMIT=		0x20040000	# "Region 1 start" + 256k
LIBSTANDDIR=		${.CURDIR}/../../../../lib/libstand
LIBALPHA=		${.OBJDIR}/../libalpha/libalpha.a
