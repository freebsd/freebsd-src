#       $Id$

.PATH:  ${.CURDIR}/../../sys/i386/i386
KMOD=   vm86_mod
SRCS=   vm86.c
NOMAN=
CFLAGS+= -I. -DLKM -DVM86_MODULE

.include <bsd.kmod.mk>
