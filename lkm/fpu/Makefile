#       $Id: Makefile,v 1.1 1995/12/14 08:26:02 phk Exp $

.PATH:  ${.CURDIR}/../../sys/i386/i386
KMOD=   fpu_mod
SRCS=   math_emulate.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
