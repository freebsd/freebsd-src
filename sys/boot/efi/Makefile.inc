# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.8.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
