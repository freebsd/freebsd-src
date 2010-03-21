# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.1.30.1 2010/02/10 00:26:20 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib
