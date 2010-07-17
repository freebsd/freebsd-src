# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.8.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
