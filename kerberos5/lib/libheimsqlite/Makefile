# $FreeBSD$

LIB=	heimsqlite
SRCS=	sqlite3.c
DPADD=	${LIBPTHREAD}
LDADD=	-lpthread
CFLAGS+=-I${KRB5DIR}/lib/sqlite

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/sqlite
