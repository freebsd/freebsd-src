# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/lib9p
CFLAGS+=-I${.CURDIR}/../../contrib/lib9p

LIB=		9p
SHLIB_MAJOR=	1
SRCS=		pack.c \
		connection.c \
		request.c \
		log.c \
		hashtable.c \
		utils.c \
		rfuncs.c \
		transport/socket.c \
		backend/fs.c

INCS=		lib9p.h

LIBADD=		sbuf

.include <bsd.lib.mk>
