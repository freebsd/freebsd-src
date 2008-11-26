# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.1.24.1 2008/10/02 02:57:24 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib
