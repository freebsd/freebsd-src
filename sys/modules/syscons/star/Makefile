#	$Id: Makefile,v 1.1 1995/02/22 13:47:35 sos Exp $

BINDIR=	/lkm
KMOD=	star_saver_mod
SRCS= 	star_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
