# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.1.26.1 2008/11/25 02:59:29 kensmith Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib
