#	Makefile,v 1.2 1995/02/23 19:45:30 phk Exp

BINDIR=	/lkm
KMOD=	star_saver_mod
SRCS= 	star_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

.include <bsd.kmod.mk>
