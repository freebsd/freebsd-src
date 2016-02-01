LIB=		9p
SHLIB_MAJOR=	1
SRCS=		pack.c \
		connection.c \
		request.c log.c \
		hashtable.c \
		utils.c \
		transport/socket.c \
		backend/fs.c

INCS=		lib9p.h
CFLAGS=		-g -O0

LIBADD=		sbuf

.include <bsd.lib.mk>
