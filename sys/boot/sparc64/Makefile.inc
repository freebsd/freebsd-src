# $FreeBSD: src/sys/boot/sparc64/Makefile.inc,v 1.1 2004/02/09 14:17:02 ru Exp $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib
