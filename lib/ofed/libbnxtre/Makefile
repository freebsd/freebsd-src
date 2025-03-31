SPATH= ${SRCTOP}/contrib/ofed/libbnxtre
.PATH: ${SPATH}

SHLIBDIR?=	/lib
LIB=		bnxtre
SHLIB_MAJOR=	1

SRCS=		main.c db.c memory.c verbs.c

LIBADD=		ibverbs pthread
CFLAGS+=	-I${SPATH}

.include <bsd.lib.mk>
