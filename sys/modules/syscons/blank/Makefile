#	$Id: Makefile,v 1.2 1995/02/23 19:45:19 phk Exp $

KMOD=	blank_saver_mod
SRCS= 	blank_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
