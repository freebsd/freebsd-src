#       $Id: Makefile,v 1.1.2.1 1998/03/06 05:26:35 jkh Exp $

.PATH:  ${.CURDIR}/../../sys/i386/i386
KMOD=   fpu_mod
SRCS=   math_emulate.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
