# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.8.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
