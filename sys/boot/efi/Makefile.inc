# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.6 2002/12/10 04:20:15 marcel Exp $

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
