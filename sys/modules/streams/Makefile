# $FreeBSD: src/sys/modules/streams/Makefile,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $ 

.PATH: ${.CURDIR}/../../dev/streams

KMOD=	streams
SRCS=	streams.c

EXPORT_SYMS=	svr4_str_initialized	\
		svr4_stream_get		\
		svr4_delete_socket

.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif
CFLAGS+= -O

.include <bsd.kmod.mk>
