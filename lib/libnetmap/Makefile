#
# $FreeBSD$
#

.include <src.opts.mk>

PACKAGE=	lib${LIB}
LIB=		netmap
SRCS=		nmctx.c nmport.c \
		nmctx-pthreads.c nmreq.c
INCS=		libnetmap.h
#MAN=		libnetmap.3
CFLAGS+=	-I${SRCTOP}/sys/net -I${.CURDIR}
WARNS?=		2

.include <bsd.lib.mk>
