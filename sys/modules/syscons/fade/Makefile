#	$Id: Makefile,v 1.2 1995/02/23 19:45:26 phk Exp $

KMOD=	fade_saver_mod
SRCS= 	fade_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
