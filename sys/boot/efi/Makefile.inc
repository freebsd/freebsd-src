# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.8.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
