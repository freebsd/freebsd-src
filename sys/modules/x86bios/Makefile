# $FreeBSD$

KMOD=	x86bios
SRCS=	opt_x86bios.h

.PATH:	${.CURDIR}/../../contrib/x86emu
.PATH:	${.CURDIR}/../../compat/x86bios
SRCS+=	x86bios.c x86bios_alloc.c x86emu.c x86emu_util.c

.include <bsd.kmod.mk>
