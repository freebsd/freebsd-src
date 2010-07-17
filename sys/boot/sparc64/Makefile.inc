# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
