#       $Id: Makefile,v 1.4 1995/10/15 17:00:49 phk Exp $

.PATH:  ${.CURDIR}/../../sys/i386/i386
KMOD=   fpu
SRCS=   math_emulate.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
