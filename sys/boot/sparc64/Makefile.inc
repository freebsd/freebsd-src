# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.2.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
