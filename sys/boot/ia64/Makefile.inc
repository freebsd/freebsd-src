# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.2 2003/02/01 22:50:08 marcel Exp $

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
