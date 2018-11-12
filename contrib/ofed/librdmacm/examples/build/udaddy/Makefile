#
# $FreeBSD$
#
.PATH: ${.CURDIR}/../..
PROG=	udaddy
MAN=
SRCS=	udaddy.c
LDADD+= -libverbs -lrdmacm -lpthread
LDADD+= -lmlx4

.include <bsd.prog.mk>
