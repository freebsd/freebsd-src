#	$Id: Makefile,v 1.2 1995/02/23 19:45:28 phk Exp $

KMOD=	green_saver_mod
SRCS= 	green_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
