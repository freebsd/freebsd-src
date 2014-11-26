# $FreeBSD$

PROG=	writefile

SRCS=	writefile.c eav.c

MAN=

CFLAGS+=	-DMD5_SUPPORT
LIBADD+= 	bz2 md util

.include <bsd.prog.mk>
