#       $FreeBSD$

.PATH:  ${.CURDIR}/../../sys/i386/i386
KMOD=   fpu
SRCS=   math_emulate.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
