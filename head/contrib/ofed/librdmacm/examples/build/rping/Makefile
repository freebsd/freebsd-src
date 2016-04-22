#
# $FreeBSD$
#
.PATH: ${.CURDIR}/../..
PROG=	rping
MAN=
SRCS=	rping.c
LIBADD+= ibverbs rdmacm pthread
LIBADD+= mlx4
LIBADD+= cxgb4

.include <bsd.prog.mk>
