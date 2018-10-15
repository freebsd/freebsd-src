# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/lib9p
CFLAGS+=-I${.CURDIR}/../../contrib/lib9p
CFLAGS+=-DL9P_DEBUG=L9P_DEBUG -DACE_DEBUG -g -O0

LIB=		9p
PACKAGE=	lib${LIB}
SHLIB_MAJOR=	1
SRCS=		pack.c \
		connection.c \
		request.c \
		genacl.c \
		log.c \
		hashtable.c \
		rfuncs.c \
		utils.c \
		threadpool.c \
		transport/socket.c \
		backend/fs.c

INCSDIR=	${INCLUDEDIR}/lib9p
INCS=		lib9p.h fid.h backend/fs.h

LIBADD=		sbuf

.include <bsd.lib.mk>
