#       $Id: Makefile,v 1.2 1995/08/25 09:49:20 phk Exp $

.PATH:  ${.CURDIR}/../../sys/pccard
KMOD=   pcic_mod
SRCS=   pcic.c
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM
EXPORT_SYMS=	_lkm_pcic

.include <bsd.kmod.mk>
