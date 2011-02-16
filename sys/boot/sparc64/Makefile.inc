# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.1.32.1 2010/12/21 17:10:29 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib
