#       $Id: Makefile,v 1.1 1995/08/24 09:06:24 phk Exp $

.PATH:  ${.CURDIR}/../../sys/pccard
KMOD=   pcic_mod
SRCS=   pcic.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
