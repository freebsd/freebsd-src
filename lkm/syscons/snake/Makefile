#	$Id: Makefile,v 1.2 1995/02/23 19:45:29 phk Exp $

KMOD=	snake_saver_mod
SRCS= 	snake_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
