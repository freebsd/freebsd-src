#	$Id: Makefile,v 1.1 1995/02/22 13:46:32 sos Exp $

BINDIR=	/lkm
KMOD=	fade_saver_mod
SRCS= 	fade_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
