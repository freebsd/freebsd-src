#       $Id: Makefile,v 1.1 1995/03/20 19:25:49 wollman Exp $

.PATH:  ${.CURDIR}/../../sys/pccard
KMOD=   pcic_mod
SRCS=   pcic.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM -Dlkm_pcic=xxxinit

.include <bsd.kmod.mk>
