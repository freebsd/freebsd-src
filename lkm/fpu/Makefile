#       $Id: Makefile,v 1.4 1997/07/20 08:46:27 bde Exp $

.PATH:  ${.CURDIR}/../../sys/i386/i386
KMOD=   fpu_mod
SRCS=   math_emulate.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
