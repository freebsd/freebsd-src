#	$Id: Makefile,v 1.1 1995/02/22 13:47:10 sos Exp $

BINDIR=	/lkm
KMOD=	green_saver_mod
SRCS= 	green_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
