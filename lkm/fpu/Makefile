#       $Id: Makefile,v 1.3 1997/02/22 12:47:51 peter Exp $

.PATH:  ${.CURDIR}/../../sys/i386/i386
KMOD=   fpu_mod
SRCS=   math_emulate.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
