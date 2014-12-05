#
# $FreeBSD$
#
.PATH: ${.CURDIR}/../..
PROG=	cmatose
MAN=
SRCS=	cmatose.c
LDADD+= -libverbs -lrdmacm -lpthread
LDADD+= -lmlx4

.include <bsd.prog.mk>
