# $FreeBSD$

.PATH:		${.CURDIR}/../../contrib/lib9p
CFLAGS+=	-DWITH_CASPER
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../contrib/lib9p

LIB=		9p
PACKAGE=	lib${LIB}
SHLIB_MAJOR=	1
SRCS=		connection.c \
		genacl.c \
		hashtable.c \
		log.c \
		pack.c \
		request.c \
		rfuncs.c \
		threadpool.c \
		utils.c \
		backend/fs.c \
		transport/socket.c

INCSDIR=	${INCLUDEDIR}/lib9p
INCS=		fid.h lib9p.h backend/fs.h

LIBADD=		sbuf

.include <bsd.lib.mk>
