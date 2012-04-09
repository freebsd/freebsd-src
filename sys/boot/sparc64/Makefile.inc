# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.2.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
