# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.1.28.1 2009/04/15 03:14:26 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib
