# $FreeBSD$ 
MAINTAINER=	newton@atdot.dotat.org

.PATH: ${.CURDIR}/../../dev/streams

KMOD=	streams
SRCS=	streams.c opt_streams.h

EXPORT_SYMS=	svr4_str_initialized	\
		svr4_stream_get		\
		svr4_delete_socket

.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif
CFLAGS+= -O

.include <bsd.kmod.mk>
