#
# $FreeBSD$
#
.PATH: ${.CURDIR}/../..
PROG=	rping
MAN=
SRCS=	rping.c
LDADD+= -libverbs -lrdmacm -lpthread
LDADD+= -lmlx4

.include <bsd.prog.mk>
