#	$Id: Makefile,v 1.1 1995/02/22 13:45:29 sos Exp $

BINDIR=	/lkm
KMOD=	blank_saver_mod
SRCS= 	blank_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
