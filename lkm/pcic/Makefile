#       $Id: Makefile,v 1.6 1997/02/22 12:48:45 peter Exp $

.PATH:  ${.CURDIR}/../../sys/pccard
KMOD=   pcic_mod
SRCS=   pcic.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
