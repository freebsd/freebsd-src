#	$Id: Makefile,v 1.1 1995/02/22 13:48:06 sos Exp $

BINDIR=	/lkm
KMOD=	snake_saver_mod
SRCS= 	snake_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
