# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
