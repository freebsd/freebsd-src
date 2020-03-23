# $FreeBSD$

KYUA_LIB=	store
LIBADD=		kyua_model kyua_utils sqlite3

CFLAGS+=	-DKYUA_STOREDIR=\"${KYUA_STOREDIR}\"

SRCS=	dbtypes.cpp		\
	exceptions.cpp		\
	layout.cpp		\
	metadata.cpp		\
	migrate.cpp		\
	read_backend.cpp	\
	read_transaction.cpp	\
	write_backend.cpp	\
	write_transaction.cpp

.include <bsd.lib.mk>
