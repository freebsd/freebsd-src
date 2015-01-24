#
# $FreeBSD$
#
.PATH: ${.CURDIR}/../..
PROG=	mckey
MAN=
SRCS=	mckey.c
LDADD+= -libverbs -lrdmacm -lpthread
LDADD+= -lmlx4

.include <bsd.prog.mk>
