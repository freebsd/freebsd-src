#       $Id: Makefile,v 1.3 1995/10/15 14:33:17 phk Exp $

.PATH:  ${.CURDIR}/../../sys/pccard
KMOD=   pcic_mod
SRCS=   pcic.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM

.include <bsd.kmod.mk>
